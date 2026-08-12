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

#define NUM_THREADS 4

#ifndef WF 
#define WF 0
#endif
#define NUM_THREADS 1

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define RECORD_FORMAT SND_PCM_FORMAT_FLOAT_LE
#define MAIN_PCM_BUF_DURATION 3
#define SAMPLE_PER_READ 1760

char **cli_arguments;
pthread_t thr[NUM_THREADS];

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

int main(int argc, char** argv)
{   
	audio_device *recdev = NULL; 
	audio_settings *settings = NULL;
	audio_device *playdev = NULL;
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

	make_ws_queue(&session_wsq);

	pthread_create(&thr[0], NULL, &ws_thread, NULL);

	free_clargs(cli_arguments);
	free_as(settings);
	free_ad(recdev);
	free_ad(playdev);
	//free_proto_list(protocols);
	//free_context_creation_info(info);
	//free_client_connection_info(cc_info);
	snd_config_update_free_global();

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

	if (*ab){
		fprintf(stderr, "pop_session_ws_queue: ab is not a null\n");
		return -1;
	}

	pop_ws_queue(session_wsq, &new);
	
	printf("pop_session_ws_queue: new->size-%d\n", new->size);	
	err = make_ab(
		ab,
		SAMPLE_RATE,
		1,
		RECORD_FORMAT,
		am_byte,
		new->size
	);
	if (err < 0){
		return err;
	}
	printf("pop_session_ws_queue: ab->size-%d\n", (*ab)->size);	

	memcpy((*ab)->buf, new->data, new->size);
	free_ws_queue_item(new);

	return 0;
}

void *ws_thread(void *arg)
{
	ws_proto_list protocols = NULL;
	lws_context_creation_info *info = NULL;
	lws_context *context = NULL;
	lws_client_connect_info *cc_info = NULL;
	lws *client_wsi = NULL;
	int err;

	//Websocket data intialization
	err = make_proto_list(
		&protocols, 
		"\0", 
		&ws_callback, 
		LWS_PRE + 2, 
		MAX_PAYLOAD / 10
	);
	if (err < 0){
		exit(err);
	}

	err = make_context_creation_info(&info, protocols);
	if (err < 0){
		exit(err);
	}

	context = lws_create_context(info);
	if (!context){
		printf("lws init failed\n");
		exit(-1);
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
		exit(err);
	}
	
	client_wsi = lws_client_connect_via_info(cc_info);
	if (!client_wsi) {
		fprintf(stderr, "lws_client_connect_via_info failed\n");
		lws_context_destroy(context);
		exit(-2);
	}

	while (!ws_conn_is_ready()){
		lws_service(context, 0);
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
		ws_conn_writable_flag = 1;

			
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

int ws_conn_is_ready()
{
	return ws_conn_established_flag && ws_conn_writable_flag;
}
