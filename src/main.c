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

static const unsigned int srate = 44100; //sample rate
static const int ch = 1;
static const snd_pcm_format_t record_form = SND_PCM_FORMAT_S16_LE;
static const int mb_dur = 30;
static const int spr = 4096; //samples per read

static audio_device *recdev = NULL; 
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;

static lame_enc *lemp3;
static lame_mp3_buf *lbmp3;
static mpeg_dec *mdmp3;

volatile sig_atomic_t ws_thread_kill_flag = 0;
volatile sig_atomic_t ws_pending_send = 0;
static int ws_thread_kill_result = 0;

static ws_proto_list protocols = NULL;
static struct lws_context_creation_info *info = NULL;
static struct lws_context *context = NULL;
static struct lws_client_connect_info *cc_info = NULL;

int record(audio_buffer *mb, audio_buffer *rb);
void write_file(unsigned char *b, int size, char *filename);
void play_mp3(mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb);

/*int ws_callback(
    struct lws *wsi, 
    enum lws_callback_reasons reason, 
    void *user, 
    void *in, 
    size_t len
);*/

int ws_callback(
    struct lws *wsi, 
    enum lws_callback_reasons reason, 
    void *user, 
    void *in, 
    size_t len
){
	return 1;
}

int main(int argc, char** argv)
{   
	int err = 0;
	int mp3_filesize;
	audio_buffer *mb;
	audio_buffer *rb;
	char **cli_arguments;

	cli_arguments = parse_clargs(argc, argv);
	if (cli_arguments == NULL){
		return -1;
	}

	make_proto_list(&protocols, "\0", &ws_callback, 2048);
	make_context_creation_info(&info, protocols);
	context = lws_create_context(info);
	if (!context) {
		printf("lws init failed\n");
	}
	make_client_connection_info(
		&cc_info, 
		context, 
		protocols,
		(cli_arguments[WS_HOST] ? cli_arguments[WS_HOST] : "127.0.0.1"),
		(cli_arguments[WS_PORT] ? cli_arguments[WS_PORT] : "9000"),
		(cli_arguments[WS_PATH] ? cli_arguments[WS_PATH] : "/ws/connection"),
		"\0",
		"\0"
	);

	free_proto_list(protocols);
	free_context_creation_info(info);
	lws_context_destroy(context);
	free_client_connection_info(cc_info);
	
	exit(0);

	err = make_as(&settings, srate, ch, record_form);
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

	err = make_enc(&lemp3, recdev, defaulteq);
	if (err < 0){
		return err;
	}

	err = make_ab(&mb, recdev, am_secs, mb_dur);
	if (err < 0){
		return err;
	}

	err = make_ab(&rb, recdev, am_frames, spr);
	if (err < 0){
		return err;
	}

	err = make_lame_mp3_buf(&lbmp3, mb);
	if (err < 0){
		return -1;
	}

	err = make_dec(&mdmp3);
	if (err < 0){
		return -1;
	}
	
	/*mb->wi = 0;
	record(mb, rb);*/
	//encode(lemp3, lbmp3, recdev, mb, &mp3_filesize);
	/*
	free_ab(mb);
	free_ad(recdev);
	free_as(settings);
	free_enc(lemp3);
	

	
	decode_mp3_settings(mdmp3, lbmp3);
	play_mp3(mdmp3, lbmp3, rb);
	free_ab(rb);
	*/
	#if WF
	write_file(lbmp3->buf, mp3_filesize, "./out.mp3");
	#endif
	/*
	free_lame_mp3_buf(lbmp3);
	free_ad(playdev);
	free_dec(mdmp3);
	free_clargs(cli_arguments);
	*/

	return 0;
}

int record(audio_buffer *mb, audio_buffer *rb)
{
	int ret;

	do {
		ret = snd_pcm_readi(recdev->handle, rb->buf, snd_pcm_bytes_to_frames(recdev->handle, rb->size));
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->handle);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi += snd_pcm_bytes_to_frames(recdev->handle, rb->size);
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

void play_mp3(mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb)
{
	int ret;
	size_t done;

	mpg123_open_feed(mdmp3->handle);
    mpg123_feed(mdmp3->handle, lb->buf, lb->size);
	
	snd_pcm_prepare(playdev->handle);
	while ((ret = mpg123_read(mdmp3->handle, rb->buf, rb->size, &done)) == MPG123_OK){
		snd_pcm_writei(playdev->handle, rb->buf, 4096);
	}
	snd_pcm_drain(playdev->handle);
}

//Websocket

/*
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch(reason){
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char*)in : "(null)");
            break;
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("Connected to server\n");
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (d->pending_send){
                int sent = lws_write(
                    wsi, 
                    &d->send_buf[LWS_PRE], 
                    d->send_len,
                    LWS_WRITE_TEXT
                );

                if (sent < (int)d->send_len){
                    lwsl_err("lws_write failed (%d)\n", sent);
                    interrupted = 1;
                    return -1;
                }

                d->pending_send = 0;
                lwsl_user("Sent %d bytes.\n", sent);
            }
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            lwsl_user("Connection closed.\n");
            interrupted = 1;
            break;
        default:
            break;
    }

    return 0;
}

static void sigint_handler(int sig){
	ws_kill_flag = 1;
	if (context){
		lws_cancel_service(context);
	} 
}

void *websocket_thread(void *arg)
{
	BLOCK_SIGNAL(SIGINT);
	int err;
	char **cli_arguments = (char**)arg;
	
	err = make_proto_list(
		&protocols, 
		ws_protocol_name, 
		&ws_callback, 
		MAX_PAYLOAD / 10
	);
	if (err < 0){
		exit(-1);
	}

	err = make_lws_info(&info, protocols);
	if (err < 0){
		exit(-2);
	}

	context = lws_create_context(info);
	if (context == NULL){
		exit(-3);
	}

	err = make_cc_info(cc_info);
	if (err < 0){

	}
}
*/