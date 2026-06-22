#include <stdio.h>
#include <stdlib.h>
#include <lame/lame.h>
#include "../include/wwaudio.h"

int init_enc(lame_enc *enc, audio_device *d, int br, mp3_quality q)
{
    unsigned int rate = 0;
    int channels = 0;
    snd_pcm_hw_params_get_rate(d->params, &rate, 0);
    snd_pcm_hw_params_get_channels(d->params, &channels);
    
    enc->lame = lame_init();
    lame_set_in_samplerate(enc->lame, rate);
    lame_set_num_channels(enc->lame, channels);
    lame_set_brate(enc->lame, bitrate);

    if (lame_init_params(lame) < 0) {
        fprintf(stderr, "Failed to initialize LAME\n");
        return -1;
    }

    enc->bitrate = br;
    enc->quality = q;
}

int encode(lame_enc *enc, lame_mp3_buf *lbmp3, audio_device *d, audio_buffer *ab)
{
    int mp3_bytes = lame_encode_buffer(
        lame_enc->lame,
        ab->buf,
        NULL,
        snd_pcm_bytes_to_samples(d->handle, ab->size),
        lbmp3->buf,
        lbmp3->size
    );

    if (mp3_bytes < 0) {
        fprintf(stderr, "Encoding error: %d\n", mp3_bytes);
        return -1;
    }

    return 0;
}

int init_lame_mp3_buf(lame_mp3_buf *lbmp3, audio_buffer *ab)
{
    lbmp3->size = ab->size + ((ab->size / 4) + 1) + 7200;
    lbmp3->buf = (lame_mp3_buf*)calloc(lbmp3->size, sizeof(char));
    if (!lbmp3){
        return -1;
    }

    return 0;
}

void free_lame_mp3_buf(lame_mp3_buf *lbmp3)
{
    free(lbmp3->buf);
    free(lbmp3);
}

