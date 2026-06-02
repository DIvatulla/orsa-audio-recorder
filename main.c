#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>

static char *device = "plughw:0,1";			/* capture device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;	/* sample format */
static unsigned int sample_rate = 16000;			/* stream rate */
static unsigned int channels = 1;			/* count of channels */

static snd_pcm_t *handle; //api connection
static snd_pcm_hw_params_t *params; //parameters

int main(int argc, char** argv)
{    
	int err;
    
    int buffer_time = 5;
    int total_frames = sample_rate * buffer_time;
    int bytes_per_frame = 2;
    char *buf = calloc(total_frames * bytes_per_frame, sizeof(char));

    err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0) //open ALSA connection
    if (err < 0) {
        fprintf(stderr, "Can't open device %s\n", device);
        exit(1);
    }

    snd_pcm_hw_params_malloc(&params); //allocates memory fow device parameters
    snd_pcm_hw_params_any(handle, params); //get device parameters
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, format);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, 0);
    
    err = snd_pcm_hw_params(handle, params);
    
    if (err < 0) {
        fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
        return 1;
    }

    return 0;
}
