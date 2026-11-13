#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main (int argc, char **argv)
{
	int conv = atoi (argv[1]) ;
	double ratio = atof (argv[2]) ;
	long inframes = atol (argv[3]) ;
	long outcap = atol (argv[4]) ;
	float *in = calloc (inframes ? inframes : 1, sizeof (float)) ;
	float *out = calloc (outcap, sizeof (float)) ;
	for (long i = 0 ; i < inframes ; i++) in [i] = 0.25f * (float) (i + 1) ;
	int err = 0 ;
	SRC_STATE *st = src_new (conv, 1, &err) ;
	SRC_DATA d ; memset (&d, 0, sizeof (d)) ;
	d.data_in = in ; d.data_out = out ;
	d.input_frames = inframes ; d.output_frames = outcap ;
	d.src_ratio = ratio ; d.end_of_input = 1 ;
	err = src_process (st, &d) ;
	printf ("err=%d used=%ld gen=%ld out0=%.7f\n", err, d.input_frames_used, d.output_frames_gen,
		d.output_frames_gen ? out[0] : 0.0f) ;
	src_delete (st) ;
	free (in) ; free (out) ;
	return 0 ;
}
