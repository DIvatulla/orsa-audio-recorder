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

void free_as(audio_settings *s)
{
    free(s);
}