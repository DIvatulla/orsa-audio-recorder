#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <speex/speex_resampler.h>
#include "../include/wwaudio.h"

unsigned int calc_pcm_out_len(SpeexResamplerState *resampler, int in_len)
{
    spx_uint32_t num, den;
    speex_resampler_get_ratio(resampler, &num, &den);
    return (((uint64_t)in_len * den + num - 1) / num) + 1;
}

int init_resampler(
    SpeexResamplerState **resampler, 
    unsigned int in_sr,
    unsigned int in_ch,
    unsigned int out_sr)
{
    int err = 0;
    
    *resampler = speex_resampler_init(
        in_ch, 
        in_sr, 
        out_sr, 
        SPEEX_RESAMPLER_QUALITY_DEFAULT, 
        &err
    );
    
    if (!resampler || err != RESAMPLER_ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize Speex resampler: %d\n", err);
        return -1;
    }

    return 0;
}

int resample_pcm(
    SpeexResamplerState *resampler,
    audio_buffer *in_ab,
    audio_buffer *out_ab)
{
    int err;
    spx_uint32_t in_len = get_sample_size_ab(in_ab);
    spx_uint32_t out_len = get_sample_size_ab(out_ab);
    
    printf("in_len %d\nout_len %d\n", in_len, out_len);
    
    err = speex_resampler_process_float(
        resampler, 
        0, 
        (float*)in_ab->buf, 
        &in_len, 
        (float*)out_ab->buf, 
        &out_len
    );

    if (err != RESAMPLER_ERR_SUCCESS) {
        fprintf(stderr, "Resampling process failed: %d\n", err);
        return -2;
    }

    out_ab->wi = out_len * sizeof_pcm_format(out_ab->pcm_format);

    return 0;
}

int convert_pcm_buf(audio_buffer **ab, int out_sr)
{
    int err = 0;
	audio_buffer *ob;
	SpeexResamplerState *resampler = NULL;

    err = init_resampler(
        &resampler, 
        (*ab)->sample_rate, 
        (*ab)->channels, 
        out_sr
    );
	if (err < 0){
        return err;
    }

    err = make_ab(
        &ob,
        out_sr,
        (*ab)->channels, 
        (*ab)->pcm_format,
        am_frame,
        calc_pcm_out_len(resampler, get_frame_size_ab(*ab))
    );
    if (err < 0){
        return err;
    }

    err = resample_pcm(resampler, *ab, ob);
	if (err < 0){
		free(ob);
		return err;
	}

    speex_resampler_destroy(resampler);
	free_ab(*ab);
	*ab = ob;
    return 0;
}
