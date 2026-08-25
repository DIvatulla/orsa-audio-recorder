#ifndef WWAUDIO_H
#define WWAUDIO_H

#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
    int rate;
    int channels;
    snd_pcm_format_t pcm_format;
} audio_settings;

typedef struct {
	char *name;
    snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
} audio_device;

typedef enum {
	am_sec, //count frames by second
	am_msec,
	am_sample,
	am_frame, //count frames by sample rate and amoutn of channels
	am_byte
} audio_measure;

typedef struct {
	unsigned char *buf;
	int size;
	int wi;	//write index
	int ri; //read index
	int sample_rate;
	int channels;
	snd_pcm_format_t pcm_format;
} audio_buffer;

int sizeof_pcm_format(snd_pcm_format_t pfmt);
int pcm_byte_per_sample(snd_pcm_format_t pfmt);
int pcm_byte_per_frame(int ch, snd_pcm_format_t pfmt);
int pcm_byte_per_second(
	int sr,
	int ch,
	snd_pcm_format_t pfmt
);

int init_as(audio_settings *s, int r, int ch, snd_pcm_format_t pcm_f);
int make_as(audio_settings **s, int r, int ch, snd_pcm_format_t pcm_f);
void free_as(audio_settings *s);

int init_ad(audio_device *d, char *name, audio_settings *s, snd_pcm_stream_t mode);
int make_ad(audio_device **d, char *name, audio_settings *s, snd_pcm_stream_t mode);
int get_rate_ad(audio_device *d);
int get_chan_ad(audio_device *d);
snd_pcm_format_t get_pfmt_ad(audio_device *d);
void free_ad(audio_device *d);

int init_ab(
	audio_buffer *ab, 
	int sr,
	int ch,
	snd_pcm_format_t pfmt, 
	audio_measure mu, 
	int mod
);
int make_ab(
	audio_buffer **ab, 
	int sr,
	int ch,
	snd_pcm_format_t pfmt,
	audio_measure mu, 
	int mod
);
int calc_buf_size(int sr, int ch, snd_pcm_format_t pfmt, audio_measure mu, int mod);
int convert_buf_size(int s, int sr, int ch, snd_pcm_format_t pfmt, audio_measure mu);
int get_size_ab(audio_buffer *ab, audio_measure mu);
int get_size_ab_cur(audio_buffer *ab, audio_measure mu);
int push_ab(audio_buffer *to, audio_buffer *from);
void free_ab(audio_buffer *ab);

int audio_record(audio_device *d, audio_buffer *ab, int frame_amount);
int audio_play(audio_device *d, audio_buffer *ab);

#endif