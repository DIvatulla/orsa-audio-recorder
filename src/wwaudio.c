#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>	

unsigned int sizeof_pcm_format(snd_pcm_format_t pfmt)
{
	return (snd_pcm_format_physical_width(pfmt) / 8);
}

unsigned int pcm_byte_per_frame(unsigned int ch, snd_pcm_format_t pfmt)
{
	return sizeof_pcm_format(pfmt) * ch;
}

unsigned int pcm_byte_per_second(
	unsigned int sr,
	unsigned int ch,
	snd_pcm_format_t pfmt)
{
	return pcm_byte_per_frame(ch, pfmt) * sr;
}

int init_as(audio_settings *s, unsigned int r, int ch, snd_pcm_format_t pcm_f)
{
	if (s == NULL) {
        fprintf(stderr, "Pointer to settings is null\n");
        return -1;
    }

	if (r < 8000){
		fprintf(stderr, "This sample rate is less than 8000 (old telephone record quality)\n");
		return -1;
	}
	s->rate = r;

	if (ch < 1){
		fprintf(stderr, "Amount of channels is less than one\n");
		return -1;
	}
	s->channels = ch;

	if ((pcm_f == SND_PCM_FORMAT_UNKNOWN) || (pcm_f > SND_PCM_FORMAT_LAST)){
		fprintf(stderr, "Unknown pcm format\n");
		return -1;
	}
	s->pcm_format = pcm_f;

	return 0;
}

