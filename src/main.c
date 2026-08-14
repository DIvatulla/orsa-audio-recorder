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
#define REC_PCM_BUF_DURATION 10 //microseconds
#define MAIN_PCM_BUF_DURATION 10000 //REC_PCM_BUF_DURATION * 1000 * 10sec

#define SERVER_SAMPLE_RATE 24000
#define SERVER_CHANNELS 1
#define SERVER_RECORD_FORMAT SND_PCM_FORMAT_FLOAT_LE

static char **cli_arguments;
static pthread_t thr[NUM_THREADS];

static audio_device *recdev = NULL; 
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;

static ws_queue *session_wsq = NULL;
static volatile ws_state wscs = WS_NONE;
const char endmsg[] = "END";
const char haltmsg[] = "HALT";

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

static pthread_t thr[2];

int main(int argc, char** argv)
{   
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

int push_session_ws_queue(audio_buffer *ab)
{
	int err = 0;
	ws_queue_item *new = NULL;

	err = make_ws_queue_item(&new, ab->buf, ab->size);
	if (err < 0){
		return err;
	}

	push_ws_queue(session_wsq, &new);

	return 0;
}

int pop_session_ws_queue(audio_buffer **ab)
{
	int err = 0;
	ws_queue_item *new = NULL;
	audio_buffer *ab = NULL;

	ab = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (!ab){
		return -1;
		fprintf(stderr, "pop_session_ws_queue: can't malloc audio buffer\n");
	}

	pop_ws_queue(session_wsq, &new);
	printf("pop_session_ws_queue: new->size-%d\n", new->size);	
	copy_queue_to_ab(new, *ab);
	printf("pop_session_ws_queue: ab->size-%d\n", (*ab)->size);	
	free(new);

	return 0;
}

void copy_queue_to_ab(ws_queue_item *wsqi, audio_buffer *ab)
{
	ab->buf = wsqi->data;
	ab->size = wsqi->size;
	ab->wi = 0;
	ab->ri = 0;
	ab->sample_rate = SERVER_SAMPLE_RATE;
	ab->channels = SERVER_CHANNELS;
	ab->pcm_format = SERVER_RECORD_FORMAT;
}

void *ws_thread_cleanup()
{

}

void *ws_thread(void *arg)
{
	int err;

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

	while (!ws_conn_is_ready()){
		lws_service(context, 0);
	}

	make_ws_queue();

	while(wscs != WS_KILL){
		switch (wscs){
		case WS_SEND:
			
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
		break;	
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		ws_conn_established_flag = 1;
		lwsl_user("Connected to server.\n");
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
		break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
		if (!ws_conn_writable_flag){
			ws_conn_writable_flag = 1;
			break
		}

		if (session_wsq->item_count == 24){
			
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

int ws_loop(lws_context *context)
{
	wscs = WS_SEND;

	while (wscs != WS_KILL)
	{
		switch(wscs){
			case WS_SEND:
				break;
		}
	}
}

int ws_conn_is_ready()
{
	return ws_conn_established_flag && ws_conn_writable_flag;
}
