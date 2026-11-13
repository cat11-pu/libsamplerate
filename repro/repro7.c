#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
/* exact replica of repro2 inner loop, single converter, no ASan-aborting converter */
int main (int argc, char **argv)
{
	int conv = atoi (argv[1]) ;
	double ratio = atof (argv[2]) ;
	float in [16], out [256] ;
	for (int i = 0 ; i < 16 ; i++) in [i] = 0.25f * (float) (i + 1) ;
	int err = 0 ;
	SRC_STATE *st = src_new (conv, 1, &err) ;
	long total_gen = 0, total_used = 0 ;
	for (long f = 0 ; f <= 16 ; f++)
	{
		SRC_DATA d ; memset (&d, 0, sizeof (d)) ; memset (out, 0, sizeof (out)) ;
		d.data_in = in + f ;
		d.data_out = out ;
		d.input_frames = (f < 16) ? 1 : 0 ;
		d.output_frames = 16 ;
		d.src_ratio = ratio ;
		d.end_of_input = (f == 16) ? 1 : 0 ;
		err = src_process (st, &d) ;
		if (err) { printf ("ERR call=%ld err=%d %s\n", f, err, src_strerror(err)) ; return 2 ; }
		total_gen += d.output_frames_gen ;
		total_used += d.input_frames_used ;
	}
	printf ("ok used=%ld gen=%ld\n", total_used, total_gen) ;
	src_delete (st) ;
	return 0 ;
}
