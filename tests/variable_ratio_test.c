/*
** Copyright (c) 2026, libsamplerate contributors.
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <samplerate.h>

#include "util.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338
#endif

#define TEST_MAX_RATIO		256.0
#define INPUT_FRAMES		(1 << 15)
#define INPUT_BLOCK_FRAMES	257
#define OUTPUT_CAPACITY		(INPUT_FRAMES * 3 + 4096)
#define MAX_BLOCKS		(INPUT_FRAMES / INPUT_BLOCK_FRAMES + 2)

static const double ratio_schedule [] =
{	1.0, 0.8, 1.25, 0.6, 1.7, 0.9, 1.1, 0.55, 1.9, 1.0, 0.75, 1.4
} ;

typedef struct
{	long	output_frames, ideal_frames, input_frames_used ;
	double	max_abs_sample, max_delta, max_boundary_delta ;
	long	boundary_index [MAX_BLOCKS] ;
	int		boundary_count ;
} RUN_METRICS ;

static void run_stream (int converter, int channels, int output_chunk,
			int variable_api, float *output, long output_capacity,
			RUN_METRICS *metrics) ;
static void test_converter (int converter, int channels, double sample_tolerance) ;
static void test_process_api (int converter) ;
static void test_ratio_bounds (void) ;
static void test_extreme_ramp (int channels) ;
static void test_zero_output (void) ;
static void compare_runs (const RUN_METRICS *a, const RUN_METRICS *b,
			const float *a_data, const float *b_data, int channels, double tolerance) ;
static double max_input_step (int channels) ;
static void fill_input (float *input, int channels) ;
static void fail (const char *message) ;

int
main (void)
{
	double zoh_tolerance = 2.0 * max_input_step (1) ;

	puts ("\n    Variable ratio streaming test") ;

	printf ("        Linear interpolator             : ") ;
	fflush (stdout) ;
	test_converter (SRC_LINEAR, 1, 2e-5) ;
	puts ("ok") ;

	printf ("        Zero order hold interpolator    : ") ;
	fflush (stdout) ;
	test_converter (SRC_ZERO_ORDER_HOLD, 1, zoh_tolerance) ;
	puts ("ok") ;

#ifdef ENABLE_SINC_FAST_CONVERTER
	printf ("        Sinc interpolator               : ") ;
	fflush (stdout) ;
	test_converter (SRC_SINC_FASTEST, 1, 2e-5) ;
	puts ("ok") ;
#endif

	printf ("        Multi-channel                   : ") ;
	fflush (stdout) ;
	test_converter (SRC_LINEAR, 2, 2e-5) ;
	test_converter (SRC_LINEAR, 9, 2e-5) ;
	puts ("ok") ;

	printf ("        Per-block src_process()         : ") ;
	fflush (stdout) ;
	test_process_api (SRC_LINEAR) ;
#ifdef ENABLE_SINC_FAST_CONVERTER
	test_process_api (SRC_SINC_FASTEST) ;
#endif
	puts ("ok") ;

	printf ("        Ratio bounds                    : ") ;
	fflush (stdout) ;
	test_ratio_bounds () ;
	puts ("ok") ;

	printf ("        Extreme block ramps             : ") ;
	fflush (stdout) ;
	test_extreme_ramp (1) ;
	test_extreme_ramp (2) ;
	test_extreme_ramp (9) ;
	puts ("ok") ;

	printf ("        Zero output capacity            : ") ;
	fflush (stdout) ;
	test_zero_output () ;
	puts ("ok") ;

	puts ("") ;

	return 0 ;
} /* main */

