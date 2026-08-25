#include "../include/wwaudio.h"
#include "../include/clarg.h"
#include "../include/websockc.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
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
#define MAIN_PCM_BUF_DURATION 60 //seconds 
#define REC_WINDOW 240
#define SILENCE_WINDOW (3 * 1000) / PER_READ_PCM_BUF_DURATION

static char **cli_arguments;

static audio_device *recdev = NULL; 
static audio_settings *settings = NULL;
static audio_device *playdev = NULL;
static audio_buffer *pcm_audio_buffer = NULL;

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

int main(int argc, char** argv)
{   
	int err = 0;
	
	cli_arguments = parse_clargs(argc, argv);
	if (!cli_arguments) return -1;

	//ALSA data intialization
	err = make_as(&settings, LOCAL_SAMPLE_RATE, LOCAL_CHANNELS, LOCAL_RECORD_FORMAT);
	if (err < 0) return err;

	err = make_ad(
		&recdev, 
		(cli_arguments[INPUT_DEV] ? cli_arguments[INPUT_DEV] : "plughw:0"), 
		settings, 
		SND_PCM_STREAM_CAPTURE
	);
	if (err < 0) return err;

	err = make_ad(
		&playdev,
		(cli_arguments[OUTPUT_DEV] ? cli_arguments[OUTPUT_DEV] : "plughw:0"),
		settings, 
		SND_PCM_STREAM_PLAYBACK
	);
	if (err < 0) return err;

	err = make_ab(
		&pcm_audio_buffer,
		get_rate_ad(recdev),
		get_chan_ad(recdev),
		get_pfmt_ad(recdev),
		am_sec,
		MAIN_PCM_BUF_DURATION
	);
	if (err < 0) return err;

	while (pcm_audio_buffer->wi <= pcm_audio_buffer->size){
		audio_record(
			recdev, 
			pcm_audio_buffer, 
			(get_rate_ad(recdev) / 1000) * 
				PER_READ_PCM_BUF_DURATION *
				get_chan_ad(recdev)
		);
	}

	wscs = WS_NONE;


	free_clargs(cli_arguments);
	free_as(settings);
	free_ad(recdev);
	free_ad(playdev);
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