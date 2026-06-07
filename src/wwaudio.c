#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
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

audio_settings *init_as(char *dn, unsigned int sr, int c, snd_pcm_format_t pf)
{
	audio_settings *s = calloc(1, sizeof(audio_settings));
	if (s == NULL) {
		fprintf(stderr, "Couldn't allocate memory for audio settings");
		return NULL;
	}

	s->device_name = dn;
	s->sample_rate = sr;
	s->channels = c;
	s->pcm_format = pf;
	s->bytes_per_frame = (snd_pcm_format_width(s->pcm_format) / 8) * (s->channels);

	return s;
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

audio_buffer *init_ab(audio_settings *s, audio_measure m, int mod)
{
	audio_buffer *ab;

	ab = calloc(1, sizeof(audio_buffer));
	if (ab == NULL) {
		fprintf(stderr, "Couldn't alloc memory for audio buffer");
		return ab;
	}

	switch (m){
		case am_usecs:
			ab->size = s->bytes_per_frame * usec_to_frames(s, mod);
			if (ab->size < 0){
				fprintf(stderr, "The frame length is too small\n");
				return NULL;
			}
			break;
		case am_samples:
			ab->size = s->bytes_per_frame * mod;
			break;
		case am_frames:
			ab->size = s->bytes_per_frame * mod;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memory allocation\n");
			return NULL;
	}

	ab->buf = calloc(ab->size, sizeof(char));
	if (ab->buf == NULL) {
		fprintf(stderr, "Error while allocating memory for audio buffer");
		return NULL;
	}

	ab->wi = 0;
	ab->ri = 0;

	return ab;
}

int free_ab(audio_buffer *ab)
{
	free(ab->buf);
	free(ab);

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
}

