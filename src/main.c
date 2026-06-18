#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

audio_device *recdev = NULL; //recording device 
audio_settings *settings = NULL; //audio parameters
int mb_dur = 60;
int spr = 4096;
double bottomline_volume;

int make_settings(audio_settings *s);
int make_record_device(audio_device *d, audio_settings *s);
int record(audio_buffer *mb, audio_buffer *rb);
double measure_volume(audio_device *d);
void write_file(audio_buffer *b, char *filename);


int main(int argc, char** argv)
{   
	int err;

	err = make_settings(settings);
	if (err < 0){
		return err;
	}

	err = make_record_device(recdev, settings);
	if (err < 0){
		return err;
	}

	bottomline_volume = measure_volume(recdev);
	if (bottomline_volume < 0) {
		fprintf(stderr, "RMS calculation is -1.0");
		return 1;
	}
	printf("bottom line volume %f\n", bottomline_volume);

	free_ad(recdev);
	free_as(settings);

	return 0;
}

int make_settings(audio_settings *s)
{
	int err;

	s = (audio_settings*)calloc(1, sizeof(audio_settings));
	if (settings == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings");
		return -1;
	}
	err = init_as(settings, 44100, 1, SND_PCM_FORMAT_S16_LE);
	if (err < 0){
		return err;
	}
}

int make_record_device(audio_device *d, audio_settings *s)
{
	int err;

	d = (audio_device*)calloc(1, sizeof(audio_device));
	d->name = "hw:1";
	d->handle = NULL;

	return init_rd(d, s);
}

int record(audio_buffer *mb, audio_buffer *rb)
{
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
	} while(mb->wi <= (mb->size - rb->size));

	return 0;
}



double measure_volume(audio_device *d)
{
	int err;
	int i = 0;
	long double sum = 0.0f;
	audio_buffer *mb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	audio_buffer *rb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	
	init_ab(mb, d, am_usecs, 10 * 1000000);
	init_ab(rb, d, am_frames, 4096);

	printf("Measuring volume\n");
	record(mb, rb);

	mb->ri = 0;	
	while (mb->ri < mb->wi) {
		printf("rms - %f\n", rms(mb, 4096, snd_pcm_frames_to_bytes(d, 1)));	
		sum += rms(mb, mb->ri, snd_pcm_frames_to_bytes(d, 1));
		printf("sum - %Lf\n", sum);	
		++i;
	}

	free_ab(mb);
	free_ab(rb);

	return (double)(sum / i);
}

/*
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
		if (rms(rb, spr) > bottomline_volume){
			printf("sound\n");
		}
		else{
			printf("silence\n");
		}
	}
}
*/


void write_file(audio_buffer *b, char *filename)
{
	FILE *f = fopen(filename, "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
	}
	printf("m->wi %d\n", b->wi);
    fwrite(b->buf, sizeof(char), b->wi, f);
    fclose(f);

    printf("Saved\n");
}
