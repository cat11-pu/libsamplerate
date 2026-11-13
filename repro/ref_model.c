#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

/*
** Independent, self-contained reference models for the two FIR-free
** converters. These do NOT call libsamplerate; they encode the documented
** interpolation semantics so the library output can be checked sample-by-sample
** at boundary lengths (0, 1 and exactly-one-frame feeds).
*/

/* ZOH: output frame k holds the input frame floor(k / ratio). Input index is
** advanced by 1/ratio each output; the held value updates when an input frame
** boundary is crossed. Missing input (beyond end) repeats the last frame. */
static long
ref_zoh (const float *in, long n, double ratio, long outcap, float *out)
{
	long k = 0 ;
	double pos = 0.0 ;	/* current input frame position */
	for (k = 0 ; k < outcap ; k++)
	{
		long idx = (long) floor (pos) ;
		if (idx >= n) idx = n - 1 ;
		if (idx < 0) idx = 0 ;
		out [k] = in [idx] ;
		pos += 1.0 / ratio ;
		if (pos >= n)
		{
			/* library stops once it has consumed the final frame region */
			k++ ;
			break ;
		}
	}
	return k ;
}

/* Linear: output frame k interpolates input at continuous position p=k/ratio
** between floor(p) and floor(p)+1; frame -1 is treated as the value of frame 0
** (the converter seeds last_value from the first input frame). */
static long
ref_linear (const float *in, long n, double ratio, long outcap, float *out)
{
	long k = 0 ;
	for (k = 0 ; k < outcap ; k++)
	{
		double p = (double) k / ratio ;
		long i0 = (long) floor (p) ;
		double frac = p - i0 ;
		float v0 = (i0 <= 0) ? in [0] : (i0 < n ? in [i0] : in [n - 1]) ;
		long i1 = i0 + 1 ;
		float v1 = (i1 < n) ? in [i1] : in [n - 1] ;
		out [k] = (float) (v0 + frac * ((double) v1 - v0)) ;
		if (i1 >= n) { k++ ; break ; }
	}
	return k ;
}

static int
run (int conv, long n, double ratio, const float *in, float *out, long *used, long *gen)
{
	int err = 0 ;
	SRC_STATE *st = src_new (conv, 1, &err) ;
	if (!st) return -1 ;
	SRC_DATA d ; memset (&d, 0, sizeof d) ;
	memset (out, 0, 256 * sizeof (float)) ;
	d.data_in = n ? in : NULL ;
	d.data_out = out ;
	d.input_frames = n ;
	d.output_frames = 256 ;
	d.src_ratio = ratio ;
	d.end_of_input = 1 ;
	err = src_process (st, &d) ;
	*used = d.input_frames_used ;
	*gen = d.output_frames_gen ;
	src_delete (st) ;
	return err ;
}

int main (void)
{
	static const double ratios[] = {0.5, 1.0, 2.0, 4.0} ;
	static const long lens[] = {0, 1, 2} ;
	float in [8], out [256], ref [256] ;
	int fails = 0, cases = 0 ;

	for (int i = 0 ; i < 8 ; i++) in [i] = 0.25f * (float) (i + 1) ;

	printf ("%-8s %-6s %-4s | %-12s | %-12s | %s\n", "conv", "ratio", "in", "lib u/g/err", "ref gen", "max|diff|") ;
	for (unsigned r = 0 ; r < sizeof ratios / sizeof ratios[0] ; r++)
	for (unsigned li = 0 ; li < sizeof lens / sizeof lens[0] ; li++)
	{
		long n = lens[li], used, gen ;
		/* LINEAR */
		{
			int e = run (SRC_LINEAR, n, ratios[r], in, out, &used, &gen) ;
			long rg = n ? ref_linear (in, n, ratios[r], 256, ref) : 0 ;
			double maxd = 0 ;
			long cm = gen < rg ? gen : rg ;
			for (long k = 0 ; k < cm ; k++) { double d = fabs (out[k] - ref[k]) ; if (d > maxd) maxd = d ; }
			int ok = (e == 0 && (n == 0 ? gen == 0 : (gen == rg && maxd < 1e-6))) ;
			if (!ok) fails++ ; cases++ ;
			printf ("%-8s %-6.2f %-4ld | %3ld/%3ld/%-3d | %12ld | %.3g %s\n",
				"linear", ratios[r], n, used, gen, e, rg, maxd, ok ? "" : "FAIL") ;
		}
		/* ZOH */
		{
			int e = run (SRC_ZERO_ORDER_HOLD, n, ratios[r], in, out, &used, &gen) ;
			long rg = n ? ref_zoh (in, n, ratios[r], 256, ref) : 0 ;
			double maxd = 0 ;
			long cm = gen < rg ? gen : rg ;
			for (long k = 0 ; k < cm ; k++) { double d = fabs (out[k] - ref[k]) ; if (d > maxd) maxd = d ; }
			int ok = (e == 0 && (n == 0 ? gen == 0 : (gen == rg && maxd < 1e-6))) ;
			if (!ok) fails++ ; cases++ ;
			printf ("%-8s %-6.2f %-4ld | %3ld/%3ld/%-3d | %12ld | %.3g %s\n",
				"zoh", ratios[r], n, used, gen, e, rg, maxd, ok ? "" : "FAIL") ;
		}
	}
	printf ("cases=%d fails=%d\n", cases, fails) ;
	return fails ? 1 : 0 ;
}
