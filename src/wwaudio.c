#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

int usec_per_sample(audio_settings *s)
{
	return (int)floorf(((1000000.0f) / (float)s->sample_rate));
}

int usec_to_samples(audio_settings *s, int usec)
{
	int uspf = usec_per_sample(s);

	if (usec < uspf) {
		return -1;
	}
	else {
		return (usec / uspf) + (usec % uspf);
	}
}

int samples_to_usec(audio_settings *s, int samples)
{
	return samples * usec_per_sample(s);
}

//add check for available sample rate
int init_as(audio_settings *s, char *dn, unsigned int sr, int c, snd_pcm_format_t pf)
{
	if (strlen(dn) < 3){
		fprintf(stderr, "Device name is too short\n");
		return -1;
	}

	s->device_name = (char*)calloc(strlen(dn)+1, sizeof(char));
	if (s->device_name == NULL){
		fprintf(stderr, "Couldn't allocate memory for device name");	
		return -1;
	}	
	
	strcpy(s->device_name, dn);
	s->sample_rate = sr;
	s->channels = c;
	s->pcm_format = pf;
	s->bytes_per_sample = (snd_pcm_format_width(s->pcm_format) / 8) * (s->channels);

	return 0;
}

void free_as(audio_settings *s)
{
	free(s->device_name);
	free(s);
}

int init_ab(audio_buffer *b, audio_settings *s, audio_measure mu, int mod)
{
	int err;

	if (s == NULL){
		fprintf(stderr, "Audio settings pointer is null");
		return -1;
	}
	b->settings = (audio_settings*)calloc(1, sizeof(audio_settings));
	init_as(b->settings, s->device_name, s->sample_rate, s->channels, s->pcm_format);
	
	err = set_ab_size(b, s, mu, mod);
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

int set_ab_size(audio_buffer *b, audio_settings *s, audio_measure mu, int mod) 
{
	switch (mu){
		case am_usecs:
			b->size = s->bytes_per_sample * usec_to_samples(s, mod);
			if (b->size < 0){
				fprintf(stderr, "The frame length is too small\n");
				return -1;
			}
			break;
		case am_samples:
			b->size = s->bytes_per_sample * mod;
			break;
		case am_frames:
			b->size = s->bytes_per_sample;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memory allocation\n");
			return -1;
	}

	return 0;
}

int push_ab(audio_buffer *to, audio_buffer *from)
{
	if (from->size >= to->size) {
		fprintf(stderr, "Size of source is more than destinantion's\n");
		return -1;
	}

	from->ri = 0;

	while (from->ri != from->size) {
		if (to->wi > to->size){
			to->wi = 0;
		}

		to->buf[to->wi] = from->buf[from->ri];
		++to->wi;
		++from->ri;
	}

	return 0;
}

void free_ab(audio_buffer *b)
{
	free_as(b->settings);
	free(b->buf);
	free(b);
}

int init_ad(audio_device *d, audio_settings *s)
{
	;
}

void free_ad(audio_device *d)
{
	snd_pcm_close(d->pcm_device);
	free_as(d->settings);
	free(d);
}

int open_mic(audio_device *d)
{
	snd_pcm_hw_params_t *p;
	unsigned int old_rate = 0;
	int err = 0;

	err = snd_pcm_open(&(d->pcm_device), d->settings->device_name, SND_PCM_STREAM_CAPTURE, 0);
	if (err < 0) {
		fprintf(stderr, "Can't open device %s\n", d->settings->device_name);
		return err;
	}

	err = snd_pcm_hw_params_malloc(&p);
	if (err < 0){
		fprintf(stderr,
			"Error while allocating memory for device's parameters %s\n",
			d->settings->device_name);
		return err;
	}

	err = snd_pcm_hw_params_any(d->pcm_device, p);
	if (err < 0){
		fprintf(stderr,
			"Can't get available parameters for device %s\n",
			d->settings->device_name);
		return err;
	}

	err = snd_pcm_hw_params_set_access(d->pcm_device, p, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0){
		fprintf(stderr, "Can't set access %s\n", d->settings->device_name);
		return err;
	}

	err = snd_pcm_hw_params_set_format(d->pcm_device, p, d->settings->pcm_format);
	if (err < 0){
		fprintf(stderr, 
			"Can't set pcm-frame's byte representation %s\n", 
			d->settings->device_name);
		return err;
	}

	old_rate = d->settings->sample_rate;
	err = snd_pcm_hw_params_set_rate_near(d->pcm_device, p, &(d->settings->sample_rate), 0);
	if (err < 0){
		fprintf(stderr, 
			"Can't set sample rate on %s to %d\n",
			d->settings->device_name, d->settings->sample_rate);
		return err;
	}
	if (old_rate != d->settings->sample_rate){
		fprintf(stderr, 
			"Can't set sample rate on %s to %d, only %d is available\n",
			d->settings->device_name, old_rate, d->settings->sample_rate);
		return -1;
	}

	err = snd_pcm_hw_params_set_channels(d->pcm_device, p, d->settings->channels); //mono/stereo
	if (err < 0) {
		fprintf(stderr, "Can't set channels on %s\n", d->settings->device_name);
		return err;
	}

	err = snd_pcm_hw_params(d->pcm_device, p);
	if (err < 0){
		fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
	}

	snd_pcm_hw_params_free(p);
	return 0;
}



double rms(audio_buffer *b, int sa)
{	
	int i;
	short *cur;
    long long sum = 0;
	
	printf("bytes to read-%d, write index-%d\n", sa * b->settings->bytes_per_sample, b->wi);
	if ((sa * b->settings->bytes_per_sample) > (unsigned int)b->wi) {
		return -1.0f;
	}

	cur = (short*)(b->buf + (b->wi - (sa * b->settings->bytes_per_sample)));
	for (i = 0; i < sa; i++){
		sum += (long long)(*cur) * (long long)(*cur);
		++cur;
	}

    return sqrt((double)sum / sa);
}