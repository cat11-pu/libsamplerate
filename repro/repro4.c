#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
/* Feed N one-frame calls then a zero-frame flush, mimicking streaming drain. */
int main (int argc, char **argv)
{
	int conv = atoi (argv[1]), ch = atoi (argv[2]) ;
	double ratio = atof (argv[3]) ;
	long n = atol (argv[4]) ;
	float *in = calloc (n * ch, sizeof (float)) ;
	float *out = calloc (64 * ch, sizeof (float)) ;
	for (long i = 0 ; i < n * ch ; i++) in [i] = 0.25f * (float) (i + 1) ;
	int err = 0 ;
	SRC_STATE *st = src_new (conv, ch, &err) ;
	long tg = 0 ;
	for (long f = 0 ; f <= n ; f++)
	{
		SRC_DATA d ; memset (&d, 0, sizeof (d)) ;
		d.data_in = in + f * ch ; d.data_out = out ;
		d.input_frames = (f < n) ? 1 : 0 ;
		d.output_frames = 64 ; d.src_ratio = ratio ;
		d.end_of_input = (f == n) ;
		err = src_process (st, &d) ;
		if (err) { printf ("flush err=%d (%s) at call %ld gen_so_far=%ld\n", err, src_strerror (err), f, tg) ; src_delete (st) ; free(in); free(out); return 2 ; }
		tg += d.output_frames_gen ;
	}
	printf ("ok gen=%ld\n", tg) ;
	src_delete (st) ; free (in) ; free (out) ;
	return 0 ;
}
