#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

audio_device *recdev = NULL; //recording device 
audio_settings *settings = NULL; //audio parameters
audio_buffer *main_buffer = NULL; //main audio buffer
spr = 1024;

//int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr);
int setup_main_settings();
int setup_main_buffer(int sec);
int setup_recdev();
audio_buffer *make_rb();

int main(int argc, char** argv)
{   
	int err;

	err = setup_main_settings();
	if (err < 0){
		return err;
	}	

	err = setup_main_buffer(60);
	if (err < 0){
		return err;
	}

	err = setup_recdev();
	if (err < 0) {
		return err;
	}

	open_mic(recdev);
	listen(recdev);
	//err = recording(rec_device, main_buffer, frame_buffer, 4096);

	free_ad(recdev);
	free_ab(main_buffer);
	free_as(settings);

	return 0;
}

int setup_main_settings()
{
	int err;

	settings = (audio_settings*)calloc(1, sizeof(audio_settings));
	if (settings == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings");
		return -1;
	}
	err = init_as(settings, "hw:1", 44100, 2, SND_PCM_FORMAT_S16_LE);
	if (err < 0){
		return err;
	}
}

int setup_main_buffer(int sec)
{
	int err;

	main_buffer = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (main_buffer == NULL) {
		fprintf(stderr, "Couldn't allocate memory for main audio buffer");
		return err;
	}

	err = init_ab(main_buffer, settings, am_usecs, 1000000 * sec);
	if (err < 0) {
		fprintf(stderr, "Can't set settings for main buffer");
		return err;
	}
	printf("main buffer's size %d\n", main_buffer->size);
	printf("bpr %d\n", main_buffer->settings->bytes_per_sample);
}

int setup_recdev()
{
	recdev = (audio_device*)calloc(1, sizeof(audio_device));
	recdev->pcm_device = NULL;
	recdev->settings = (audio_settings*)calloc(1, sizeof(audio_settings));
	memcpy(recdev->settings, settings, sizeof(audio_settings));

	return 0;
}

audio_buffer *make_rb()
{
	int err;

	audio_buffer *frame_buffer = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (frame_buffer == NULL) {
		fprintf(stderr, "Couldn't allocate memory for frame audio buffer");
		return NULL;
	}
	frame_buffer->settings = (audio_settings*)calloc(1, sizeof(audio_settings));
	if (frame_buffer->settings == NULL) {
		fprintf(stderr, "Couldn't allocate memory for frame audio buffer's settings");
		return NULL;
	}
	err = init_ab(frame_buffer, settings, am_samples, 4096); //buffer 4096 samples
	if (err < 0) {
		fprintf(stderr, "Can't set settings for frame buffer");
		return NULL;
	}

	return frame_buffer;
}

int listen(int duration, int timeout)
{
	int ret;
	audio_buffer *rb = make_rb();

	if (rb == NULL) {
		return -1;
	}

	for (int i = 0;i < 100; i++){
		ret = snd_pcm_readi(recdev->pcm_device, rb->buf, spr);
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->pcm_device);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			return -1;
		}

		rb->wi = rb->settings->bytes_per_sample * spr;
		printf("%d bytes were read\n", rb->wi);
		printf("rms = %f\n", rms(rb, spr));
	}

	free_ab(rb);
}
/*
int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr) //samples per reading
{
	int ret;
	int sample_counter;

	if ((ret = snd_pcm_prepare(d)) < 0) {
    	fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror (ret));
		return -1;
  	}

	printf("buf start %ld\n", (long int)m->buf);
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
		push_ab(m, f);
		printf("how many bytes was read %d\n", m->wi);
		++sample_counter;

		if (sample_counter == 5) {
			printf("rms %.6f\n", rms(m, spr*5));
			sample_counter = 0;
		}
	} while(m->wi < ((m->size) - (f->size)));

	return 0;
}
*/

void write_file(char *filename)
{
	FILE *f = fopen(filename, "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
		return -1;; 
	}
	printf("m->wi %d\n", main_buffer->wi);
    fwrite(main_buffer->buf, sizeof(char), main_buffer->wi, f);
    fclose(f);

    printf("Saved\n");
}