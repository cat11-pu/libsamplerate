#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>

static const int convs[] = {
	SRC_SINC_BEST_QUALITY, SRC_SINC_MEDIUM_QUALITY, SRC_SINC_FASTEST,
	SRC_ZERO_ORDER_HOLD, SRC_LINEAR
} ;
static const char *cnames[] = {
	"sinc_best", "sinc_medium", "sinc_fastest", "zoh", "linear"
} ;

int main (void)
{
	float in [64], out [256] ;
	for (int i = 0 ; i < 64 ; i++) in [i] = 0.25f * (float) (i + 1) ;

	double ratios [] = {0.25, 0.5, 1.0, 2.0, 4.0} ;

	for (unsigned c = 0 ; c < sizeof (convs) / sizeof (convs [0]) ; c++)
	for (unsigned r = 0 ; r < sizeof (ratios) / sizeof (ratios [0]) ; r++)
	for (long inframes = 0 ; inframes <= 8 ; inframes++)
	{
		int err = 0 ;
		SRC_STATE *st = src_new (convs [c], 1, &err) ;
		if (!st) { printf ("src_new failed %s\n", cnames [c]) ; return 1 ; }

		SRC_DATA d ;
		memset (&d, 0, sizeof (d)) ;
		memset (out, 0, sizeof (out)) ;
		d.data_in = in ;
		d.data_out = out ;
		d.input_frames = inframes ;
		d.output_frames = 64 ;
		d.src_ratio = ratios [r] ;
		d.end_of_input = 1 ;

		err = src_process (st, &d) ;
		printf ("%-12s ratio=%4.2f in=%2ld -> err=%d used=%ld gen=%ld out0=%.7f\n",
			cnames [c], ratios [r], inframes, err,
			d.input_frames_used, d.output_frames_gen,
			d.output_frames_gen > 0 ? out [0] : 0.0f) ;
		src_delete (st) ;
	}
	return 0 ;
}
