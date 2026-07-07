#include "../include/wwaudio.h"
#include "../include/wwmp3.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

audio_device *recdev = NULL; //recording device 
audio_settings *settings = NULL; //audio parameters
int mb_dur = 60;
double bottomline_volume;
audio_device *playdev;

lame_enc *lemp3;
lame_mp3_buf *lbmp3;

mpeg_dec *mdmp3;

int make_settings(audio_settings **s);
int make_record_device(audio_device **d, audio_settings *s);
int make_play_device(audio_device **d, audio_settings *s);
int record(audio_buffer *mb, audio_buffer *rb, double border_vol, int sf_count);
double measure_volume();
int listen(audio_device *d);
void write_file(unsigned char *b, int size, char *filename);
int make_mp3_buffer(lame_mp3_buf **lbuf, audio_buffer *ab);
int make_mp3_encoder(lame_enc **le);
int make_mp3_decoder(mpeg_dec **mdmp3);
void play_mp3(mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb);

int main(int argc, char** argv)
{   
	int err = 0;
	int mp3_filesize;
	audio_buffer *mb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	audio_buffer *rb = (audio_buffer*)calloc(1, sizeof(audio_buffer));

	err = make_settings(&settings);
	if (err < 0){
		return err;
	}
	err = make_record_device(&recdev, settings);
	if (err < 0){
		return err;
	}
	err = make_play_device(&playdev, settings);
	if (err < 0){
		return err;
	}
	err = make_mp3_encoder(&lemp3);
	if (err < 0){
		return err;
	}

	init_ab(mb, recdev, am_secs, 30);
	init_ab(rb, recdev, am_frames, 4096);

	record(mb, rb, 0.0f, 0);
	bottomline_volume = measure_volume(mb);
	if (bottomline_volume < 0) {
		fprintf(stderr, "RMS calculation is -1.0");
		return 1;
	}
	printf("bottom line volume %f\n", bottomline_volume);

	mb->wi = 0;
	record(mb, rb, bottomline_volume, calc_ab_size(recdev, am_secs, 5));
	
	lbmp3 = (lame_mp3_buf*)calloc(1, sizeof(lame_mp3_buf));
	init_lame_mp3_buf(lbmp3, mb);
	encode(lemp3, lbmp3, recdev, mb, &mp3_filesize);
	
	free_ab(mb);
	//free_ab(rb);
	free_ad(recdev);
	free_as(settings);
	free_enc(lemp3);

	make_mp3_decoder(&mdmp3);
	decode_mp3_settings(mdmp3, lbmp3);
	play_mp3(mdmp3, lbmp3, rb);
	free_ab(rb);

	write_file(lbmp3->buf, mp3_filesize, "./out.mp3");
	free_lame_mp3_buf(lbmp3);
	free_ad(playdev);

	return 0;
}

int make_settings(audio_settings **s)
{
	int err;

	*s = (audio_settings*)calloc(1, sizeof(audio_settings));
	if (*s == NULL){
		fprintf(stderr, "Couldn't allocate memory for main audio buffer's settings");
		return -1;
	}
	err = init_as(*s, 44100, 1, SND_PCM_FORMAT_S16_LE);
	if (err < 0){
		return err;
	}

	return 0;
}

int make_record_device(audio_device **d, audio_settings *s)
{
	int err;
	char *device_name = "hw:0";

	*d = (audio_device*)calloc(1, sizeof(audio_device));
	(*d)->name = (char*)calloc(5, sizeof(char));
	strcpy((*d)->name, device_name);
	(*d)->handle = NULL;

	return init_rd(*d, s);
}

int make_play_device(audio_device **d, audio_settings *s)
{
	int err;
	char *device_name = "plughw:2";

	*d = (audio_device*)calloc(1, sizeof(audio_device));
	(*d)->name = (char*)calloc(5, sizeof(char));
	strcpy((*d)->name, device_name);
	(*d)->handle = NULL;

	return init_pd(*d, s);
}

int make_mp3_encoder(lame_enc **le)
{
	int err;

	*le = (lame_enc*)calloc(1, sizeof(lame_enc));
	if ((*le) == NULL){
		fprintf(stderr, "can't malloc lame_enc\n");
		return -1;
	}
	init_enc(*le, recdev, 128, defaulteq);

	return 0;
}

