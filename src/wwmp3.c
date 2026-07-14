#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <lame/lame.h>

int init_enc(lame_enc *enc, audio_device *d, int br, mp3_quality q)
{
    unsigned int rate = 0;
    unsigned int channels = 0;

    snd_pcm_hw_params_get_rate(d->params, &rate, 0);
    snd_pcm_hw_params_get_channels(d->params, &channels);
    
    enc->lame = lame_init();
    lame_set_in_samplerate(enc->lame, rate);
    lame_set_num_channels(enc->lame, channels);
    lame_set_brate(enc->lame, br);

    if (lame_init_params(enc->lame) < 0) {
        fprintf(stderr, "Failed to initialize LAME\n");
        return -1;
    }

    enc->bitrate = br;
    enc->quality = q;

    return 0;
}

int encode(lame_enc *enc, lame_mp3_buf *lbmp3, audio_device *d, audio_buffer *ab, int *fsz)
{
    *fsz = lame_encode_buffer(
        enc->lame,
        (short*)ab->buf,
        NULL,
        snd_pcm_bytes_to_samples(d->handle, ab->wi),
        lbmp3->buf,
        lbmp3->size
    );

    if ((*fsz) < 0) {
        fprintf(stderr, "Encoding error:\n");
        return -1;
    }

    return 0;
}

int make_enc(lame_enc **enc, audio_device *recdev, mp3_quality q)
{
    int err;

    *enc = (lame_enc*)calloc(1, sizeof(lame_enc));
	if ((*enc) == NULL){
		fprintf(stderr, "Can't malloc lame_enc\n");
		return -1;
	}

	err = init_enc(*enc, recdev, 128, q);
    if (err < 0){
        free_enc(*enc);
        return -1;
    }

	return 0;
}

void free_enc(lame_enc *enc)
{
    lame_close(enc->lame);
    free(enc);
}

int init_lame_mp3_buf(lame_mp3_buf *lbmp3, audio_buffer *ab)
{
    lbmp3->size = ab->size + ((ab->size / 4) + 1) + 7200;
    lbmp3->buf = (unsigned char*)calloc(lbmp3->size, sizeof(char));
    if (lbmp3 == NULL){
        fprintf(stderr, "Can't malloc lame_mp3_buf->buf\n");
        return -1;
    }

    return 0;
}

int make_lame_mp3_buf(lame_mp3_buf **lbmp3, audio_buffer *ab)
{
    int err;

    *lbmp3 = (lame_mp3_buf*)calloc(1, sizeof(lame_mp3_buf));
    if ((*lbmp3) == NULL){
        fprintf(stderr, "Can't malloc lame_mp3_buf struct\n");
        return -1;
    }

    (*lbmp3)->buf = NULL;
    (*lbmp3)->size = 0;

    err = init_lame_mp3_buf(*lbmp3, ab);
    if (err < 0){
        free_lame_mp3_buf(*lbmp3);
        return -1;
    }

    return 0;
}

void free_lame_mp3_buf(lame_mp3_buf *lbmp3)
{
    if (lbmp3->buf != NULL){
        free(lbmp3->buf);
    }
    free(lbmp3);
}

int init_dec(mpeg_dec *dec)
{
    int err;

    dec->handle = mpg123_new(NULL, &err);
    if (dec->handle == NULL){
        fprintf(stderr, "Can't create decoder\n");
        return -1;
    }
    
    dec->rate = 0;
    dec->channels = 0;
    dec->pcm_format = MPG123_ENC_16; //0000 0000 0100 0000

    return 0;
}

int make_dec(mpeg_dec **dec)
{
    int err;

	*dec = (mpeg_dec*)calloc(1, sizeof(mpeg_dec));
	if ((*dec) == NULL) {
		fprintf(stderr, "can't malloc mpg123 decoder\n");
		return -1;
	}
	err = init_dec(*dec);
	if (err < 0){
		return err;
	}

	return 0;
}

int decode_mp3_settings(mpeg_dec *dec, lame_mp3_buf *lbmp3)
{
    int ret, res;
    audio_buffer *pcmb;
    size_t done;

    pcmb = (audio_buffer*)calloc(1, sizeof(audio_buffer));

    mpg123_open_feed(dec->handle);
    mpg123_feed(dec->handle, lbmp3->buf, lbmp3->size);
    pcmb->size = mpg123_outblock(dec->handle);
    pcmb->buf = (unsigned char*)calloc(pcmb->size, sizeof(char));

    printf("pcm buffer's size %d\n", pcmb->size);

    while ((ret = mpg123_read(dec->handle, pcmb->buf, pcmb->size, &done)) != MPG123_NEED_MORE){
        printf("mp3 read\n");
        if (ret == MPG123_NEW_FORMAT) {
            mpg123_getformat(dec->handle, &dec->rate, &dec->channels, &dec->pcm_format);
            printf("%ld Hz, %d channels, %d pcm_format\n", dec->rate, dec->channels, MPG123_SAMPLESIZE(dec->pcm_format));
            res = 0;
            goto cleanup;
        }
    }
    res = -1;
    fprintf(stderr, "Can't get settings for decoding this mp3 file\n");

cleanup:        
    free_ab(pcmb);
    mpg123_close(dec->handle);

    return res;
}

void free_dec(mpeg_dec *dec)
{
    mpg123_close(dec->handle);
    mpg123_delete(dec->handle);
    free(dec);
}