static void
test_converter (int converter, int channels, double sample_tolerance)
{
	static const int chunks [] = { 32, 128, 4096 } ;
	float *output [3] ;
	RUN_METRICS metrics [3] ;
	int k ;

	for (k = 0 ; k < 3 ; k++)
	{	output [k] = calloc (OUTPUT_CAPACITY * channels, sizeof (float)) ;
		if (output [k] == NULL)
			fail ("Out of memory.") ;

		run_stream (converter, channels, chunks [k], 1, output [k],
				OUTPUT_CAPACITY, &metrics [k]) ;
		} ;

	if (metrics [0].input_frames_used != INPUT_FRAMES)
		fail ("Not all input frames were consumed.") ;

	for (k = 1 ; k < 3 ; k++)
	{	compare_runs (&metrics [0], &metrics [k], output [0], output [k],
				channels, sample_tolerance) ;
		} ;

	/* A linear ratio ramp over an input block has an expected output length
	** equal to the block length times the mean of the two endpoint ratios. */
	if (fabs (metrics [0].output_frames - metrics [0].ideal_frames) > 4)
	{	printf ("\noutput length %ld does not match ideal %ld\n",
				metrics [0].output_frames, metrics [0].ideal_frames) ;
		fail ("Cumulative output length error.") ;
		} ;

	/* A ratio change must not create an isolated sample jump larger than the
	** normal first difference of the resampled signal. */
	if (metrics [0].max_boundary_delta > 1.5 * metrics [0].max_delta + 1e-5)
		fail ("Discontinuity at a ratio block boundary.") ;

	if (metrics [0].max_abs_sample > 1.1)
		fail ("Output level is out of range.") ;

	printf ("converter=%d channels=%d chunk=%d output=%ld ideal=%ld max_abs=%g max_delta=%g boundary_delta=%g\n",
			converter, channels, chunks [0], metrics [0].output_frames,
			metrics [0].ideal_frames, metrics [0].max_abs_sample,
			metrics [0].max_delta,
			metrics [0].max_boundary_delta) ;

	for (k = 0 ; k < 3 ; k++)
		free (output [k]) ;
} /* test_converter */

static void
test_process_api (int converter)
{
	float *output [2] ;
	RUN_METRICS metrics [2] ;
	int k ;

	for (k = 0 ; k < 2 ; k++)
	{	output [k] = calloc (OUTPUT_CAPACITY, sizeof (float)) ;
		if (output [k] == NULL)
			fail ("Out of memory.") ;

		run_stream (converter, 1, k == 0 ? 32 : 4096, 0, output [k],
				OUTPUT_CAPACITY, &metrics [k]) ;
		} ;

	compare_runs (&metrics [0], &metrics [1], output [0], output [1], 1, 2e-5) ;

	if (fabs (metrics [0].output_frames - metrics [0].ideal_frames) > 4)
		fail ("src_process() accumulated output length error.") ;

	for (k = 0 ; k < 2 ; k++)
		free (output [k]) ;
} /* test_process_api */

