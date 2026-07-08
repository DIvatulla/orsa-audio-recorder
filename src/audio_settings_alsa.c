#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>

int init_as(audio_settings *s, unsigned int r, int ch, snd_pcm_format_t pcm_f)
{
    if (s == NULL) {
        fprintf(stderr, "Pointer to settings is null\n");
        return -1;
    }

	s->rate = r;
	s->channels = ch;
	s->pcm_format = pcm_f;

	return 0;
}

int make_as(audio_settings **s, unsigned int r, int ch, snd_pcm_format_t pcm_f)
{
    int err;

	*s = (audio_settings*)calloc(1, sizeof(audio_settings));
	if (*s == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings\n");
		return -1;
	}

	err = init_as(*s, r, ch, pcm_f);
	if (err < 0){
        free_as(*s);
		return err;
	}

	return 0;
}

void free_as(audio_settings *s)
{
    free(s);
}