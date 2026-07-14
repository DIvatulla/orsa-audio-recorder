#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include "../include/clarg.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#ifndef RMS
#define RMS 0
#endif
#ifndef WF 
#define WF 0
#endif

#define MAX_PAYLOAD 5242880

static const unsigned int srate = 44100; //sample rate
static const int ch = 1;
static const snd_pcm_format_t record_form = SND_PCM_FORMAT_S16_LE;
static const int mb_dur = 30;
static const int spr = 4096; //samples per read

//global variables for threads
int interrupted = 0;

typedef struct{
    unsigned char send_buf[LWS_PRE + MAX_PAYLOAD]; /* LWS_PRE bytes of headroom for framing */
    size_t send_len;
    int    pending_send;
} ws_session_data;

audio_device *recdev = NULL; //recording device 
audio_settings *settings = NULL; //audio parameters
double bottomline_volume;
audio_device *playdev;

lame_enc *lemp3;
lame_mp3_buf *lbmp3;
mpeg_dec *mdmp3;

int record(audio_buffer *mb, audio_buffer *rb, double border_vol, int sf_count);
double measure_volume(audio_buffer *ab);
int listen(audio_device *d);
void write_file(unsigned char *b, int size, char *filename);
void play_mp3(mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb);

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

	#if RMS
	record(mb, rb, 0.0f, 0);
	bottomline_volume = measure_volume(mb);
	if (bottomline_volume < 0) {
		fprintf(stderr, "RMS calculation is -1.0");
		return 1;
	}
	printf("bottom line volume %f\n", bottomline_volume);
	#endif
	
	mb->wi = 0;
	record(mb, rb, bottomline_volume, calc_ab_size(recdev, am_secs, 3));
	
	err = make_lame_mp3_buf(&lbmp3, mb);
	if (err < 0){
		return -1;
	}
	encode(lemp3, lbmp3, recdev, mb, &mp3_filesize);
	
	free_ab(mb);
	free_ad(recdev);
	free_as(settings);
	free_enc(lemp3);

	make_dec(&mdmp3);
	decode_mp3_settings(mdmp3, lbmp3);
	play_mp3(mdmp3, lbmp3, rb);
	free_ab(rb);

	#if WF
	write_file(lbmp3->buf, mp3_filesize, "./out.mp3");
	#endif

	free_lame_mp3_buf(lbmp3);
	free_ad(playdev);
	free_dec(mdmp3);
	free_clargs(cli_arguments);

	return 0;
}

int record(audio_buffer *mb, audio_buffer *rb, double border_vol, int sf_count)
{
	int old_sf_count = sf_count;
	double frame_vol = 0.0f;
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

		if (old_sf_count > 0){
			rb->ri = 0;
			frame_vol = rms(rb, 4096);
			printf("rms - %f\n", frame_vol);
			if (frame_vol > (border_vol + 15.0f)) {
				sf_count = old_sf_count;
				printf("sound\n");
			}
			else{
				sf_count -= 4096;
				printf("silence\n");
				printf("sf_count %d\n", sf_count);
			}
			if (sf_count < 0){
				break;
			}
		}

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

double measure_volume(audio_buffer *ab)
{
	double cur_rms;
	unsigned int i = 0;
	long double sum = 0.0f;

	ab->ri = 0;	
	while (ab->ri < (ab->wi - (ab->wi % (int)snd_pcm_frames_to_bytes(recdev->handle, 4096)))){
		cur_rms = rms(ab, 4096);
		sum += cur_rms;
		printf("rms - %f\n", cur_rms);
		++i;
	}
	
	return (double)(sum / i);
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



static void sigint_handler(int sig){ interrupted = 1; }

int queue_message(session_data *data, const char *msg)
{
    size_t len = strlen(msg);

    if (len > MAX_PAYLOAD){
        return -1;
    }

    memcpy(&data->send_buf[LWS_PRE], msg, len);
    data->send_len = len;
    data->pending_send = 1;

    return 0;
}

int queue_message(ws_session_data *data, const char *msg)
{
    size_t len = strlen(msg);

    if (len > MAX_PAYLOAD){
        return -1;
    }

    memcpy(&data->send_buf[LWS_PRE], msg, len);
    data->send_len = len;
    data->pending_send = 1;

    return 0;
}

int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    ws_session_data *data = (ws_session_data*)user;

    switch(reason){
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
        break;
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        lwsl_user("Connected to server.\n");
        queue_message(data, "Hello from smart-speaker!");
        lws_callback_on_writable(wsi);
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
        lwsl_hexdump_notice(in, len);
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (data->pending_send) {
            int n = lws_write(wsi, &data->send_buf[LWS_PRE], data->send_len, LWS_WRITE_TEXT);

            if (n < (int)data->send_len) {
                lwsl_err("lws_write failed (%d)\n", n);
                return -1;
            }
            data->pending_send = 0;
            lwsl_user("Sent %d bytes.\n", n);
		}
		break;
	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_user("Connection closed.\n");
        interrupted = 1;	
		break;
	}
       

void *websocket_thread(void *arg)
{
	char **cli_arguments = (char**)arg;


}