static void
run_stream (int converter, int channels, int output_chunk, int variable_api,
			float *output, long output_capacity, RUN_METRICS *metrics)
{
	float *input ;
	SRC_STATE *state ;
	SRC_DATA data ;
	long input_pos, output_pos, block_start ;
	int block, error, ch ;
	double previous_ratio, ideal_frames ;

	input = calloc (INPUT_FRAMES * channels, sizeof (float)) ;
	if (input == NULL)
		fail ("Out of memory.") ;

	fill_input (input, channels) ;

	if ((state = src_new (converter, channels, &error)) == NULL)
	{	printf ("src_new() failed : %s\n", src_strerror (error)) ;
		fail ("src_new() failed.") ;
		} ;

	memset (metrics, 0, sizeof (*metrics)) ;
	memset (&data, 0, sizeof (data)) ;

	data.end_of_input = 0 ;
	input_pos = output_pos = 0 ;
	block = 0 ;
	previous_ratio = ratio_schedule [0] ;
	ideal_frames = 0.0 ;

	while (input_pos < INPUT_FRAMES)
	{	long block_frames = MIN (INPUT_BLOCK_FRAMES, INPUT_FRAMES - input_pos) ;
		long block_remaining = block_frames ;
		double ratio_start = previous_ratio ;
		double ratio_end = ratio_schedule [block % ARRAY_LEN (ratio_schedule)] ;

		ideal_frames += block_frames * (ratio_start + ratio_end) * 0.5 ;
		block_start = output_pos ;
		metrics->boundary_index [metrics->boundary_count++] = block_start ;

		while (block_remaining > 0)
		{	data.data_in = input + input_pos * channels ;
			data.input_frames = block_remaining ;
			data.data_out = output + output_pos * channels ;
			data.output_frames = MIN (output_chunk, output_capacity - output_pos) ;

			if (variable_api)
				error = src_process_variable (state, &data, ratio_start, ratio_end) ;
			else
			{	data.src_ratio = ratio_end ;
				error = src_process (state, &data) ;
				} ;

			if (error != 0)
			{	printf ("Processing failed : %s\n", src_strerror (error)) ;
				fail ("Variable ratio processing failed.") ;
				} ;

			input_pos += data.input_frames_used ;
			block_remaining -= data.input_frames_used ;
			output_pos += data.output_frames_gen ;

			if (data.input_frames_used == 0 && data.output_frames_gen == 0)
				fail ("src_process_variable() made no progress.") ;

			if (output_pos > output_capacity)
				fail ("Output buffer overflow.") ;
			} ;

		previous_ratio = ratio_end ;
		block ++ ;
		} ;

	/* Flush the converter at the last requested ratio. */
	memset (&data, 0, sizeof (data)) ;
	data.end_of_input = 1 ;
	data.data_in = input + INPUT_FRAMES * channels ;
	data.src_ratio = previous_ratio ;

	do
	{	data.data_out = output + output_pos * channels ;
		data.output_frames = MIN (output_chunk, output_capacity - output_pos) ;

		if ((error = src_process (state, &data)) != 0)
		{	printf ("src_process() failed : %s\n", src_strerror (error)) ;
			fail ("Flush failed.") ;
			} ;

		output_pos += data.output_frames_gen ;
		if (output_pos > output_capacity)
			fail ("Output buffer overflow.") ;
		}
	while (data.output_frames_gen > 0) ;

	metrics->output_frames = output_pos ;
	metrics->input_frames_used = input_pos ;
	metrics->ideal_frames = (long) floor (ideal_frames + 0.5) ;

	for (long k = 0 ; k < output_pos ; k++)
		metrics->max_abs_sample = MAX (metrics->max_abs_sample, fabs (output [k])) ;

	for (ch = 0 ; ch < channels ; ch++)
		for (long k = 1 ; k < output_pos ; k++)
			metrics->max_delta = MAX (metrics->max_delta,
					fabs (output [k * channels + ch] - output [(k - 1) * channels + ch])) ;

	for (int b = 1 ; b < metrics->boundary_count ; b++)
	{	long edge = metrics->boundary_index [b] ;

		if (edge <= 0 || edge >= output_pos)
			continue ;

		for (ch = 0 ; ch < channels ; ch++)
			metrics->max_boundary_delta = MAX (metrics->max_boundary_delta,
					fabs (output [edge * channels + ch] - output [(edge - 1) * channels + ch])) ;
		} ;

	state = src_delete (state) ;
	free (input) ;
} /* run_stream */

static void
compare_runs (const RUN_METRICS *a, const RUN_METRICS *b,
			const float *a_data, const float *b_data, int channels, double tolerance)
{
	long frames = MIN (a->output_frames, b->output_frames) ;
	double max_diff = 0.0 ;

	if (labs (a->output_frames - b->output_frames) > 1)
		fail ("Output length depends on output chunk size.") ;

	for (long k = 0 ; k < frames * channels ; k++)
	{	double diff = fabs (a_data [k] - b_data [k]) ;
		max_diff = MAX (max_diff, diff) ;
		} ;

	if (max_diff > tolerance)
	{	printf ("\noutput chunk mismatch : %g > %g\n", max_diff, tolerance) ;
		fail ("Output samples depend on output chunk size.") ;
		} ;
} /* compare_runs */

