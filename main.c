#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>

static char *device = "plughw:0,0";			/* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;	/* sample format */
static unsigned int rate = 44100;			/* stream rate */
static unsigned int channels = 1;			/* count of channels */
static unsigned int buffer_time = 500000;		/* ring buffer length in us */
static unsigned int period_time = 100000;		/* period time in us */
static double freq = 440;				/* sinusoidal wave frequency in Hz */
static int verbose = 0;					/* verbose flag */
static int resample = 1;				/* enable alsa-lib resampling */
static int period_event = 0;				/* produce poll event after each period */

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;

int main(int argc, char** argv)
{
	int val;

	printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);

	printf("\nPCM stream types:\n");
	for (val = 0; val <= SND_PCM_STREAM_LAST; val++){
		printf("  %s\n", snd_pcm_stream_name((snd_pcm_stream_t)val));
	}

	printf("\nPCM access types:\n");
	for (val = 0; val <= SND_PCM_ACCESS_LAST; val++){
		printf("  %s\n", snd_pcm_access_name((snd_pcm_access_t)val));
	}

	printf("\nPCM formats:\n");
	for (val = 0; val <= SND_PCM_FORMAT_LAST; val++){
		if (snd_pcm_format_name((snd_pcm_format_t)val) != NULL){
			printf("  %s (%s)\n", snd_pcm_format_name((snd_pcm_format_t)val),
				snd_pcm_format_description((snd_pcm_format_t)val));
		}
	}

	printf("\nPCM subformats:\n");
	for (val = 0; val <= SND_PCM_SUBFORMAT_LAST; val++){
		printf("  %s (%s)\n", snd_pcm_subformat_name((snd_pcm_subformat_t)val),
	        snd_pcm_subformat_description((snd_pcm_subformat_t)val));
    }
	
	printf("\nPCM states:\n");
	for (val = 0; val <= SND_PCM_STATE_LAST; val++){
		printf("  %s\n", snd_pcm_state_name((snd_pcm_state_t)val));
	}
}
