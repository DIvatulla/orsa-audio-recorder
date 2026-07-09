#include <stdio.h>
#include <stdlib.h>
#include <lame/lame.h>
#include <mpg123.h>
#include "../include/wwaudio.h"

typedef struct{
    lame_t lame;
    int bitrate;
    int quality;
} lame_enc;

typedef struct {
    unsigned char *buf;
    int size;
} lame_mp3_buf;

typedef enum {
    endeq = 0, //best quality, slowest
    hieq = 2, //very high quality
    defaulteq = 5, //mid
    lossyeq = 7, //fast
    badeq = 9 //fastest
} mp3_quality;

typedef struct{
    mpg123_handle *handle;
    long int rate;
    int channels;
    int pcm_format; //mpg123_enc_enum type
} mpeg_dec;

int init_enc(lame_enc *enc, audio_device *d, int br, mp3_quality q);
int encode(lame_enc *enc, lame_mp3_buf *lbmp3, audio_device *d, audio_buffer *ab, int *fsz);
int make_enc(lame_enc **enc, audio_device *recdev, mp3_quality q);
void free_enc(lame_enc *enc);

int init_lame_mp3_buf(lame_mp3_buf *lbmp3, audio_buffer *ab);
int make_lame_mp3_buf(lame_mp3_buf **lbmp3, audio_buffer *ab);
void free_lame_mp3_buf(lame_mp3_buf *lbmp3);

int init_dec(mpeg_dec *dec);
int decode_mp3_settings(mpeg_dec *dec, lame_mp3_buf *lbmp3);