static void
test_ratio_bounds (void)
{
	SRC_STATE *state ;
	SRC_DATA data ;
	float input [4] = { 0.0f, 0.0f, 0.0f, 0.0f } ;
	float output [4096] ;
	int error ;

	if ((state = src_new (SRC_LINEAR, 1, &error)) == NULL)
		fail ("src_new() failed.") ;

	memset (&data, 0, sizeof (data)) ;
	data.data_in = input ;
	data.data_out = output ;
	data.input_frames = 1 ;
	data.output_frames = ARRAY_LEN (output) ;

	if (src_is_valid_ratio (1.0 / 512.0) || src_is_valid_ratio (512.0))
		fail ("Ratio validation bounds are wrong.") ;

	if (src_process_variable (state, &data, 1.0 / 512.0, 1.0) == 0)
		fail ("Below-minimum start ratio was accepted.") ;
	if (src_process_variable (state, &data, 1.0, 512.0) == 0)
		fail ("Above-maximum end ratio was accepted.") ;

	if (src_process_variable (state, &data, 1.0 / TEST_MAX_RATIO, TEST_MAX_RATIO) != 0)
		fail ("Extreme but valid ratios were rejected.") ;

	state = src_delete (state) ;
} /* test_ratio_bounds */

static void
test_extreme_ramp (int channels)
{
	SRC_STATE *state ;
	SRC_DATA data ;
	float *input, *output ;
	long input_pos, output_pos ;
	int error ;

	input = calloc (32 * channels, sizeof (float)) ;
	output = calloc (8192 * channels, sizeof (float)) ;
	if (input == NULL || output == NULL)
		fail ("Out of memory.") ;

	if ((state = src_new (SRC_LINEAR, channels, &error)) == NULL)
		fail ("src_new() failed.") ;

	memset (&data, 0, sizeof (data)) ;
	input_pos = output_pos = 0 ;
	data.end_of_input = 0 ;

	while (input_pos < 32)
	{	data.data_in = input + input_pos * channels ;
		data.input_frames = 32 - input_pos ;
		data.data_out = output + output_pos * channels ;
		data.output_frames = 8192 - output_pos ;

		if (src_process_variable (state, &data, 1.0 / TEST_MAX_RATIO, TEST_MAX_RATIO) != 0)
			fail ("Extreme ramp failed.") ;

		input_pos += data.input_frames_used ;
		output_pos += data.output_frames_gen ;

		if (data.input_frames_used == 0 && data.output_frames_gen == 0)
			fail ("Extreme ramp made no progress.") ;
		} ;

	for (long k = 0 ; k < output_pos * channels ; k++)
	{	if (!isfinite (output [k]))
			fail ("Extreme ramp produced a non-finite sample.") ;
		} ;

	state = src_delete (state) ;
	free (input) ;
	free (output) ;
} /* test_extreme_ramp */

static void
test_zero_output (void)
{
	SRC_STATE *state ;
	SRC_DATA data ;
	float input [64] = { 0.0f } ;
	float output [64] ;
	int error ;

	if ((state = src_new (SRC_LINEAR, 1, &error)) == NULL)
		fail ("src_new() failed.") ;

	memset (&data, 0, sizeof (data)) ;
	data.data_in = input ;
	data.data_out = output ;
	data.input_frames = 64 ;
	data.output_frames = 0 ;
	data.end_of_input = 0 ;

	if (src_process_variable (state, &data, 1.0, 1.0) != 0)
		fail ("Zero-sized output buffer failed.") ;

	if (data.input_frames_used != 0 || data.output_frames_gen != 0)
		fail ("Zero-sized output buffer consumed input.") ;

	data.output_frames = ARRAY_LEN (output) ;
	if (src_process_variable (state, &data, 1.0, 1.0) != 0)
		fail ("Continuation after zero-sized output failed.") ;

	state = src_delete (state) ;
} /* test_zero_output */

static void
fill_input (float *input, int channels)
{
	for (int ch = 0 ; ch < channels ; ch++)
	{	double frequency = 0.003 + 0.0002 * ch ;
		double phase = 0.37 * ch ;

		for (int k = 0 ; k < INPUT_FRAMES ; k++)
			input [k * channels + ch] =
					(float) (0.5 * sin (2.0 * M_PI * frequency * k + phase)) ;
		} ;
} /* fill_input */

static double
max_input_step (int channels)
{
	double result = 0.0 ;

	for (int ch = 0 ; ch < channels ; ch++)
	{	double frequency = 0.003 + 0.0002 * ch ;
		result = MAX (result, 0.5 * 2.0 * M_PI * frequency) ;
		} ;

	return result ;
} /* max_input_step */

static void
fail (const char *message)
{	printf ("\n%s\n", message) ;
	exit (1) ;
} /* fail */
