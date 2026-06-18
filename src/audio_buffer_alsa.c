#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

int set_ab_size(audio_buffer *b, audio_device *d, audio_measure mu, int mod) 
{
    if (b == NULL){
        fprintf(stderr, "Pointer to buffer is null\n");
        return -1;
    }
    if (d == NULL) {
        fprintf(stderr, "Pointer to device is null\n");
        return -2;
    }

    unsigned int rate = 0;
    snd_pcm_hw_params_get_rate(d->params, &rate, 0);

	switch (mu){
		case am_usecs:
            b->size = snd_pcm_frames_to_bytes(d->handle, rate * mod);
			break;
		case am_samples:
			b->size =  snd_pcm_samples_to_bytes(d->handle, mod);
			break;
		case am_frames:
			b->size = snd_pcm_frames_to_bytes(d->handle, 1) * mod;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memory allocation\n");
			return -1;
	}

	return 0;
}

int init_ab(audio_buffer *b, audio_device *d, audio_measure mu, int mod)
{
	int err;

    if (b == NULL){
        fprintf(stderr, "Audio buffer pointer is null\n");
        return -1;
    }
	if (d == NULL){
		fprintf(stderr, "Audio device pointer is null\n");
		return -2;
	}
	
	err = set_ab_size(b, d, mu, mod);
	if (err < 0){
		return err;
	}

	b->buf = (char*)calloc(b->size, sizeof(char));
	if (b->buf == NULL) {
		fprintf(stderr, "Error while allocating memory for audio buffer\n");
		return -1;
	}

	b->wi = 0;
	b->ri = 0;

	return 0;
}

int push_ab(audio_buffer *to, audio_buffer *from)
{
	if (from->size >= to->size) {
		fprintf(stderr, "Size of source is more than destinantion's\n");
		return -1;
	}

	from->ri = 0;
	for (from->ri = 0; from->ri != from->size; ++to->wi, ++from->ri) {
		if (to->wi > to->size){
			to->wi = 0;
		}

		to->buf[to->wi] = from->buf[from->ri];
	}

	return 0;
}

void free_ab(audio_buffer *b)
{
    free(b->buf);
    free(b);
}