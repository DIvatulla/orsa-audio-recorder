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
#include <pthread.h>
#include <speex/speex_resampler.h> 
#include <fvad.h>

#ifndef WF 
#define WF 0
#endif

#define LOCAL_SAMPLE_RATE 48000
#define LOCAL_CHANNELS 1
#define LOCAL_RECORD_FORMAT SND_PCM_FORMAT_S16_LE

#define SERVER_SAMPLE_RATE 24000
#define SERVER_CHANNELS 1
#define SERVER_RECORD_FORMAT SND_PCM_FORMAT_S16_LE

#define PER_READ_PCM_BUF_DURATION 30 //milliseconds
#define MAIN_PCM_BUF_DURATION 60 * 1000
#define REC_WINDOW 240
#define SILENCE_WINDOW (3 * 1000) / PER_READ_PCM_BUF_DURATION

static char **cli_arguments;

static audio_device *recdev = NULL; 
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;

static ws_queue *session_wsq = NULL;
static ws_state wscs = WS_NONE;
static const char endmsg[] = "END";
static const char haltmsg[] = "HALT";
static char msg[LWS_PRE+4] = {0};

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
int push_audio_ws_queue(audio_buffer *ab);
int pop_audio_ws_queue(audio_buffer **ab);

int record();
int play();

void *record_thread(void *arg);
void record_thread_cleanup(void *arg);
void *play_thread(void *arg);
void play_thread_cleanup(void *arg);

int main(int argc, char** argv)
{   
	int err = 0;
	
	cli_arguments = parse_clargs(argc, argv);
	if (cli_arguments == NULL){
		return -1;
	}

	//ALSA data intialization
	err = make_as(&settings, LOCAL_SAMPLE_RATE, LOCAL_CHANNELS, LOCAL_RECORD_FORMAT);
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

	wscs = WS_NONE;
	make_ws_queue(&session_wsq);

	pthread_t thr[2];
	pthread_create(&thr[0], NULL, &record_thread, NULL);
	pthread_create(&thr[1], NULL, &play_thread, NULL);

	pthread_join(thr[0], NULL);
	pthread_join(thr[1], NULL);

	//record();
	//play();

	free_clargs(cli_arguments);
	free_as(settings);
	free_ad(recdev);
	free_ad(playdev);
	free_ws_queue(session_wsq);
	snd_config_update_free_global();

	//free_proto_list(protocols);
	//free_context_creation_info(info);
	//free_client_connection_info(cc_info);

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
	ws_queue_item *first = NULL;

	pop_ws_queue(session_wsq, &first);
	if (!first){
		*ab = NULL;
		return -1;
	}
	*ab = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (!ab){
		return -1;
		fprintf(stderr, "pop_session_ws_queue: can't malloc audio buffer\n");
	}
	
	printf("pop_session_ws_queue: new->size-%d\n", first->size);	
	
	(*ab)->buf = first->data;
	(*ab)->size = first->size;
	(*ab)->wi = 0;
	(*ab)->ri = 0;
	(*ab)->sample_rate = LOCAL_SAMPLE_RATE;
	(*ab)->channels = LOCAL_CHANNELS;
	(*ab)->pcm_format = LOCAL_RECORD_FORMAT;
	free(first);
	first = NULL;

	printf("pop_session_ws_queue: ab->size-%d\n", (*ab)->size);	
	
	return 0;
}

int record()
{
	Fvad *vad = fvad_new();
	audio_buffer *rb = NULL;
	int i, ret, err, brdr, bpr, spr; //bpr - bytes per read, spr - sample per read

	fvad_set_sample_rate(vad, LOCAL_SAMPLE_RATE);
	fvad_set_mode(vad, 1);
	brdr = pcm_byte_per_second(
		get_rate_ad(recdev), 
		get_chan_ad(recdev), 
		get_pfmt_ad(recdev)) * MAIN_PCM_BUF_DURATION;
	bpr = pcm_byte_per_second(
		get_rate_ad(recdev),
		get_chan_ad(recdev), 
		get_pfmt_ad(recdev)) * PER_READ_PCM_BUF_DURATION / 1000;
	spr = bpr / pcm_byte_per_frame(get_chan_ad(recdev), get_pfmt_ad(recdev));

	for (i = 0; i <= brdr; i += bpr){
		printf("i %d, brdr %d\n", i, brdr);
		rb = NULL;
		rb = rec_ad(recdev, spr);
		if (!rb){
			fprintf(stderr, "record: error\n");
		}
		if (fvad_process(vad, (int16_t*)rb->buf, spr)){
			printf("Speech detected\n");	
		}
		convert_pcm_buf(&rb, 16000);
		push_audio_ws_queue(rb);
	}
	
	fvad_free(vad);
	return 0;
}			

