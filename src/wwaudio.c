#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

int usec_to_frames(audio_settings *s, int usec)
{
	int uspf = (int)floorf(((1000000.0f) / (float)s->sample_rate)); //microseconds per frame

	if (usec < uspf) {
		return -1;
	}
	else {
		return (usec / uspf) + (usec % uspf);
	}
}

//add check for available sample rate
int init_as(audio_settings *s, char *dn, unsigned int sr, int c, snd_pcm_format_t pf)
{
	char *tmp;

	if (strlen(dn) < 3){
		fprintf(stderr, "Device name is too short\n");
		return -1;
	}

	tmp = calloc(strlen(dn), sizeof(char));
	if (tmp == NULL){
		fprintf(stderr, "Couldn't allocate memory for device name");	
		return -1;
	}

	strcpy(tmp, dn);
	s->device_name = tmp;
	s->sample_rate = sr;
	s->channels = c;
	s->pcm_format = pf;
	s->bytes_per_frame = (snd_pcm_format_width(s->pcm_format) / 8) * (s->channels);

	return 0;
}

int open_mic(snd_pcm_t **d, audio_settings *s)
{
	snd_pcm_hw_params_t *p; 
	int err;

	err = snd_pcm_open(d, s->device_name, SND_PCM_STREAM_CAPTURE, 0); //open ALSA connection
    if (err < 0) {
        fprintf(stderr, "Can't open device %s\n", s->device_name);
    }
	err = snd_pcm_hw_params_malloc(&p); //allocate memory for possible parameters of device
	if (err < 0) {
		fprintf(stderr, "Error while allocating memory for device's parameters %s\n", s->device_name);
	}
	err = snd_pcm_hw_params_any(*d, p); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't get available parameters for device %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_access(*d, p, SND_PCM_ACCESS_RW_INTERLEAVED); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't set access %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_format(*d, p, s->pcm_format); //get available parameters for device
	if (err < 0) {
		fprintf(stderr, "Can't set pcm frame-byte-format %s\n", s->device_name);

	}
	err = snd_pcm_hw_params_set_rate_near(*d, p, &s->sample_rate, 0); //setting the sample rate
	if (err < 0) {
		fprintf(stderr, "Can't set sample rate on %s to %d\n", s->device_name, s->sample_rate);
	}
	err = snd_pcm_hw_params_set_channels(*d, p, s->channels); //mono/stereo
	if (err < 0) {
		fprintf(stderr, "Can't get available parameters for device %s\n", s->device_name);
	}
	err = snd_pcm_hw_params(*d, p);
    if (err < 0) {
        fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
    }

	snd_pcm_hw_params_free(p);
	return err;
}

int set_ab_as(audio_buffer *b, audio_settings *s)
{
	if (b == NULL){
		return -1;
	}
	if (s == NULL){
		return -1;
	}
	memcpy(b->settings, s, sizeof(audio_settings));
	return 0;
}

int set_ab_size(audio_buffer *b, audio_settings *s, audio_measure mu, int mod) 
{
	switch (mu){
		case am_usecs:
			b->size = s->bytes_per_frame * usec_to_frames(s, mod);
			if (b->size < 0){
				fprintf(stderr, "The frame length is too small\n");
				return -1;
			}
			break;
		case am_samples:
			b->size = s->bytes_per_frame * mod;
			break;
		case am_frames:
			b->size = s->bytes_per_frame * mod;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memory allocation\n");
			return -1;
	}

	return 0;
}

int init_ab(audio_buffer *b, audio_settings *s, audio_measure mu, int mod)
{
	int err;

	err = set_ab_as(b, s);
	if (err < 0){
		return err;
	}

	err = set_ab_size(b, s, mu, mod);
	if (err < 0){
		return err;
	}

	b->buf = calloc(b->size, sizeof(char));
	if (b->buf == NULL) {
		fprintf(stderr, "Error while allocating memory for audio buffer");
		return -1;
	}

	b->wi = 0;
	b->ri = 0;

	return 0;
}

int free_ab(audio_buffer *b)
{
	free(b->buf);
	free(b);

	return 0;
}

int free_as(audio_settings *s)
{
	free(s->device_name);
	free(s);

	return 0;
}

int push_ab(audio_buffer *to, audio_buffer *from)
{
	if (from->size >= to->size) {
		fprintf(stderr, "Size of source is more than destinantion's");
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

double rms(audio_buffer *b, int usec)
{	
	long int i, j;
    long long sum = 0;
	short *buf;
	
	j = usec_to_frames(b->settings, usec) / 2;
	if (j > b->wi) {
		return -1.0f;
	}

	buf = (short*)b->buf;
	for (i = b->wi; i > j; i--){
        sum += (long long)buf[i] * buf[i];
		printf("help me - %ld, %d\n", i, buf[i]);
	}

    return sqrt((double)sum / (j / b->settings->channels / 2));
}