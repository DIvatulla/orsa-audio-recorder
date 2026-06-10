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
	char *device_name;
	unsigned int sample_rate;
	int channels;
	snd_pcm_format_t pcm_format;
	unsigned int bytes_per_sample;
} audio_settings;

typedef struct {
	char *buf;
	int size;
	audio_settings *settings;
	int wi;	//write index
	int ri; //read index
} audio_buffer;

typedef struct {
	snd_pcm_t *pcm_device;
	audio_settings *settings;
} audio_device;

int usec_to_samples(audio_settings *s, int usec);
int usec_per_sample(audio_settings *s);
int samples_to_usec(audio_settings *s, int frames);

int init_as(audio_settings *s, char *dn, unsigned int sr, int c, snd_pcm_format_t pf);
int copy_as(audio_settings *dst, audio_settings *src);
void free_as(audio_settings *s);

int init_ab(audio_buffer *b, audio_settings *s, audio_measure mu, int mod);
int set_ab_size(audio_buffer *b, audio_settings *s, audio_measure mu, int mod);
void free_ab(audio_buffer *ab);
int push_ab(audio_buffer *to, audio_buffer *from);

int init_ad(audio_device *d, audio_settings *s);
void free_ad(audio_device *d);

int open_mic(audio_device *d);
double rms(audio_buffer *b, int usec);

#endif