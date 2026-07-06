#ifndef WWAUDIO_H
#define WWAUDIO_H

#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
    unsigned int rate;
    unsigned int channels;
    snd_pcm_format_t pcm_format;
} audio_settings;

typedef struct {
	char *name;
    snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
} audio_device;

typedef enum {
	am_secs = 1, //count frames by second
	am_samples = 2,
	am_frames = 3 //count frames by sample rate and amoutn of channels
} audio_measure;

typedef struct {
	unsigned char *buf;
	int size;
	int wi;	//write index
	int ri; //read index
} audio_buffer;

int init_as(audio_settings *s, unsigned int r, int ch, snd_pcm_format_t pcm_f);
void free_as(audio_settings *s);

int init_rd(audio_device *d, audio_settings *s);
int init_pd(audio_device *d, audio_settings *s);
void free_ad(audio_device *d);

int init_ab(audio_buffer *b, audio_device *d, audio_measure mu, int mod);
int set_ab_size(audio_buffer *b, audio_device *d, audio_measure mu, int mod);
unsigned int calc_ab_size(audio_device *d, audio_measure mu, int mod);
int push_ab(audio_buffer *to, audio_buffer *from);
void free_ab(audio_buffer *ab);

double rms(audio_buffer *b, int frames);

#endif