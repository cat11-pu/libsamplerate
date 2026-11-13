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

/* feed one frame at a time over several calls, then a final empty flush */
int main (void)
{
	float in [16], out [256] ;
	for (int i = 0 ; i < 16 ; i++) in [i] = 0.25f * (float) (i + 1) ;
	double ratios [] = {0.5, 1.0, 2.0} ;

	for (unsigned c = 0 ; c < 5 ; c++)
	for (unsigned r = 0 ; r < 3 ; r++)
	{
		int err = 0 ;
		SRC_STATE *st = src_new (convs [c], 1, &err) ;
		long total_gen = 0, total_used = 0 ;
		for (long f = 0 ; f <= 16 ; f++)
		{
			SRC_DATA d ; memset (&d, 0, sizeof (d)) ; memset (out, 0, sizeof (out)) ;
			d.data_in = in + f ;
			d.data_out = out ;
			d.input_frames = (f < 16) ? 1 : 0 ;
			d.output_frames = 16 ;
			d.src_ratio = ratios [r] ;
			d.end_of_input = (f == 16) ? 1 : 0 ;
			err = src_process (st, &d) ;
			if (err) { printf ("ERR %s ratio=%.1f call=%ld err=%d\n", cnames[c], ratios[r], f, err) ; break ; }
			total_gen += d.output_frames_gen ;
			total_used += d.input_frames_used ;
		}
		printf ("%-12s ratio=%.1f streaming total_used=%ld total_gen=%ld\n", cnames[c], ratios[r], total_used, total_gen) ;
		src_delete (st) ;
	}
	return 0 ;
}
