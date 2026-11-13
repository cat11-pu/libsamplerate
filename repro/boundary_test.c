#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

/*
** Boundary correctness for inputs at/under one frame.
**
** Reference identity: the result of a conversion must not depend on how the
** input is split across src_process() calls. We compare a one-shot call
** (whole input, end_of_input=1) against a chunked run that feeds the same
** samples in blocks of `chunk` frames and then drains (end_of_input) until the
** converter returns zero output frames. Both runs start from a fresh state and
** must produce byte-identical output samples, with identical accounting.
*/

static const int convs[] = {
	SRC_SINC_BEST_QUALITY, SRC_SINC_MEDIUM_QUALITY, SRC_SINC_FASTEST,
	SRC_ZERO_ORDER_HOLD, SRC_LINEAR
} ;
static const char *cnames[] = {
	"sinc_best", "sinc_medium", "sinc_fastest", "zoh", "linear"
} ;

#define OUT_CAP 512

static void
run_oneshot (int conv, int ch, double ratio, long n, const float *in,
             float *out, long *used, long *gen, int *errp)
{
	SRC_STATE *st = src_new (conv, ch, errp) ;
	SRC_DATA d ; memset (&d, 0, sizeof d) ;
	memset (out, 0, OUT_CAP * ch * sizeof (float)) ;
	d.data_in = n ? in : NULL ;
	d.data_out = out ;
	d.input_frames = n ;
	d.output_frames = OUT_CAP ;
	d.src_ratio = ratio ;
	d.end_of_input = 1 ;
	*errp = src_process (st, &d) ;
	*used = d.input_frames_used ;
	*gen = d.output_frames_gen ;
	src_delete (st) ;
}

static void
run_chunked (int conv, int ch, double ratio, long n, long chunk, const float *in,
             float *out, long *used, long *gen, int *errp)
{
	SRC_STATE *st = src_new (conv, ch, errp) ;
	SRC_DATA d ;
	long pos = 0 ;
	*used = *gen = 0 ; *errp = 0 ;
	memset (out, 0, OUT_CAP * ch * sizeof (float)) ;

	/* feed all input in blocks of `chunk` frames */
	while (pos < n && *errp == 0)
	{
		long take = (n - pos < chunk) ? (n - pos) : chunk ;
		memset (&d, 0, sizeof d) ;
		d.data_in = in + pos * ch ;
		d.data_out = out + *gen * ch ;
		d.input_frames = take ;
		d.output_frames = OUT_CAP - *gen ;
		d.src_ratio = ratio ;
		d.end_of_input = 0 ;
		*errp = src_process (st, &d) ;
		pos += d.input_frames_used ;
		*used += d.input_frames_used ;
		*gen += d.output_frames_gen ;
	}

	/* drain: zero-frame, NULL-input, end_of_input calls until nothing comes out */
	for (int guard = 0 ; guard < 1000 && *errp == 0 ; guard++)
	{
		memset (&d, 0, sizeof d) ;
		d.data_in = NULL ;
		d.data_out = out + *gen * ch ;
		d.input_frames = 0 ;
		d.output_frames = OUT_CAP - *gen ;
		d.src_ratio = ratio ;
		d.end_of_input = 1 ;
		*errp = src_process (st, &d) ;
		*gen += d.output_frames_gen ;
		if (d.output_frames_gen == 0)
			break ;
	}
	src_delete (st) ;
}

int main (void)
{
	static const double ratios[] = {0.25, 0.5, 1.0, 2.0, 4.0} ;
	static const long lens[] = {0, 1, 2} ; /* zero-length, single-sample, exact-one-frame */
	static const long chunks[] = {1, 2, 4} ;
	float in [64], oa [OUT_CAP * 8], ob [OUT_CAP * 8] ;
	int failures = 0, cases = 0 ;

	for (int i = 0 ; i < 64 ; i++) in [i] = 0.25f * (float) (i + 1) ;

	printf ("%-12s %-6s %-4s %-4s %-3s | %-14s | %-14s | %s\n",
		"converter", "ratio", "in", "ch", "blk", "oneshot u/g/e", "chunked u/g/e", "exact") ;

	for (unsigned c = 0 ; c < sizeof convs / sizeof convs[0] ; c++)
	for (unsigned r = 0 ; r < sizeof ratios / sizeof ratios[0] ; r++)
	for (unsigned li = 0 ; li < sizeof lens / sizeof lens[0] ; li++)
	for (int ch = 1 ; ch <= 2 ; ch++)
	for (unsigned ci = 0 ; ci < sizeof chunks / sizeof chunks[0] ; ci++)
	{
		long n = lens[li], ua, ga, ub, gb ; int ea, eb ;
		cases++ ;
		run_oneshot (convs[c], ch, ratios[r], n, in, oa, &ua, &ga, &ea) ;
		run_chunked (convs[c], ch, ratios[r], n, chunks[ci], in, ob, &ub, &gb, &eb) ;

		int same = (ea == eb && ua == ub && ga == gb) ;
		if (same)
			for (long k = 0 ; k < ga * ch ; k++)
				if (memcmp (&oa[k], &ob[k], sizeof (float)) != 0) { same = 0 ; break ; }

		int sane = (ua <= n && ga <= OUT_CAP && ub <= n && gb <= OUT_CAP) ;
		if (!same || !sane)
		{
			failures++ ;
			printf ("%-12s %-6.2f %-4ld %-4d %-3ld | %4ld/%4ld/%-3d  | %4ld/%4ld/%-3d  | %s%s\n",
				cnames[c], ratios[r], n, ch, chunks[ci], ua, ga, ea, ub, gb, eb,
				same ? "yes" : "NO", sane ? "" : " UNSANE") ;
		}
	}

	printf ("\ncases=%d failures=%d\n", cases, failures) ;
	return failures ? 1 : 0 ;
}
