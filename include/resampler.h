#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <speex/speex_resampler.h>
#include "../include/wwaudio.h"

unsigned int calc_pcm_out_len(SpeexResamplerState *resampler, int in_len);
int init_resampler(
    SpeexResamplerState **resampler, 
    unsigned int in_sr,
    unsigned int in_ch,
    unsigned int out_sr
);
int init_resampler(
    SpeexResamplerState **resampler, 
    unsigned int in_sr,
    unsigned int in_ch,
    unsigned int out_sr);
int resample_pcm(
    SpeexResamplerState *resampler,
    audio_buffer *in_ab,
    audio_buffer *out_ab
);
int convert_44100_16000(audio_buffer **ab);
