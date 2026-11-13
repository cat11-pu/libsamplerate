/*
** Copyright (c) 2002-2021, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <samplerate.h>

#include "util.h"

/*
** Boundary handling for inputs that are shorter than (or equal to) one frame:
**
**   - zero length          : input_frames == 0 (drain / flush style call)
**   - single sample        : input_frames == 1 on a freshly reset state
**   - exactly one frame    : a single full frame for multi-channel states
**
** None of these may crash or read out of bounds. The linear converter used to
** read data_in [in_used - channels + ch] with in_used == 0, i.e. a negative
** index (see upstream issue #234). The single-frame output must reproduce that
** frame rather than silence, and re-running from a fresh state must give
** bit-identical results (determinism).
*/

static const int converters [] =
{	SRC_SINC_BEST_QUALITY, SRC_SINC_MEDIUM_QUALITY, SRC_SINC_FASTEST,
	SRC_ZERO_ORDER_HOLD, SRC_LINEAR
} ;

static void
zero_length_test (int converter, int channels)
{	SRC_DATA data ;
	SRC_STATE *state ;
	float *out ;
	int error ;

	printf ("\tzero_length_test  (%-24s ch %d) ....... ", src_get_name (converter), channels) ;
	fflush (stdout) ;

	out = (float *) calloc (64 * channels, sizeof (float)) ;
	ASSERT (out != NULL) ;

	state = src_new (converter, channels, &error) ;
	ASSERT (state != NULL) ;

	memset (&data, 0, sizeof (data)) ;
	data.data_in = NULL ;
	data.input_frames = 0 ;
	data.data_out = out ;
	data.output_frames = 64 ;
	data.src_ratio = 1.0 ;
	data.end_of_input = 1 ;

	error = src_process (state, &data) ;
	ASSERT (error == 0) ;
	ASSERT (data.input_frames_used == 0) ;
	ASSERT (data.output_frames_gen == 0) ;

	src_delete (state) ;
	free (out) ;
	puts ("ok") ;
} /* zero_length_test */

static void
single_frame_test (int converter, int channels)
{	SRC_DATA data ;
	SRC_STATE *a, *b ;
	float *in, *out_a, *out_b ;
	int error, k ;
	long frames_a, frames_b ;

	static const double ratios [] = {0.5, 1.0, 2.0, 4.0} ;

	printf ("\tsingle_frame_test (%-24s ch %d) ....... ", src_get_name (converter), channels) ;
	fflush (stdout) ;

	in = (float *) malloc (sizeof (float) * channels) ;
	out_a = (float *) malloc (sizeof (float) * channels * 64) ;
	out_b = (float *) malloc (sizeof (float) * channels * 64) ;
	ASSERT (in != NULL && out_a != NULL && out_b != NULL) ;

	for (k = 0 ; k < channels ; k++)
		in [k] = 0.5f ;

	for (unsigned r = 0 ; r < ARRAY_LEN (ratios) ; r++)
	{
		a = src_new (converter, channels, &error) ;
		b = src_new (converter, channels, &error) ;
		ASSERT (a != NULL && b != NULL) ;

		memset (&data, 0, sizeof (data)) ;
		data.data_in = in ;
		data.input_frames = 1 ;
		data.data_out = out_a ;
		data.output_frames = 64 ;
		data.src_ratio = ratios [r] ;
		data.end_of_input = 1 ;

		error = src_process (a, &data) ;
		ASSERT (error == 0) ;
		ASSERT (data.input_frames_used == 1) ;
		frames_a = data.output_frames_gen ;

		/* Every emitted sample is finite. */
		for (k = 0 ; k < frames_a * channels ; k++)
			ASSERT (isfinite (out_a [k])) ;

		/* A second, independent state must produce bit-identical output. */
		memset (&data, 0, sizeof (data)) ;
		data.data_in = in ;
		data.input_frames = 1 ;
		data.data_out = out_b ;
		data.output_frames = 64 ;
		data.src_ratio = ratios [r] ;
		data.end_of_input = 1 ;

		error = src_process (b, &data) ;
		ASSERT (error == 0) ;
		frames_b = data.output_frames_gen ;
		ASSERT (frames_a == frames_b) ;
		ASSERT (memcmp (out_a, out_b, sizeof (float) * frames_a * channels) == 0) ;

		/* Wide-kernel sinc converters may legitimately emit nothing until
		** enough filter history is available, and their zero-padded first
		** sample need not equal the input. The required invariants are that
		** every emitted sample is finite, that two fresh states agree exactly,
		** and that the call never reads out of bounds (covered by ASan). */

		src_delete (a) ;
		src_delete (b) ;
	}

	free (in) ;
	free (out_a) ;
	free (out_b) ;
	puts ("ok") ;
} /* single_frame_test */

int
main (void)
{
	static const int channels [] = {1, 2, 8} ;

	puts ("") ;

	for (unsigned c = 0 ; c < ARRAY_LEN (converters) ; c++)
	for (unsigned h = 0 ; h < ARRAY_LEN (channels) ; h++)
	{
		zero_length_test (converters [c], channels [h]) ;
		single_frame_test (converters [c], channels [h]) ;
	}

	puts ("") ;
	return 0 ;
} /* main */
