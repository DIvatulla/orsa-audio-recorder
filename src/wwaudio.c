#include "../include/wwaudio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

double rms(audio_buffer *b, int frames, int bpf)
{	
	int i;
	int end;
    long long int sum = 0;
	short *sample_ptr;

	if (b->ri > b->wi){
		fprintf(stderr, "Start position is ahead of last written byte\n");
		return -1.0f;
	}
	end = (b->ri + (frames * bpf));
	if (end > b->wi){
		fprintf(stderr, "Trying to read more samples that were wrote\n");
		printf("b->size - %d, b->wi - %d, end - %d\n", b->size, b->wi, end);
		return -1.0f;
	}
	
	sample_ptr = (short*)(b->buf + b->ri);
	while (b->ri < end){
		b->ri += bpf;
		sum += (long long int)((*sample_ptr) * (*sample_ptr));
		++sample_ptr;
	}

    return (sqrt((double)sum / sa));
}