int make_mp3_buffer(lame_mp3_buf **lbuf, audio_buffer *ab)
{
	int err;
	
	*lbuf = (lame_mp3_buf*)calloc(1, sizeof(lame_mp3_buf));
	if ((*lbuf) == NULL){
		fprintf(stderr, "can't malloc lame_mp3_buf\n");
		return -1;
	}
	err = init_lame_mp3_buf(*lbuf, ab);
	if (err < 0){
		return err;
	}

	return 0;
}

int make_mp3_decoder(mpeg_dec **mdmp3)
{
	int err;

	*mdmp3 = (mpeg_dec*)calloc(1, sizeof(mpeg_dec));
	if ((*mdmp3) == NULL) {
		fprintf(stderr, "can't malloc mpg123 decoder\n");
		return -1;
	}
	err = init_dec(*mdmp3);
	if (err < 0){
		return err;
	}

	return 0;
}

int record(audio_buffer *mb, audio_buffer *rb, double border_vol, int sf_count)
{
	int old_sf_count = sf_count;
	double frame_vol = 0.0f;
	int ret;

	do {
		ret = snd_pcm_readi(recdev->handle, rb->buf, snd_pcm_bytes_to_frames(recdev->handle, rb->size));
		if (ret == -EPIPE){
			snd_pcm_prepare(recdev->handle);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi += snd_pcm_bytes_to_frames(recdev->handle, rb->size);
		push_ab(mb, rb);

		if (old_sf_count > 0){
			rb->ri = 0;
			frame_vol = rms(rb, 4096);
			printf("rms - %f\n", frame_vol);
			if (frame_vol > (border_vol + 15.0f)) {
				sf_count = old_sf_count;
				printf("sound\n");
			}
			else{
				sf_count -= 4096;
				printf("silence\n");
				printf("sf_count %d\n", sf_count);
			}
			if (sf_count < 0){
				break;
			}
		}

		printf("rec is going; mb->wi = %d, (mb->size - rb->size) = %d\n", mb->wi, mb->size - rb->size);
	} while(mb->wi <= (mb->size - rb->size));

	return 0;
}

int listen(audio_device *d)
{
	double curms;
	int ret;

	audio_buffer *rb = (audio_buffer*)calloc(1, sizeof(audio_buffer));
	init_ab(rb, d, am_frames, 4096);

	for (;;) {
		rb->wi = 0;
		ret = snd_pcm_readi(d->handle, rb->buf, snd_pcm_bytes_to_frames(recdev->handle, 4096));
		if (ret == -EPIPE){
			snd_pcm_prepare(d->handle);
		}
		else if (ret < 0){
			fprintf(stderr, "Error while recording\n");	
			break;
		}

		rb->wi += (int)snd_pcm_frames_to_bytes(d->handle, 4096);
		rb->ri = 0;

		curms = rms(rb, 4096);
		printf("current rms - %f\n", curms);
		if (curms >= (bottomline_volume + 10.0f)){
			printf("sound\n");
		}
		else{
			printf("silence\n");
		}
	}
}


void write_file(unsigned char *b, int size, char *filename)
{
	FILE *f = fopen(filename, "wb");
    if (!f) { 
		fprintf(stderr, "fopen");
	}
    fwrite(b, sizeof(char), size, f);
    fclose(f);

    printf("Saved\n");
}

double measure_volume(audio_buffer *ab)
{
	double cur_rms;
	unsigned int i = 0;
	long double sum = 0.0f;

	ab->ri = 0;	
	while (ab->ri < (ab->wi - (ab->wi % (int)snd_pcm_frames_to_bytes(recdev->handle, 4096)))){
		cur_rms = rms(ab, 4096);
		sum += cur_rms;
		printf("rms - %f\n", cur_rms);
		++i;
	}
	
	return (double)(sum / i);
}

void play_mp3(mpeg_dec *mdmp3, lame_mp3_buf *lb, audio_buffer *rb)
{
	int ret;
	size_t done;

	mpg123_open_feed(mdmp3->handle);
    mpg123_feed(mdmp3->handle, lb->buf, lb->size);
	
	snd_pcm_prepare(playdev->handle);
	while ((ret = mpg123_read(mdmp3->handle, rb->buf, rb->size, &done)) == MPG123_OK){
		snd_pcm_writei(playdev->handle, rb->buf, 4096);
	}
	snd_pcm_drain(playdev->handle);
}