#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef enum {
	am_usecs = 1, //count frames by second
	am_samples = 2,
	am_frames = 3 //count frames by sample rate and amoutn of channels
} audio_measure;

typedef struct {
	char *buf;
	int size;
	int wi;	//write index
	int ri; //read index
} audio_buffer;

typedef struct {
	char *device_name;
	unsigned int sample_rate;
	int channels;
	snd_pcm_format_t pcm_format;
	unsigned int bytes_per_frame;
} audio_settings;

static snd_pcm_t *device; //audio connection
static snd_pcm_hw_params_t *params; //parameters

int open_mic(snd_pcm_t **d, snd_pcm_hw_params_t **p, audio_settings *s);
int usec_to_frames(audio_settings *s, int usec);
int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr);
int init_ab(audio_buffer *ab, audio_settings *s, audio_measure m, int mod);
int free_ab(audio_buffer *ab);
int push_ab(audio_buffer *ab);
int pop_ab(audio_buffer *ab);

int main(int argc, char** argv)
{    
	int err;
	audio_settings *settings;
	audio_buffer *main_buffer; //main audio buffer
	audio_buffer *frame_buffer; //frame audio buffer

	/*DEVICE INIT*/
	settings = calloc(1, sizeof(audio_settings));
	if (settings == NULL) {
		printf("malloc on audio_settings has failed");
	}
		
	settings->device_name = "hw:1";
	settings->sample_rate = 44100;
	settings->channels = 2; //stereo
	settings->pcm_format = SND_PCM_FORMAT_S16_LE;
	settings->bytes_per_frame = (snd_pcm_format_width(settings->pcm_format) / 8) * (settings->channels);
	snd_pcm_hw_params_free(params);

	if ((err = open_mic(&device, &params, settings)) > 0) {
		return err;
	}
	/*DEVICE INIT*/

	/*RECORDING*/
		main_buffer = calloc(1, sizeof(audio_buffer));
		err = init_ab(main_buffer, settings, am_usecs, 1000000 * 10); //buffer 1 seccond
		printf("main buffer's size %d\n", main_buffer->size);

		frame_buffer = calloc(1, sizeof(audio_buffer));
		err = init_ab(frame_buffer, settings, am_samples, 4096); //buffer 4096 samples
		printf("frame buffer's size %d\n", frame_buffer->size);

		int file_size = recording(device, main_buffer, frame_buffer, 4096);
		free_ab(frame_buffer);
	/*RECORDING*/

	/*WRITING A FILE*/
	FILE *f = fopen("out.raw", "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
		return 1; 
	}
	printf("file size - %d\n", (unsigned int)file_size);
    fwrite(main_buffer->buf, sizeof(char), file_size, f);
    fclose(f);

    printf("Saved\n");

	snd_pcm_close(device);
	free_ab(main_buffer);
	free(settings);

	return 0;
}

int open_mic(snd_pcm_t **d, snd_pcm_hw_params_t **p, audio_settings *s)
{
	int err;

	err = snd_pcm_open(d, s->device_name, SND_PCM_STREAM_CAPTURE, 0); //open ALSA connection
    if (err < 0) {
        fprintf(stderr, "Can't open device %s\n", s->device_name);
    }
	err = snd_pcm_hw_params_malloc(p); //allocate memory for possible parameters of device
	if (err < 0) {
		fprintf(stderr, "Error while allocating memory for device's parameters %s\n", s->device_name);
	}
	err = snd_pcm_hw_params_any(*d, *p); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't get available parameters for device %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_access(*d, *p, SND_PCM_ACCESS_RW_INTERLEAVED); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't set access %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_format(*d, *p, s->pcm_format); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't set pcm frame-byte-format %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_rate_near(*d, *p, &s->sample_rate, 0); //setting the sample rate
	if (err < 0) {
		fprintf(stderr, "Can't set sample rate on %s to %d\n", s->device_name, s->sample_rate);
	}
	err = snd_pcm_hw_params_set_channels(*d, *p, s->channels); //mono/stereo
	if (err < 0) {
		fprintf(stderr, "Can't get available parameters for device %s\n", s->device_name);
	}
	err = snd_pcm_hw_params(*d, *p);
    if (err < 0) {
        fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
    }

	return err;
}


int init_ab(audio_buffer *ab, audio_settings *s, audio_measure m, int mod)
{
	switch (m){
		case am_usecs:
			ab->size = s->bytes_per_frame * usec_to_frames(s, mod);
			if (ab->size < 0){
				fprintf(stderr, "The frame length is too small\n");
				return -1;
			}
			break;
		case am_samples:
			ab->size = s->bytes_per_frame * mod;
			break;
		case am_frames:
			ab->size = s->bytes_per_frame * mod;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memory allocation\n");
			return -1;
	}

	ab->buf = calloc(ab->size, sizeof(char));
	if (ab->buf == NULL) {
		fprintf(stderr, "Error while allocating memory for audio buffer");
		return -1;
	}

	return 0;
}

int free_ab(audio_buffer *ab)
{
	free(ab->buf);
	free(ab);
	return 0;
}

int usec_to_frames(audio_settings *s, int usec)
{
	int uspf = (int)floorf(((1000000.0f) / (float)s->sample_rate)); //microseconds per frame

	if (usec < uspf) {
		return -1;
	}
	else {
		return (usec / uspf) + (usec % uspf);
	}
}

int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr) //samples per reading
{
	int read = 0;
	int to_read = m->size - f->size;
	int ret;

	if ((ret = snd_pcm_prepare(d)) < 0) {
    	fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror (ret));
		return -1;
  	}

	printf("Recording...\n");
	do {
		ret = snd_pcm_readi(d, f->buf, spr);
		if (ret == -EPIPE){
			snd_pcm_prepare(d);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			return -1;
		}
		read += f->size;

		memcpy((m->buf + read) - f->size, f->buf, f->size);
		memset(f->buf, 0, f->size);
		printf("how many bytes was read %d\n", read);

	} while(read < to_read);

	return read;
}
