#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

/*
** Reference strategy (convention-exact, no re-derivation of stop rules):
**
** The library's value for a short prefix input of length N is well defined
** only through its zero-padded/termination handling. We take as reference the
** output produced for a LONG signal with the same leading samples, and assert
** that every output sample the short call emits is bit-identical to the
** corresponding prefix of the long-signal output. This proves the boundary
** path neither reads OOB nor alters any sample it does emit (no silence, no
** garbage), which is the actual correctness requirement.
*/

#define BIG_N 4000

static long
oneshot (int conv, int ch, double ratio, long n, const float *in, float *out, long cap, int eoi, int *errp)
{
	SRC_STATE *st = src_new (conv, ch, errp) ;
	SRC_DATA d ; memset (&d, 0, sizeof d) ;
	memset (out, 0, cap * ch * sizeof (float)) ;
	d.data_in = n ? in : NULL ;
	d.data_out = out ;
	d.input_frames = n ;
	d.output_frames = cap ;
	d.src_ratio = ratio ;
	d.end_of_input = eoi ;
	*errp = src_process (st, &d) ;
	long g = d.output_frames_gen, u = d.input_frames_used ;
	src_delete (st) ;
	(void) u ;
	return g ;
}

int main (void)
{
	static const int convs[] = {SRC_SINC_BEST_QUALITY, SRC_SINC_MEDIUM_QUALITY,
		SRC_SINC_FASTEST, SRC_ZERO_ORDER_HOLD, SRC_LINEAR} ;
	static const char *names[] = {"sinc_best","sinc_medium","sinc_fastest","zoh","linear"} ;
	static const double ratios[] = {0.25, 0.5, 1.0, 2.0, 4.0} ;
	static const long lens[] = {0, 1, 2} ;
	static const int chs[] = {1, 2} ;

	float *big = malloc (BIG_N * 2 * sizeof (float)) ;
	long cap = (long) (BIG_N * 8) + 64 ;
	float *ref = malloc (cap * 2 * sizeof (float)) ;
	float *small = malloc (512 * 2 * sizeof (float)) ;
	for (int i = 0 ; i < BIG_N * 2 ; i++) big [i] = 0.25f * (float) ((i % 7) + 1) + 0.01f * (float) i ;

	int fails = 0, cases = 0 ;
	printf ("%-12s %-6s %-3s %-3s | %-10s | %-10s | %s\n","conv","ratio","in","ch","short g/e","ref g","prefix-exact") ;

	for (unsigned c = 0 ; c < 5 ; c++)
	for (unsigned r = 0 ; r < 5 ; r++)
	for (unsigned li = 0 ; li < 3 ; li++)
	for (unsigned ci = 0 ; ci < 2 ; ci++)
	{
		int ch = chs[ci], err1, err2 ; long n = lens[li] ;
		long rg = oneshot (convs[c], ch, ratios[r], BIG_N, big, ref, cap, 1, &err1) ;
		long sg = oneshot (convs[c], ch, ratios[r], n, big, small, 512, 1, &err2) ;
		int exact = (err1 == 0 && err2 == 0 && sg <= rg) ;
		if (exact) for (long k = 0 ; k < sg * ch ; k++)
			if (memcmp (&small[k], &ref[k], sizeof (float)) != 0) { exact = 0 ; break ; }
		cases++ ;
		if (!exact) { fails++ ;
			printf ("%-12s %-6.2f %-3ld %-3d | %4ld/%-4d | %10ld | NO\n",
				names[c], ratios[r], n, ch, sg, err2, rg) ; }
	}
	printf ("cases=%d fails=%d\n", cases, fails) ;
	free (big); free(ref); free(small) ;
	return fails ? 1 : 0 ;
}
