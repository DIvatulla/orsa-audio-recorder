#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>

static char *device_name = "hw:1,0";			/* capture device */
static unsigned int sample_rate = 16000;			/* stream rate */
static unsigned int channels = 2;			/* count of channels */

static snd_pcm_t *device; //audio connection
static snd_pcm_hw_params_t *params; //parameters

int main(int argc, char** argv)
{    
	int err;
    char *buf = calloc(0, 60 * (16000 * ((snd_pcm_format_width(SND_PCM_FORMAT_S16_LE) / 8) * 2)));
	printf("buf size %ld\n", sizeof(buf));

    err = snd_pcm_open(&device, device_name, SND_PCM_STREAM_CAPTURE, 0); //open ALSA connection
    if (err < 0) {
        fprintf(stderr, "Can't open device %s\n", device);
        exit(1);
    }

    snd_pcm_hw_params_malloc(&params); //allocates memory fow device parameters
    snd_pcm_hw_params_any(device, params); //get device parameters
    snd_pcm_hw_params_set_access(device, params, SND_PCM_ACCESS_RW_INTERLEAVED);//settings for samples to be interleaved
    snd_pcm_hw_params_set_format(device, params, SND_PCM_FORMAT_S16_LE);//set format for PCM frames to be written as 16bit le
    snd_pcm_hw_params_set_channels(device, params, channels);// Mono - 1, Stereo - 2
    snd_pcm_hw_params_set_rate_near(device, params, &sample_rate, 0); //set PCM frames rate to 16khz
    
    err = snd_pcm_hw_params(device, params);
    if (err < 0) {
        fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
        return 1;
    }

	printf("recording\n");
	for (int i = 0; i < 10 ; i++) {
		int ret = snd_pcm_readi(device, buf + (i * 2), 4096);  
		if (ret == -EPIPE){
			snd_pcm_prepare(device);
		}
		else if (ret < 0){
			printf("Error\n");	
			exit(1);
		}
	}

    return 0;
}


