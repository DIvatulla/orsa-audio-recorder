#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

audio_device *recdev = NULL; //recording device 
audio_settings *settings = NULL; //audio parameters
audio_buffer *main_buffer = NULL; //main audio buffer

int mb_dur = 60;
int spr = 4096;

double bottomline_volume;

int setup_main_settings();
int setup_main_buffer();
int setup_recdev();
int record(audio_buffer *mb, audio_buffer *rb);
double measure_volume();
int listen();

int main(int argc, char** argv)
{   
	int err;

	err = setup_main_settings();
	if (err < 0){
		return err;
	}	

	err = setup_main_buffer();
	if (err < 0){
		return err;
	}

	err = setup_recdev();
	if (err < 0) {
		return err;
	}

	err = open_mic(recdev);
	if (err < 0) {
		return err;
	}

	bottomline_volume = measure_volume();
	if (bottomline_volume < 0) {
		fprintf(stderr, "RMS calculation is -1.0");
		return 1;
	}
	printf("bottom line volume %f\n", bottomline_volume);
	

	//listen();

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

int setup_main_buffer()
{
	int err;

	main_buffer = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (main_buffer == NULL) {
		fprintf(stderr, "Couldn't allocate memory for main audio buffer");
		return err;
	}

	err = init_ab(main_buffer, settings, am_usecs, 1000000 * mb_dur);
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
	
	if (copy_as(recdev->settings, settings) < 0){
		return -1;
	}

	return 0;
}

int record(audio_buffer *mb, audio_buffer *rb)
{
	int ret;

	if (rb->samples > mb->samples){
		return -1;
	}

	do {
		ret = snd_pcm_readi(recdev->pcm_device, rb->buf, rb->samples);
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->pcm_device);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi = spr * rb->settings->bytes_per_sample;
		push_ab(mb, rb);
	} while(mb->wi <= (mb->size - rb->size));

	return 0;
}

double measure_volume()
{
	int err;
	int i = 0;
	long double sum = 0.0f;
	audio_buffer *mb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	audio_buffer *rb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	
	init_ab(mb, settings, am_usecs, 10 * 1000000);
	init_ab(rb, settings, am_samples, 4096);

	printf("Measuring volume\n");
	record(mb, rb);

	mb->ri = 0;

	printf("mb->wi - %d\n", mb->wi);	
	while (mb->ri <= mb->size) {
		printf("rms - %f\n", rms(mb, spr));	
		sum = sum + rms(mb, spr);
		printf("sum - %Lf\n", sum);	
		++i;
	}

	free_ab(mb);
	free_ab(rb);

	return (double)(sum / i);
}

int listen()
{
	int ret;

	audio_buffer *rb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	init_ab(rb, settings, am_samples, spr);

	for (;;) {
		rb->wi = 0;
		ret = snd_pcm_readi(recdev->pcm_device, rb->buf, rb->samples);
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->pcm_device);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi = spr * rb->settings->bytes_per_sample;
		rb->ri = 0;
		printf("rms = %f\n", rms(rb, spr));
	}
}


void write_file(char *filename)
{
	FILE *f = fopen(filename, "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
	}
	printf("m->wi %d\n", main_buffer->wi);
    fwrite(main_buffer->buf, sizeof(char), main_buffer->wi, f);
    fclose(f);

    printf("Saved\n");
}
