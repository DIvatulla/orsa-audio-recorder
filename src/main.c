#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include "../include/clarg.h"
#include "../include/websockc.h"
#include "../include/resampler.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>   /* FIX: strcmp/memcmp/memcpy were used without this header */
#include <math.h>
#include <unistd.h>
#include <speex/speex_resampler.h>

#ifndef WF
#define WF 0
#endif
#define NUM_THREADS 1

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define RECORD_FORMAT SND_PCM_FORMAT_FLOAT_LE
#define MAIN_PCM_BUF_DURATION 15
#define SAMPLE_PER_READ 1760

/* Server-side stream markers / rates.  Pull these out of magic numbers so the
 * data-path math in play() is self-documenting and can't drift. */
#define RECV_RATE   24000                     /* rate the server sends audio at */
#define PLAY_RATE   SAMPLE_RATE               /* rate the playback device runs at */
#define RECORD_SEND_RATE 16000                /* rate we downsample mic audio to */
#define BYTES_PER_SAMPLE (sizeof_pcm_format(SND_PCM_FORMAT_FLOAT_LE))

const char endmsg[] = "END";

static audio_device *recdev = NULL;
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;

static volatile ws_state wscs = WS_NONE;
static ws_queue *in_wsq = NULL;
static ws_queue *out_wsq = NULL;

static ws_proto_list protocols = NULL;
static struct lws_context_creation_info *info = NULL;
static struct lws_context *context = NULL;
static struct lws_client_connect_info *cc_info = NULL;
static struct lws *client_wsi = NULL;

int push_ws_queue_out(ws_queue *wsq, char *from, int from_size);

int ws_loop(audio_device *recdev, audio_device *playdev);
int ws_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
);


int main(int argc, char** argv)
{
    int err = 0;
    char **cli_arguments;

    cli_arguments = parse_clargs(argc, argv);
    if (cli_arguments == NULL){
        return -1;
    }

    /* ALSA data initialization */
    err = make_as(&settings, SAMPLE_RATE, CHANNELS, RECORD_FORMAT);
    if (err < 0){
        return err;
    }

    err = make_ad(
        &recdev,
        (cli_arguments[INPUT_DEV] ? cli_arguments[INPUT_DEV] : "plughw:0"),
        settings,
        SND_PCM_STREAM_CAPTURE
    );
    if (err < 0){
        return err;
    }

    err = make_ad(
        &playdev,
        (cli_arguments[OUTPUT_DEV] ? cli_arguments[OUTPUT_DEV] : "plughw:0"),
        settings,
        SND_PCM_STREAM_PLAYBACK
    );
    if (err < 0){
        return err;
    }

    /* Websocket data initialization */
    err = make_proto_list(
        &protocols,
        "\0",
        &ws_callback,
        LWS_PRE + 2,
        MAX_PAYLOAD / 10
    );
    if (err < 0){
        return err;
    }

    err = make_context_creation_info(&info, protocols);
    if (err < 0){
        return err;
    }

    context = lws_create_context(info);
    if (!context){
        /* FIX: this used to print and then keep going, dereferencing a null
         * context in make_client_connection_info. Bail out instead. */
        printf("lws init failed\n");
        return -1;
    }

    err = make_client_connection_info(
        &cc_info,
        context,
        protocols,
        (cli_arguments[WS_HOST] ? cli_arguments[WS_HOST] : "127.0.0.1"),
        (cli_arguments[WS_PORT] ? atoi(cli_arguments[WS_PORT]) : 9000),
        (cli_arguments[WS_PATH] ? cli_arguments[WS_PATH] : "/"),
        "\0",
        "\0"
    );
    if (err < 0){
        return err;
    }

    client_wsi = lws_client_connect_via_info(cc_info);
    if (!client_wsi) {
        fprintf(stderr, "lws_client_connect_via_info failed\n");
        lws_context_destroy(context);
        return -16;
    }

    err = make_ws_queue(&in_wsq, 655360);
    if (err < 0){
        printf("can't create in wsq\n");
        return -1;
    }
    err = make_ws_queue(
        &out_wsq,
        LWS_PRE +
        (sizeof_pcm_format(RECORD_FORMAT) * CHANNELS * SAMPLE_PER_READ)
    );
    if (err < 0){
        printf("can't create out wsq\n");
        return -1;
    }

    out_wsq->wi = LWS_PRE;

    for (int i = 0; i < 3; i++){
        lws_service(context, 0);
        lws_callback_on_writable(client_wsi);
    }
    lws_service(context, 0);

    ws_loop(recdev, playdev);

    free_clargs(cli_arguments);
    free_as(settings);
    free_ad(recdev);
    free_ad(playdev);
    free_proto_list(protocols);
    free_context_creation_info(info);
    free_client_connection_info(cc_info);
    free_ws_queue(in_wsq);
    free_ws_queue(out_wsq);
    snd_config_update_free_global();

    return 0;
}