int play()
{
	audio_buffer *rb = NULL;

	while (session_wsq->head){
		printf("play: session_wsq->head == NULL - %d\n", session_wsq->head == NULL);
		pop_audio_ws_queue(&rb);
		if (!rb){
			printf("continue\n");
			continue;
		}
		convert_pcm_buf(&rb, 48000);
		play_ad(playdev, rb);
		free_ab(rb);
		printf("play: session_wsq->item_count - %d\n", session_wsq->count);
	}

	return 0;
}

void *record_thread(void *arg)
{
	Fvad *vad = fvad_new();

	pthread_cleanup_push(&record_thread_cleanup, (void*)vad);
	
	audio_buffer *rb = NULL;
	long int i;
	int cond = ((LOCAL_SAMPLE_RATE / 1000) * (MAIN_PCM_BUF_DURATION));
	int spr = ((LOCAL_SAMPLE_RATE / 1000) * PER_READ_PCM_BUF_DURATION);
	int s_count = 0;	
	
	fvad_set_sample_rate(vad, LOCAL_SAMPLE_RATE);
	fvad_set_mode(vad, 3);
	
	printf("MAIN_PCM_BUF_DURATION %d\n", MAIN_PCM_BUF_DURATION);
	printf("cond %d\n", cond);
	printf("spr %d\n", spr);
	for (i = 0; i <= cond; i += spr){
		printf("i = %d\n", i);
		pthread_mutex_lock(&session_wsq->mutex);
		printf("recording\n");

		if (wscs == WS_KILL){
			pthread_exit(NULL);
		}

		rb = NULL;
		rb = rec_ad(recdev, spr);
		if (!rb){
			fprintf(stderr, "record: error\n");
			exit(-1);
		}

		if (!fvad_process(vad, (int16_t*)rb->buf, spr)){
			printf("Silence\n");
			++s_count;
			if (s_count >= SILENCE_WINDOW){
				wscs = WS_SEND_END;
				pthread_exit(NULL);
			}
		}
		else{
			printf("Speech detected\n");
			s_count = 0;
		}

		push_audio_ws_queue(rb);
		printf("record: session_wsq->count - %d\n", session_wsq->count);
		if (session_wsq->count >= (REC_WINDOW / PER_READ_PCM_BUF_DURATION)){
        	pthread_cond_signal(&session_wsq->cond);
		}

		pthread_mutex_unlock(&session_wsq->mutex);
	}

	wscs = WS_SEND_END;
	fvad_free((Fvad*)arg);
	pthread_mutex_unlock(&session_wsq->mutex);
	pthread_cond_broadcast(&session_wsq->cond);
	pthread_cleanup_pop(0);
}
void record_thread_cleanup(void *arg)
{
	printf("record_thread_cleanup\n");
	wscs = WS_SEND_END; 
	pthread_mutex_unlock(&session_wsq->mutex);
	pthread_cond_signal(&session_wsq->cond);
	fvad_free((Fvad*)arg);
}

void *play_thread(void *arg)
{
	pthread_cleanup_push(&play_thread_cleanup, NULL);

	int i;
	audio_buffer *rb = NULL;

	for (;;){
		pthread_mutex_lock(&session_wsq->mutex);
		printf("playing\n");

		while ((session_wsq->count < (REC_WINDOW / PER_READ_PCM_BUF_DURATION))) {
			if (wscs == WS_SEND_END){
				printf("WS_SEND_END\n");
				pthread_exit(NULL);
			}
			pthread_cond_wait(&session_wsq->cond, &session_wsq->mutex);
		}
		printf("stop wait\n");

		for (i = 1; i <= (REC_WINDOW / PER_READ_PCM_BUF_DURATION); i++){
			pop_audio_ws_queue(&rb);
			if (rb){
				play_ad(playdev, rb);
				free_ab(rb);
				printf("play: session_wsq->count - %d\n", session_wsq->count);
			}
			rb = NULL;
			if (wscs == WS_SEND_END){
				pthread_exit(NULL);
			}
		}

		pthread_mutex_unlock(&session_wsq->mutex);
	}

	pthread_mutex_unlock(&session_wsq->mutex);
	pthread_cleanup_pop(0);
}
void play_thread_cleanup(void *arg)
{
	printf("play_thread_cleanup\n");
	audio_buffer *rb = NULL;

	if (session_wsq->count > 0){
		while (session_wsq->head){
			pop_audio_ws_queue(&rb);
			printf("consumer cleanup\n");
			play_ad(playdev, rb);
			free_ab(rb);
		}
	}

	pthread_mutex_unlock(&session_wsq->mutex);	
}
