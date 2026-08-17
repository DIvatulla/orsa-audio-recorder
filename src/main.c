#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include "../include/clarg.h"
#include "../include/websockc.h"
#include "../include/resampler.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <speex/speex_resampler.h> 

#ifndef WF 
#define WF 0
#endif

#define LOCAL_SAMPLE_RATE 44100
#define LOCAL_CHANNELS 1
#define LOCAL_RECORD_FORMAT SND_PCM_FORMAT_FLOAT_LE

#define PER_READ_PCM_BUF_DURATION 30 //microseconds
#define MAIN_PCM_BUF_DURATION 60 //seconds

#define SERVER_SAMPLE_RATE 24000
#define SERVER_CHANNELS 1
#define SERVER_RECORD_FORMAT SND_PCM_FORMAT_FLOAT_LE

static char **cli_arguments;

static audio_device *recdev = NULL; 
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;

static volatile ws_queue *session_wsq = NULL;
static volatile ws_state wscs = WS_NONE;
const char endmsg[] = "END";
const char haltmsg[] = "HALT";
char msg[LWS_PRE+4] = {};

static ws_proto_list protocols = NULL;
static struct lws_context_creation_info *info = NULL;
static struct lws_context *context = NULL;
static struct lws_client_connect_info *cc_info = NULL;
static struct lws *client_wsi;

static int ws_conn_established_flag = 0;
static int ws_conn_writable_flag = 0;

void write_file(unsigned char *b, int size, char *filename);

int ws_loop(audio_device *recdev, audio_device *playdev);
int ws_callback(
    struct lws *wsi, 
    enum lws_callback_reasons reason, 
    void *user, 
    void *in, 
    size_t len
);
int push_session_ws_queue(audio_buffer *ab);
int pop_session_ws_queue(audio_buffer **ab);

int main(int argc, char** argv)
{   
	pthread_t wst; //websocket_thread
	int err = 0;
	
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
	
	//Websocket data initialization
	err = make_proto_list(
		&protocols, 
		"\0", 
		&ws_callback, 
		LWS_PRE + 2, 
		MAX_PAYLOAD / 10
	);
	if (err < 0){
		fprintf(stderr, "ws_thread: can't create proto list\n");
		pthread_exit(NULL);
	}

	err = make_context_creation_info(&info, protocols);
	if (err < 0){
		fprintf(stderr, "ws_thread: can't create context creation info\n");
		pthread_exit(NULL);
	}

	context = lws_create_context(info);
	if (!context){
		printf("ws_thread: can't create context\n");
		pthread_exit(NULL);
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
		printf("ws_thread: can't create client connection info\n");
		pthread_exit(NULL);
	}
	
	client_wsi = lws_client_connect_via_info(cc_info);
	if (!client_wsi){
		fprintf(stderr, "ws_thread: lws_client_connect_via_info failed\n");
		lws_context_destroy(context);
		pthread_exit(NULL);
	}

	wscs = WS_START;
	while (!ws_conn_is_ready()){
		lws_service(context, 0);
	}
	wscs = WS_SEND;
	make_ws_queue(&session_wsq);
	ws_loop();

	free_clargs(cli_arguments);
	free_as(settings);
	free_ad(recdev);
	free_ad(playdev);
	snd_config_update_free_global();
	free_proto_list(protocols);
	free_context_creation_info(info);
	free_client_connection_info(cc_info);

	return 0;
}

void ws_loop()
{
	audio_buffer *rb = NULL;
	int i, ret, err, brdr, bpr, spr; //bpr - bytes per read, spr - sample per read
	brdr = pcm_byte_per_second(
		get_rate_ad(recdev), 
		get_chan_ad(recdev), 
		get_pfmt_ad(recdev)) * MAIN_PCM_BUF_DURATION;
	bpr = pcm_byte_per_second(
		get_rate_ad(recdev),
		get_chan_ad(recdev), 
		get_pfmt_ad(recdev)) * PER_READ_PCM_BUF_DURATION / 1000;
	spr = bpr / pcm_byte_per_frame(get_chan_ad(recdev), get_pfmt_ad(recdev));
	
	while (wscs != WS_KILL)
	{
		switch(wscs){
			case WS_SEND:
				for (i = 0; i <= brdr; i += bpr){
					rb = NULL;
					rb = rec_ad(recdev, spr);
					if (!rb){
						return -1;
					}
					convert_pcm_buf(&rb, 16000);
					push_session_ws_queue(rb);
					
					if (session_wsq->item_count == 8){
						lws_callback_on_writable(client_wsi);
					}
				}
				wscs = WS_SEND_END;
				break;
			case WS_SEND_END:
				lws_callback_on_writable(client_wsi);
				break;
			
		}
	}
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
		wscs = WS_KILL;
		break;	
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		ws_conn_established_flag = 1;
		lwsl_user("Connected to server.\n");
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:

		break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
		if (LWS_CALLBACK_CLIENT_WRITEABLE_handler() < 0){
			wscs = WS_KILL;
		}
		break;
	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_user("Connection closed.\n");
		wscs = WS_KILL;
		break;
	default:
		break;
    }
 
    return 0;
}

int LWS_CALLBACK_CLIENT_WRITEABLE_handler()
{
	int err = 0;

	switch(wscs){
	case WS_START:
		ws_conn_writable_flag = 1;
		break;
	case WS_SEND:
		err = send_n_ws_queue(context, session_wsq, 8);
		if (err < 0){
			return err;
		}
		break;
	case WS_SEND_END:
		memcpy(&msg[LWS_PRE], endmsg, strlen(endmsg));
		lws_write(wsi, &msg[LWS_PRE], strlen(endmsg), LWS_WRITE_TEXT);
		break;
	default:
		return -1;
	}

	return 0;
}

int LWS_CALLBACK_CLIENT_RECEIVE_handler(void *in, size_t len)
{
	int err = 0;

	switch(wscs){
	case WS_RECV:
		if ((len == 4) && (strcmp(endmsg, in))){
			wscs = WS_RECV_END;
			break;
		}

		push_ws_queue(in_wsq, in, len);
		break;
	case WS_SEND_END:
		memcpy(&msg[LWS_PRE], endmsg, strlen(endmsg));
		lws_write(wsi, &msg[LWS_PRE], strlen(endmsg), LWS_WRITE_TEXT);
		break;
	default:
		return -1;
	}

	return 0;
}

int ws_conn_is_ready()
{
	return ws_conn_established_flag && ws_conn_writable_flag;
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

int push_audio_ws_queue(audio_buffer *ab)
{
	int err = 0;
	ws_queue_item *new = NULL;

	err = make_ws_queue_item(&new, ab->buf, ab->size);
	if (err < 0){
		return err;
	}

	push_ws_queue(session_wsq, &new);
	free(ab);

	return 0;
}

int pop_audio_ws_queue(audio_buffer **ab)
{
	int err = 0;
	ws_queue_item *last = NULL;
	audio_buffer *ab = NULL;

	ab = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (!ab){
		return -1;
		fprintf(stderr, "pop_session_ws_queue: can't malloc audio buffer\n");
	}

	pop_ws_queue(session_wsq, &last);
	printf("pop_session_ws_queue: new->size-%d\n", new->size);	
	
	ab->buf = last->data;
	ab->size = last->size;
	ab->wi = 0;
	ab->ri = 0;
	ab->sample_rate = SERVER_SAMPLE_RATE;
	ab->channels = SERVER_CHANNELS;
	ab->pcm_format = SERVER_RECORD_FORMAT;
	free(new);

	printf("pop_session_ws_queue: ab->size-%d\n", (*ab)->size);	
	
	return 0;
}