int rec(audio_device *d)
{
    audio_buffer *rb = NULL;
    int ret, err;

    err = make_ab(
        &rb,
        get_rate_ad(d),
        get_chan_ad(d),
        get_pfmt_ad(d),
        am_sample,
        SAMPLE_PER_READ
    );
    if (err < 0){
        return err;
    }

    ret = snd_pcm_readi(d->handle, rb->buf, snd_pcm_bytes_to_frames(d->handle, rb->size));
    if (ret == -EPIPE){
        snd_pcm_prepare(d->handle);
        free_ab(rb);          /* FIX: return without leaking rb on overrun */
        return -EPIPE;
    }
    else if (ret < 0){
        fprintf(stderr, "Error while recording\n");
        free_ab(rb);
        return -1;
    }

    convert_pcm_buf(&rb, RECORD_SEND_RATE);
    printf("REC rb->size %d\n", rb->size);

    err = push_ws_queue(out_wsq, rb->buf, rb->size);
    if (err < 0){
        free_ab(rb);          /* FIX: don't leak rb on push failure */
        return err;
    }

    free_ab(rb);
    printf("rec is going; out_wsq->wi = %d\n", out_wsq->wi);

    return 0;
}

int play(audio_device *d)
{
    int err;
    audio_buffer *rb = NULL;

    /* Nothing buffered -> nothing to play. */
    if (in_wsq->wi <= 0){
        return 0;
    }

    /* FIX: sample count is bytes / bytes-per-sample, NOT bytes / sample-rate. */
    int sample_count = in_wsq->wi / BYTES_PER_SAMPLE;

    err = make_ab(
        &rb,
        RECV_RATE,
        1,
        SND_PCM_FORMAT_FLOAT_LE,
        am_sample,
        sample_count
    );
    if (err < 0){          /* FIX: make_ab result was ignored -> null deref risk */
        return err;
    }

    /* FIX: the received PCM was never copied into the buffer being played.
     * Move it from in_wsq into rb before resampling. */
    int copy_bytes = sample_count * BYTES_PER_SAMPLE;
    if (copy_bytes > rb->size){
        copy_bytes = rb->size;
    }
    memcpy(rb->buf, in_wsq->buf, copy_bytes);

    convert_pcm_buf(&rb, PLAY_RATE);
    printf("PLAY rb->size %d\n", rb->size);

    snd_pcm_writei(d->handle, rb->buf, get_cur_sample_size_ab(rb));
    free_ab(rb);

    return 0;              /* FIX: function is declared int but never returned */
}

int ws_loop(audio_device *recdev, audio_device *playdev)
{
    int rec_limit = sizeof_pcm_format(RECORD_FORMAT) *
        CHANNELS *
        RECORD_SEND_RATE *
        MAIN_PCM_BUF_DURATION;
    int count = 0;
    int old_out_wsq_wi = out_wsq->wi;

    wscs = WS_SEND;
    for (;;){
        lws_service(context, 0);

        switch(wscs){
            case WS_SEND:
                old_out_wsq_wi = out_wsq->wi;
                rec(recdev);
                count += out_wsq->wi - old_out_wsq_wi;
                if (count > rec_limit){
                    count = 0;              /* FIX: reset so the next utterance measures fresh */
                    wscs = WS_END;
                }
                lws_callback_on_writable(client_wsi);
                break;
            case WS_RECV:
                break;
            case WS_PLAY:
                play(playdev);
                clear_ws_queue(in_wsq);
                wscs = WS_SEND;             
                break;
            case WS_KILL:
                goto stop;
                break;
            case WS_NONE:
                break;
        }
    }
    stop:
        return 0;
}

int ws_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
){
    switch(reason){
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
        break;
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        lwsl_user("Connected to server.\n");
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
        if (len == 3 && memcmp(in, endmsg, 3) == 0){
            wscs = WS_PLAY;
            break;
        }

        push_ws_queue(in_wsq, in, len);
        printf("remaining packet size - %ld, in_wsq.wi - %d\n",
               lws_remaining_packet_payload(wsi), in_wsq->wi);
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (wscs == WS_END){
            clear_ws_queue(out_wsq);
            push_ws_queue_out(out_wsq, (char*)endmsg, 3);
            lws_write(wsi, &out_wsq->buf[LWS_PRE], out_wsq->wi, LWS_WRITE_BINARY);
            wscs = WS_RECV;
            break;
        }
        if (wscs != WS_SEND){
            clear_ws_queue(out_wsq);
            break;
        }

        printf("Sending\n");
        printf("out_wsd.wi %d\n", out_wsq->wi);

        int sent = lws_write(wsi, &out_wsq->buf[LWS_PRE], out_wsq->wi, LWS_WRITE_BINARY);
        if (sent < out_wsq->wi){
            lwsl_err("lws_write failed (%d)\n", sent);
            wscs = WS_KILL;
            break;
        }

        lwsl_user("Sent %d bytes.\n", sent);
        clear_ws_queue(out_wsq);
        break;
    case LWS_CALLBACK_CLIENT_CLOSED:
        lwsl_user("Connection closed.\n");
        wscs = WS_KILL;
        return 0;
    default:
        break;
    }

    return 0;
}

int push_ws_queue_out(ws_queue *wsq, char *from, int from_size)
{
    if ((wsq->wi + from_size) >= wsq->cap){
        return -1;
    }
    if ((wsq->wi + from_size) >= wsq->size){
        wsq->buf = (char*)realloc(wsq->buf, (wsq->wi + from_size + 1) * sizeof(char));
        if (wsq->buf == NULL){
            fprintf(stderr, "can't realloc queue while push\n");
            return -1;
        }
    }
    memcpy(&wsq->buf[LWS_PRE + wsq->wi], from, from_size);
    wsq->wi += from_size;

    return 0;
}

