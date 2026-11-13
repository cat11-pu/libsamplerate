#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main (int argc, char **argv)
{
	int conv = atoi (argv[1]) ;
	double ratio = atof (argv[2]) ;
	long n = atol (argv[3]) ;
	float in [16], out [256] ;
	for (int i = 0 ; i < 16 ; i++) in [i] = 0.25f * (float) (i + 1) ;
	int err = 0 ;
	SRC_STATE *st = src_new (conv, 1, &err) ;
	long tg = 0, tu = 0 ;
	for (long f = 0 ; f <= n ; f++)
	{
		SRC_DATA d ; memset (&d, 0, sizeof (d)) ; memset (out, 0, sizeof (out)) ;
		d.data_in = in + f ; d.data_out = out ;
		d.input_frames = (f < n) ? 1 : 0 ;
		d.output_frames = 16 ; d.src_ratio = ratio ;
		d.end_of_input = (f == n) ;
		err = src_process (st, &d) ;
		if (err) { printf ("FAIL call=%ld err=%d %s tu=%ld tg=%ld\n", f, err, src_strerror(err), tu, tg) ; return 2 ; }
		tu += d.input_frames_used ; tg += d.output_frames_gen ;
	}
	printf ("ok used=%ld gen=%ld\n", tu, tg) ;
	src_delete (st) ;
	return 0 ;
}
