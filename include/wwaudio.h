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
	am_sec = 0, //count frames by second
	am_sample = 1,
	am_frame = 2 //count frames by sample rate and amoutn of channels
} audio_measure;

typedef struct {
	unsigned char *buf;
	int size;
	int wi;	//write index
	int ri; //read index
	unsigned int sample_rate;
	unsigned int channels;
	snd_pcm_format_t pcm_format;
} audio_buffer;

unsigned int sizeof_pcm_format(snd_pcm_format_t pfmt);
unsigned int pcm_byte_per_frame(unsigned int ch, snd_pcm_format_t pfmt);
unsigned int pcm_byte_per_second(
	unsigned int sr,
	unsigned int ch,
	snd_pcm_format_t pfmt
);

int init_as(audio_settings *s, unsigned int r, int ch, snd_pcm_format_t pcm_f);
int make_as(audio_settings **s, unsigned int r, int ch, snd_pcm_format_t pcm_f);
void free_as(audio_settings *s);

int init_ad(audio_device *d, char *name, audio_settings *s, snd_pcm_stream_t mode);
int make_ad(audio_device **d, char *name, audio_settings *s, snd_pcm_stream_t mode);
unsigned int get_rate_ad(audio_device *d);
unsigned int get_chan_ad(audio_device *d);
snd_pcm_format_t get_pfmt_ad(audio_device *d);
void free_ad(audio_device *d);

int init_ab(
	audio_buffer *ab, 
	unsigned int sr,
	unsigned int ch,
	snd_pcm_format_t pfmt, 
	audio_measure mu, 
	int mod
);
int make_ab(
	audio_buffer **ab, 
	unsigned int sr,
	unsigned int ch,
	snd_pcm_format_t pfmt,
	audio_measure mu, 
	int mod
);
unsigned int calc_buf_size_ab(audio_buffer *ab, audio_measure mu, int mod);
unsigned int get_sample_size_ab(audio_buffer *ab);
unsigned int get_cur_sample_size_ab(audio_buffer *ab);
unsigned int get_frame_size_ab(audio_buffer *ab);
unsigned int get_cur_frame_size_ab(audio_buffer *ab);
int push_ab(audio_buffer *to, audio_buffer *from);
void free_ab(audio_buffer *ab);

#endif