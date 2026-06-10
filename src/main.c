#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

audio_device *recdev = NULL; //recording device 
audio_settings *settings = NULL; //audio parameters
audio_buffer *main_buffer = NULL; //main audio buffer

int mb_dur = 10;
int spr = 4096;

double bottomline_volume;

//int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr);
int setup_main_settings();
int setup_main_buffer();
int setup_recdev();
audio_buffer *make_rb();
int recording();
double measure_volume();

//int listen(int duration);

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

	open_mic(recdev);
	bottomline_volume = measure_volume();
	err = recording();

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

audio_buffer *make_rb()
{
	int err;

	audio_buffer *frame_buffer = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if (frame_buffer == NULL) {
		fprintf(stderr, "Couldn't allocate memory for frame audio buffer");
		return NULL;
	}
	err = init_ab(frame_buffer, settings, am_samples, spr); //buffer 4096 samples
	if (err < 0) {
		fprintf(stderr, "Can't set settings for frame buffer");
		return NULL;
	}

	return frame_buffer;
}

double measure_volume()
{
	int ret;
	int i = 0;
	int rms_arr_size = main_buffer->size / spr;
	double sum = 0;
	double *rms_arr = calloc(rms_arr_size, sizeof(double));
	audio_buffer *rb = make_rb();

	do {
		ret = snd_pcm_readi(recdev->pcm_device, rb->buf, spr);
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->pcm_device);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi = rb->settings->bytes_per_sample * spr;
		rms_arr[i] = rms(rb, spr);
		printf("rms = %f\n", rms_arr[i]);
		++i;
		main_buffer->wi += spr * main_buffer->settings->bytes_per_sample;

		printf("main_buffer->wi %d\n", main_buffer->wi);
	} while(main_buffer->wi <= (main_buffer->size - rb->size));

	free_ab(rb);
	main_buffer->wi = 0;
	
	for (i = 0; i < rms_arr_size; i++){
		printf("rms_arr[i] = %f\n", rms_arr[i]);
		sum += rms_arr[i];
		printf("sum = %f\n", sum);
	}

	free(rms_arr);

	return sum / rms_arr_size;
}

int recording()
{
	int ret;
	double r;
	audio_buffer *rb = make_rb();
	ret = 0;

	do {
		ret = snd_pcm_readi(recdev->pcm_device, rb->buf, spr);
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->pcm_device);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi = spr * rb->settings->bytes_per_sample;
		r = rms(rb, spr);
		if (r < bottomline_volume) {
			printf("Silence, rms = %f\n", r);
		}
		else{
			printf("Not silence, rms = %f\n", r);
		}

		push_ab(main_buffer, rb);
		printf("main_buffer->wi = %d\n", main_buffer->wi);
	} while(main_buffer->wi <= (main_buffer->size - rb->size));

	free_ab(rb);
	return ret;
}

/*
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
*/