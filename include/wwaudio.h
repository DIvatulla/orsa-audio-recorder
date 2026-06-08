#ifndef WWAUDIO_H
#define WWAUDIO_H

#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef enum {
	am_usecs = 1, //count frames by second
	am_samples = 2,
	am_frames = 3 //count frames by sample rate and amoutn of channels
} audio_measure;

typedef struct {
	char *buf;
	int size;
	audio_settings *settings;
	int wi;	//write index
	int ri; //read index
} audio_buffer;

typedef struct {
	char *device_name;
	unsigned int sample_rate;
	int channels;
	snd_pcm_format_t pcm_format;
	unsigned int bytes_per_frame;
} audio_settings;

int usec_to_frames(audio_settings *s, int usec);
int init_as(audio_settings *s, char *dn, unsigned int sr, int c, snd_pcm_format_t pf);
int free_as(audio_settings *s);
int init_ab(audio_buffer *b, audio_settings *s, audio_measure mu, int mod);
int free_ab(audio_buffer *ab);
int push_ab(audio_buffer *to, audio_buffer *from);
int open_mic(snd_pcm_t **d, audio_settings *s);
double rms(audio_buffer *b, int usec);

#endif