int make_as(audio_settings **s, unsigned int r, int ch, snd_pcm_format_t pcmf)
{
    int err;
	
	*s = (audio_settings*)calloc(1, sizeof(audio_settings));
	if (*s == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings\n");
		return -1;
	}

	err = init_as(*s, r, ch, pcmf);
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

int init_ad(audio_device *d, char *name, audio_settings *s, snd_pcm_stream_t mode)
{
	int err;

	d->name = (char*)calloc((strlen(name)+1), sizeof(char));
	if (d->name == NULL){
		return -1;
	}
	strcpy(d->name, name);

    err = snd_pcm_open(&(d->handle), d->name, mode, 0);
    if (err < 0) {
        fprintf(stderr, "Can't open device %s\n", d->name);
		return err;
    }

    err = snd_pcm_hw_params_malloc(&(d->params));
    if (err < 0){
		fprintf(stderr, "Error while allocating memory for device's parameters\n");
		return err;
	}

	err = snd_pcm_hw_params_any(d->handle, d->params);
    if (err < 0){
        fprintf(stderr, "Can't get available parameters for device %s\n", d->name);
        return err;
    }

    err = snd_pcm_hw_params_set_access(d->handle, d->params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0){
		fprintf(stderr, "Can't set access %s\n", d->name);
		return err;
	}

    err = snd_pcm_hw_params_set_format(d->handle, d->params, s->pcm_format);
	if (err < 0){
		fprintf(stderr, "Can't set pcm-frame's byte representation %s\n", d->name);
		return err;
	}

	err = snd_pcm_hw_params_set_rate(d->handle, d->params, s->rate, 0);
	if (err < 0){
		fprintf(stderr, "Can't set rate on %s to %d\n",
            d->name, s->rate);
		return err;
	}
    
    err = snd_pcm_hw_params_set_channels(d->handle, d->params, s->channels); //mono/stereo
	if (err < 0) {
		fprintf(stderr, "Can't set channels on %s\n", d->name);
		return err;
	}

	err = snd_pcm_hw_params(d->handle, d->params);
	if (err < 0){
		fprintf(stderr, "Can't set params: %s\n", snd_strerror(err));
	}

    return 0;
}

int make_ad(audio_device **d, char *name, audio_settings *s, snd_pcm_stream_t mode)
{
	int err = 9;

	*d = (audio_device*)calloc(1, sizeof(audio_device));
	if (*d == NULL){
		fprintf(stderr, "Can't malloc device\n");
		return -1;
	}

	err = init_ad(*d, name, s, mode);
	if (err < 0){
		free_ad(*d);
		return err;
	}

	return 0;
}

audio_buffer *rec_ad(audio_device *d, int sample_amount)
{
	audio_buffer *rb = NULL;
	int ret, err;

	err = make_ab(
		&rb,
        get_rate_ad(d),
        get_chan_ad(d), 
        get_pfmt_ad(d),
        am_sample,
		sample_amount
	);
	if (err < 0){
		fprintf(stderr, "rec_ad: make_ab fail\n");
		return NULL;
	}

	printf("snd_pcm_bytes_to_frames(d->handle, rb->size) = %d\n",  snd_pcm_bytes_to_frames(d->handle, rb->size));
	ret = snd_pcm_readi(
		d->handle, 
		rb->buf, 
		snd_pcm_bytes_to_frames(d->handle, rb->size)
	);
	if (ret == -EPIPE){
		snd_pcm_prepare(d->handle);
	}
	else if (ret < 0){
		fprintf(stderr, "Error while recording\n");	
		free_ab(rb);
		return NULL;
	}

	return rb;
}

int play_ad(audio_device *d, audio_buffer *pb)
{
	int err;
	snd_pcm_writei(d->handle, pb->buf, get_sample_size_ab(pb));
}

unsigned int get_rate_ad(audio_device *d)
{
	unsigned int rate = 0;
	snd_pcm_hw_params_get_rate(d->params, &rate, 0);
	return rate;
}

unsigned int get_chan_ad(audio_device *d)
{
	unsigned int channels = 0;
	snd_pcm_hw_params_get_channels(d->params, &channels);
	return channels;
}

snd_pcm_format_t get_pfmt_ad(audio_device *d)
{
	snd_pcm_format_t format = 0;
	snd_pcm_hw_params_get_format(d->params, &format);
	return format;
}

void free_ad(audio_device *d)
{
	d->name ? free(d->name) : 0;
	d->params ? snd_pcm_hw_params_free(d->params) : 0;
    d->handle ? snd_pcm_close(d->handle) : 0;
	free(d);
}

int init_ab(
	audio_buffer *ab, 
	unsigned int sr,
	unsigned int ch,
	snd_pcm_format_t pfmt, 
	audio_measure mu, 
	int mod)
{
	ab->sample_rate = sr;
	ab->channels = ch;
	ab->pcm_format = pfmt;
	ab->size = calc_buf_size_ab(ab, mu, mod);
	ab->wi = 0;
	ab->ri = 0;

	if (ab->size <= 0){
		return -1;
	}

	ab->buf = (unsigned char*)calloc(ab->size, sizeof(char));
	if (ab->buf == NULL) {
		fprintf(stderr, "Error while allocating memory for audio buffer\n");
		return -1;
	}

	return 0;
}

int make_ab(
	audio_buffer **ab, 
	unsigned int sr,
	unsigned int ch,
	snd_pcm_format_t pfmt,
	audio_measure mu, 
	int mod)
{
	int err;

	*ab = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	if ((*ab) == NULL){
		fprintf(stderr, "Can't malloc audio buffer\n");
		return -1;
	}

	err = init_ab(*ab, sr, ch, pfmt, mu, mod);
	if (err < 0){
		fprintf(stderr, "Can't init audio buffer");
		free_ab(*ab);
		return err;
	}

	return 0;
}

unsigned int calc_buf_size_ab(audio_buffer *ab, audio_measure mu, int mod)
{
	switch (mu){
		case am_sec:
			return pcm_byte_per_second(
				ab->sample_rate, 
				ab->channels, 
				ab->pcm_format) * mod;
			break;
		case am_usec:
			return (pcm_byte_per_second(
				ab->sample_rate, 
				ab->channels, 
				ab->pcm_format) * mod) / 1000;
			break;
		case am_sample:
			return sizeof_pcm_format(ab->pcm_format) * mod;
			break;
		case am_frame:
			return pcm_byte_per_frame(ab->channels, ab->pcm_format) * mod;
			break;
		case am_byte:
			return mod;
			break;
		default:
			fprintf(stderr, "Unknown specifier for audio buffer memory allocation\n");
			return -1;
	}
}

unsigned int get_sample_size_ab(audio_buffer *ab)
{
	return ab->size / sizeof_pcm_format(ab->pcm_format);
}

unsigned int get_cur_sample_size_ab(audio_buffer *ab)
{
	return ab->wi / sizeof_pcm_format(ab->pcm_format);
}

unsigned int get_frame_size_ab(audio_buffer *ab)
{
	return ab->size / pcm_byte_per_frame(ab->channels, ab->pcm_format);
}

unsigned int get_cur_frame_size_ab(audio_buffer *ab)
{
	return ab->wi / pcm_byte_per_frame(ab->channels, ab->pcm_format);
}


int push_ab(audio_buffer *to, audio_buffer *from)
{
	if (from->size >= to->size) {
		fprintf(stderr, "Size of source is more than destinantion's\n");
		return -1;
	}

	from->ri = 0;
	for (from->ri = 0; from->ri != from->size; ++to->wi, ++from->ri) {
		if (to->wi > to->size){
			to->wi = 0;
		}

		to->buf[to->wi] = from->buf[from->ri];
	}

	return 0;
}

void free_ab(audio_buffer *ab)
{
	ab->buf ? free(ab->buf) : 0;
    free(ab);
}
