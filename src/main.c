#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr);

int main(int argc, char** argv)
{   
	int err; 
	int file_size;
	audio_buffer *main_buffer; //main audio buffer
	audio_buffer *frame_buffer; //frame audio buffer
	snd_pcm_t *rec_device; //audio connection
	audio_settings *settings;

	settings = calloc(1, sizeof(audio_settings));
	if (settings == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings");
		return -1;
	}
	err = init_as(settings, "hw:1", 44100, 2, SND_PCM_FORMAT_S16_LE);
	if (err < 0){
		return err;
	}

	err = open_mic(&rec_device, settings);
	if (err > 0) {
		return err;
	}


	main_buffer = calloc(1, sizeof(audio_buffer));
	if (main_buffer == NULL) {
		fprintf(stderr, "Couldn't allocate memory for main audio buffer");
		return err;
	}
	main_buffer->settings = calloc(1, sizeof(audio_settings));
	if (main_buffer->settings == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings");
	}

	err = init_ab(main_buffer, settings, am_usecs, 1000000 * 30);
	if (err < 0) {
		fprintf(stderr, "Can't set settings for main buffer");
		return err;
	}
	printf("main buffer's size %d\n", main_buffer->size);

	frame_buffer = calloc(1, sizeof(audio_buffer));
	if (frame_buffer == NULL) {
		fprintf(stderr, "Couldn't allocate memory for frame audio buffer");
		return -1;
	}
	frame_buffer->settings = calloc(1, sizeof(audio_settings));
	if (frame_buffer->settings == NULL) {
		fprintf(stderr, "Couldn't allocate memory for frame audio buffer's settings");
		return -1;
	}
	err = init_ab(frame_buffer, settings, am_samples, 4096); //buffer 4096 samples
	if (err < 0) {
		fprintf(stderr, "Can't set settings for frame buffer");
		return err;
	}
	printf("frame buffer's size %d\n", frame_buffer->size);

	printf("bpr %d\n", main_buffer->settings->bytes_per_frame);
	
	err = recording(rec_device, main_buffer, frame_buffer, 4096);
	free_ab(frame_buffer);

	/*WRITING A FILE*/
	FILE *f = fopen("out.raw", "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
		return 1; 
	}
	printf("m->wi %d\n", main_buffer->wi);
    fwrite(main_buffer->buf, sizeof(char), main_buffer->wi, f);
    fclose(f);

    printf("Saved\n");

	snd_pcm_close(rec_device);
	free_ab(main_buffer);
	free(settings);

	return 0;
}

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

		printf("rms %.6f\n", rms(m, spr));
	} while(m->wi < ((m->size) - (f->size)));

	return 0;
}
