#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef enum {
	abuf_msecf = 1, //count frames by second
	abuf_srf = 2; //count frames by sample rate and amoutn of channels
} malloc_abuf_specifier;

typedef struct {
	char *device_name;
	unsigned int sample_rate;
	int channels;
	snd_pcm_format_t pcm_format;
	int mpf; //memory per frame
} audio_settings;

static snd_pcm_t *device; //audio connection
static snd_pcm_hw_params_t *params; //parameters

int main(int argc, char** argv)
{    
	int err;
	audio_settings *settings;
	char *main_buffer; //main audio buffer
	char *frame_buffer; //frame audio buffer

	/*DEVICE INIT*/
	settings = calloc(1, sizeof(audio_settings));
	if (settings == NULL) {
		printf("malloc on audio_settings has failed");
	}
		
	settings->device_name = "plughw:1";
	settings->sample_rate = 44100;
	settings->dimension = 2; //stereo
	settings->pcm_format = SND_PCM_FORMAT_S16_LE;
	settings->mpf = snd_pcm_format_width(settings->pcm_format) / 8) * (settings->channels);

	if ((err = open_mic(device, params, settings)) > 0) {
		return err;
	}
	/*DEVICE INIT*/

	/*RECORDING*/
		main_buffer = malloc_abuf(settings, abuf_msecf, 1000000); //malloc 1 seccond
		frame_buffer = malloc_abuf(settings, abuf_srf, 4096); //malloc 4096 frames
		err = recording(device, audio_settings, main_buffer, frame_buffer, 4096);
	/*RECORDING*/

	
	FILE *f = fopen("out.raw", "wb");
    if (!f) { 
		perror("fopen"); return 1; 
	}
    fwrite(buf, 1, 60 * (16000 * ((snd_pcm_format_width(SND_PCM_FORMAT_S16_LE) / 8) * 2)), f);
    fclose(f);

    printf("Saved\n");

	snd_pcm_close(device);
	free(buf);
    return 0;
}

int open_mic(snd_pcm_t *d, snd_pcm_hw_params_t *p, audio_settings *s)
{
	int err;

	err = snd_pcm_open(&d, s->device_name, SND_PCM_STREAM_CAPTURE, 0); //open ALSA connection
    if (err < 0) {
        fprintf(stderr, "Can't open device %s\n", s->device_name);
    }
	err = snd_pcm_hw_params_malloc(&p); //allocate memory for possible parameters of device
	if (err < 0) {
		fprintf(stderr, "Can't allocate memory for device's parameters %s\n", s->device_name);
	}
	err = snd_pcm_hw_params_any(d, p); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't get available parameters for device %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_channels(d, p, s->channels); //mono/stereo
	if (err < 0) {
		fprintf(stderr, "Can't get available parameters for device %s\n", s->device_name);
	}
	err = snd_pcm_hw_params_set_rate_near(d, p, &s->sample_rate, 0) //setting the sample rate
	if (err < 0) {
		fprintf(stderr, "Can't set sample rate on %s to %d\n", s->device_name, s->sample_rate);
	}
	
	err = snd_pcm_hw_params(d, p);
    if (err < 0) {
        fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
    }

	return err;
}


char *malloc_abuf(audio_settings *s, malloc_abuf_format sp, unsigned int mod)
{
	abuf b;
	int frames;

	switch (sp){
		case msecf:
			bsize = s->mpf * usec_to_frames(s->sample_rate, mod);
			break;
		case srf:
			bsize = s->mpf * mod * s->channels;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memeory allocation");
			return NULL;
	}

	if (frames < 0) {
		fprintf(stderr, "Requested buffer length in microseconds is too small, %d\n", usec);
		return NULL;
	}

	b = calloc(bsize, sizeof(abuf));

	return b;
}

int usec_to_frames(audio_settings *s, int usec)
{
	int spf = (int)round(((1 * 1000000) / (float)s->sample_rate)); //seconds per frame
	if (usec < spf) {
		return -1;
	}
	else {
		return (usec / spf) + ((usec % spf) >= 1) * s->channels;
	}
}

int recording(snd_pcm_t *d, audio_settings *s, char* mabuf, char* fabuf, int fpr) //fpr frames per reading
{
	//framelen - how much samples in one frame
	int rframes = 0;
	int framemem = s->mpf * framelen * 2;
	int tobeframes = calc_frames(s->sr, 60000000); //1 minute
	
	if ((mabuf || fabuf) == NULL) {
		return -1;
	}

	while (rfames < tobeframes) {
		int ret = snd_pcm_readi(d, fabuf, framelen);
		if (ret == -EPIPE){
			snd_pcm_prepare(d);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			return -1;
		}
		
		rframes += framemem;

		memcpy((mabuf + rframes) - framemem, fabuf, framemem);
		memset(fabuf, 0, framemem);
	}

	return mabuf;
}
