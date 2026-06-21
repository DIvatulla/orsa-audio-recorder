#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>

int init_rd(audio_device *d, audio_settings *s)
{
    int err;
    unsigned int old_rate = 0;

    err = snd_pcm_open(&(d->handle), d->name, SND_PCM_STREAM_CAPTURE, 0);
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

    old_rate = s->rate;
	err = snd_pcm_hw_params_set_rate_near(d->handle, d->params, &(s->rate), 0);
	if (err < 0){
		fprintf(stderr, "Can't set rate on %s to %d\n",
            d->name, s->rate);
		return err;
	}
    if (old_rate != s->rate){
		fprintf(stderr, "Can't set rate on %s to %d, only %d is available\n",
			d->name, old_rate, s->rate);
		return -1;
	}

    err = snd_pcm_hw_params_set_channels(d->handle, d->params, s->channels); //mono/stereo
	if (err < 0) {
		fprintf(stderr, "Can't set channels on %s\n", d->name);
		return err;
	}

	err = snd_pcm_hw_params(d->handle, d->params);
	if (err < 0){
		fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
	}

    return 0;
}

void free_ad(audio_device *d)
{
	free(d->name);
    snd_pcm_hw_params_free(d->params);
    snd_pcm_close(d->handle);
}