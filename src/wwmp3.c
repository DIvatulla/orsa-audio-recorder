#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <lame/lame.h>

int init_enc(lame_enc *enc,
    unsigned int sr,
    unsigned int ch,
    unsigned int br,
    mp3_quality q)
{   
    enc->lame = lame_init();
    lame_set_in_samplerate(enc->lame, sr);
    lame_set_num_channels(enc->lame, ch);
    lame_set_brate(enc->lame, br);

    if (lame_init_params(enc->lame) < 0) {
        fprintf(stderr, "Failed to initialize LAME\n");
        return -1;
    }

    enc->bitrate = br;
    enc->quality = q;

    return 0;
}

int encode(lame_enc *enc, lame_mp3_buf *lbmp3, audio_buffer *ab)
{
    int fsz = 0;
    
    if (ab->channels == 1){
        fsz = lame_encode_buffer_ieee_float(
            enc->lame,
            (float*)ab->buf,
            NULL,
            get_cur_frame_size_ab(ab),
            lbmp3->buf,
            lbmp3->size
        );
    }
    else if (ab->channels == 2){
        fsz = lame_encode_buffer_interleaved_ieee_float(
            enc->lame,
            (float*)ab->buf,
            get_cur_frame_size_ab(ab),
            lbmp3->buf,
            lbmp3->size
        );
    }
    else{
        fprintf(stderr, "Amount of channels %d is unsupported\n", ab->channels);
        return -1;
    }

    if (fsz < 0) {
        fprintf(stderr, "Encoding error:\n");
        return -1;
    }

    lbmp3->wi = fsz;

    return 0;
}

int make_enc(lame_enc **enc, int sr, int ch, mp3_quality q)
{
    int err;

    *enc = (lame_enc*)calloc(1, sizeof(lame_enc));
	if ((*enc) == NULL){
		fprintf(stderr, "Can't malloc lame_enc\n");
		return -1;
	}

	err = init_enc(*enc, sr, ch, 128, q);
    if (err < 0){
        free_enc(*enc);
        return -1;
    }

	return 0;
}

void free_enc(lame_enc *enc)
{
    enc->lame ? lame_close(enc->lame) : 0;
    free(enc);
}


int init_lame_mp3_buf(lame_mp3_buf *lbmp3, audio_buffer *ab)
{
    unsigned int framelen = get_frame_size_ab(ab);

    lbmp3->size = (framelen / 4) + framelen + 7200;
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

    err = init_lame_mp3_buf(*lbmp3, ab);
    if (err < 0){
        free_lame_mp3_buf(*lbmp3);
        return -1;
    }

    return 0;
}

void free_lame_mp3_buf(lame_mp3_buf *lbmp3)
{
    lbmp3->buf ? free(lbmp3->buf) : 0;
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

