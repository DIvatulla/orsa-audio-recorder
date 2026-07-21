#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include "../include/clarg.h"
#include "../include/websockc.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#ifndef WF 
#define WF 0
#endif
#define NUM_THREADS 1

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define RECORD_FORMAT SND_PCM_FORMAT_S16_LE
#define MAIN_PCM_BUF_DURATION 30
#define SAMPLE_PER_READ 4096

static audio_device *recdev = NULL; 
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;

static audio_buffer *mb = NULL;
static audio_buffer *rb = NULL;

static lame_enc *lemp3 = NULL;
static lame_mp3_buf *lbmp3 = NULL;
static mpeg_dec *mdmp3 = NULL;

volatile sig_atomic_t ws_kill_flag = 0;
volatile sig_atomic_t ws_pending_send = 0;
volatile sig_atomic_t ws_pending_receive = 0;


static ws_proto_list protocols = NULL;
static struct lws_context_creation_info *info = NULL;
static struct lws_context *context = NULL;
static struct lws_client_connect_info *cc_info = NULL;

int record(audio_device *d, audio_buffer *mb, audio_buffer *rb);
void write_file(unsigned char *b, int size, char *filename);
void play_mp3(audio_device *d, mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb);

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

	//ALSA data intialization
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

	err = make_ab(&mb, recdev, am_secs, MAIN_PCM_BUF_DURATION);
	if (err < 0){
		return err;
	}
	
	err = make_ab(&rb, recdev, am_frames, SAMPLE_PER_READ);
	if (err < 0){
		return err;
	}

	err = make_enc(&lemp3, recdev, defaulteq);
	if (err < 0){
		return err;
	}

	err = make_lame_mp3_buf(&lbmp3, mb);
	if (err < 0){
		return err;
	}

	err = make_dec(&mdmp3);
	if (err < 0){
		return err;
	}

	//Websocket data intialization
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
	if (!context) {
		printf("lws init failed\n");
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

	if (!lws_client_connect_via_info(cc_info)) {
		fprintf(stderr, "lws_client_connect_via_info failed\n");
		lws_context_destroy(context);
		return -1;
	}

	free_clargs(cli_arguments);
	ws_buf.size = LWS_PRE + MAX_PAYLOAD;
	ws_buf.data = (unsigned char*)calloc(ws_buf.size, sizeof(char));
	ws_buf.wi = 0;

	for (;;){
		lws_service(context, 0);
		if (ws_pending_send){
			clear_ws_buf();
			ws_buf->wi = lbmp3->wi;
			memcpy(ws_buf.data[LWS_PRE], lbmp3->buf, lbmp3->wi);
			lws_callback_on_writable(wsi);
		}
		if (ws_pending_receive){
			lbmp3->wi = ws_buf->wi;
			memcpy(lbmp3->buf, ws_buf.data, ws_buf.wi);
			clear_ws_buf();
			decode_mp3_settings(mdmp3, lbmp3);
			play_mp3(mdmp3, lbmp3, rb);
		}
		if (ws_kill_flag){
			//lws_cancel_service(context);
			lws_context_destroy(context);
			break;
		}
	}

	free_as(settings);
	free_ad(recdev);
	free_ad(playdev);
	free_enc(lemp3);
	free_lame_mp3_buf(lbmp3);
	free_ab(mb);
	free_ab(rb);
	free_dec(mdmp3);
	free_proto_list(protocols);
	free_context_creation_info(info);
	free_client_connection_info(cc_info);

	return 0;
}

int record(audio_device *d, audio_buffer *mb, audio_buffer *rb)
{
	int ret;

	do {
		ret = snd_pcm_readi(d->handle, rb->buf, snd_pcm_bytes_to_frames(d->handle, rb->size));
		if (ret == -EPIPE){
			snd_pcm_prepare(d->handle);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi += snd_pcm_bytes_to_frames(d->handle, rb->size);
		push_ab(mb, rb);

		printf("rec is going; mb->wi = %d, (mb->size - rb->size) = %d\n", mb->wi, mb->size - rb->size);
	} while(mb->wi <= (mb->size - rb->size));

	return 0;
}

void write_file(unsigned char *b, int size, char *filename)
{
	FILE *f = fopen(filename, "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
	}
    fwrite(b, sizeof(char), size, f);
    fclose(f);

    printf("Saved\n");
}

void play_mp3(audio_device *d, mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb)
{
	int ret;
	size_t done;

	mpg123_open_feed(mdmp3->handle);
    mpg123_feed(mdmp3->handle, lb->buf, lb->size);
	
	snd_pcm_prepare(d->handle);
	while ((ret = mpg123_read(mdmp3->handle, rb->buf, rb->size, &done)) == MPG123_OK){
		snd_pcm_writei(d->handle, rb->buf, SAMPLE_PER_READ);
	}
	snd_pcm_drain(d->handle);
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
		ws_buf.wi += len;
		memcpy(ws_buf.data, in, len);
		printf("remaining packet size - %ld, lbmp3.wi - %d\n", lws_remaining_packet_payload(wsi), ws_buf->wi);
		if ((!lws_remaining_packet_payload(wsi)) && lws_is_final_fragment(wsi)){
			ws_pending_receive = 1;
		}
		break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
		printf("Sending\n");
		printf("ws_data.wi %d\n", lbmp3->wi);
		
		int sent = lws_write(wsi, &ws_buf.data[LWS_PRE], ws_buf.wi, LWS_WRITE_BINARY);
		if (sent < ws_buf.wi){
			lwsl_err("lws_write failed (%d)\n", sent);
			ws_thread_kill_flag = 1;
			return -1;
		}
		ws_pending_send = 0;
		lwsl_user("Sent %d bytes.\n", sent);
		break;
	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_user("Connection closed.\n");
		break;
	default:
		break;
    }
 
    return 0;
}