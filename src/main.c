#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static snd_pcm_t *rec_device; //audio connection

int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr);

int main(int argc, char** argv)
{    
	int file_size;
	audio_settings *settings;
	audio_buffer *main_buffer; //main audio buffer
	audio_buffer *frame_buffer; //frame audio buffer

	settings = init_as("hw:1", 44100, 2, SND_PCM_FORMAT_S16_LE);
	if (settings == NULL) {
		return -1;
	}

	if (open_mic(&rec_device, settings) > 0) {
		return -1;
	}
	
	
	main_buffer = init_ab(settings, am_usecs, 1000000 * 10);
	if (main_buffer == NULL) {
		return -1;
	}
	printf("main buffer's size %d\n", main_buffer->size);

	frame_buffer = init_ab(settings, am_samples, 4096); //buffer 4096 samples
	if (frame_buffer == NULL) {
		return -1;
	}
	printf("frame buffer's size %d\n", frame_buffer->size);

	file_size = recording(rec_device, main_buffer, frame_buffer, 4096);
	free_ab(frame_buffer);

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

	snd_pcm_close(rec_device);
	free_ab(main_buffer);
	free(settings);

	return 0;
}

int recording(snd_pcm_t *d, audio_buffer *m, audio_buffer *f, int spr) //samples per reading
{
	int count = 1;
	int read = 0;
	int to_read = m->size - f->size;
	int ret;

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
		read += f->size;
		printf("how many bytes was read %d\n", read);
		push_ab(m, f);

		printf("rms %.6f\n", rms(m, 4096, 500000));
		
	} while(read < to_read);

	return read;
}
