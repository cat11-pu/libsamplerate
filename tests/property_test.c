/*
** Copyright (c) 2002-2026, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <samplerate.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338
#endif

#define DEFAULT_SEED UINT64_C (0x1234abcd5678ef90)
#define RANDOM_CASES 1000
#define JUMP_CASES 40
#define SHORT_CASES 40
#define LONG_FRAMES 1048576L
#define GUARD_FRAMES 16
#define MAX_SAVED_CASES 16
#define CLIP_LIMIT 1.0
#define STEP_LIMIT 1.5
#define MAX_CALLS 10000000L

typedef struct
{
	int converter ;
	double ratio ;
	long input_frames ;
	long in_chunk ;
	long out_chunk ;
	uint64_t seed ;
} TEST_CASE ;

typedef struct
{
	TEST_CASE base ;
	int jump_count ;
	long jump_at [4] ;
	double jump_ratio [4] ;
} JUMP_CASE ;

typedef struct
{
	double max_abs ;
	double max_step ;
	long samples ;
	long nonfinite ;
	long clipped ;
	long steps ;
} STATS ;

typedef struct
{
	int failed ;
	const char *reason ;
	long index ;
	double value ;
	double limit ;
	long reference_frames ;
	long stream_frames ;
	long first_difference ;
} EVALUATION ;

typedef struct
{
	long input_used ;
	long output_generated ;
	long calls ;
	const char *error ;
} RUN_RESULT ;

typedef struct
{
	TEST_CASE cases [MAX_SAVED_CASES] ;
	int count ;
} SAVED_CASES ;

static uint64_t
rng_next (uint64_t *state)
{
	uint64_t z = (*state += UINT64_C (0x9e3779b97f4a7c15)) ;

	z = (z ^ (z >> 30)) * UINT64_C (0xbf58476d1ce4e5b9) ;
	z = (z ^ (z >> 27)) * UINT64_C (0x94d049bb133111eb) ;
	return z ^ (z >> 31) ;
}

static double
rng_unit (uint64_t *state)
{
	return (double) (rng_next (state) >> 11) * (1.0 / 9007199254740992.0) ;
}

static long
rng_range (uint64_t *state, long low, long high)
{
	uint64_t span = (uint64_t) (high - low + 1) ;

	return low + (long) (rng_next (state) % span) ;
}

static void
fill_input (float *input, long frames, uint64_t seed)
{
	uint64_t state = seed ;
	double phase [4] ;
	static const double periods [4] = { 211.0, 67.0, 1279.0, 19.0 } ;
	static const double amplitudes [4] = { 0.18, 0.16, 0.14, 0.08 } ;
	long i ;
	int k ;

	for (k = 0 ; k < 4 ; k++)
		phase [k] = 2.0 * M_PI * rng_unit (&state) ;

	for (i = 0 ; i < frames ; i++)
	{
		double value = 0.0 ;

		for (k = 0 ; k < 4 ; k++)
			value += amplitudes [k] * sin (2.0 * M_PI * (double) i / periods [k] + phase [k]) ;

		input [i] = (float) value ;
	}
}

static long
output_capacity (long input_frames, double ratio)
{
	double ratio_guard = 4.0 * fmax (ratio, 1.0 / ratio) ;
	long capacity = (long) ceil ((double) input_frames * ratio + 8192.0 + ratio_guard) ;

	if (capacity < 64)
		capacity = 64 ;

	return capacity ;
}

static int
same_float_bits (float a, float b)
{
	return memcmp (&a, &b, sizeof (float)) == 0 ;
}

static long
first_float_difference (const float *a, const float *b, long frames)
{
	long i ;

	for (i = 0 ; i < frames ; i++)
		if (! same_float_bits (a [i], b [i]))
			return i ;

	return -1 ;
}

static void
mark_failure (EVALUATION *evaluation, const char *reason, long index, double value, double limit)
{
	if (evaluation->failed)
		return ;

	evaluation->failed = 1 ;
	evaluation->reason = reason ;
	evaluation->index = index ;
	evaluation->value = value ;
	evaluation->limit = limit ;
}

static void
check_output (const float *output, long frames, STATS *stats, EVALUATION *evaluation)
{
	long i ;

	for (i = 0 ; i < frames ; i++)
	{
		double value = output [i] ;
		double magnitude = fabs (value) ;

		stats->samples ++ ;

		if (! isfinite (value))
		{
			stats->nonfinite ++ ;
			mark_failure (evaluation, "non-finite sample", i, value, 0.0) ;
			continue ;
		}

		if (magnitude > stats->max_abs)
			stats->max_abs = magnitude ;

		if (magnitude > CLIP_LIMIT)
		{
			stats->clipped ++ ;
			mark_failure (evaluation, "output exceeds full scale", i, value, CLIP_LIMIT) ;
		}

		if (i > 0 && isfinite (output [i - 1]))
		{
			double step = fabs (value - (double) output [i - 1]) ;

			if (step > stats->max_step)
				stats->max_step = step ;

			if (step > STEP_LIMIT)
			{
				stats->steps ++ ;
				mark_failure (evaluation, "adjacent sample step too large", i, step, STEP_LIMIT) ;
			}
		}
	}
}

static RUN_RESULT
run_fixed_stream (const TEST_CASE *test_case, const float *input, long input_frames,
	float *output, long output_frames, long in_chunk, long out_chunk)
{
	SRC_STATE *state ;
	SRC_DATA data ;
	RUN_RESULT result = { 0, 0, 0, NULL } ;
	long input_pos = 0 ;
	long output_pos = 0 ;
	int error = 0 ;

	if ((state = src_new (test_case->converter, 1, &error)) == NULL)
	{
		result.error = src_strerror (error) ;
		return result ;
	}

	if ((error = src_set_ratio (state, test_case->ratio)) != 0)
	{
		result.error = src_strerror (error) ;
		src_delete (state) ;
		return result ;
	}

	while (result.calls < MAX_CALLS)
	{
		long feed = input_pos < input_frames ? input_frames - input_pos : 0 ;
		long available = output_frames - output_pos ;
		long want ;
		long used, generated ;

		if (feed > in_chunk)
			feed = in_chunk ;

		want = out_chunk < available ? out_chunk : available ;
		if (want <= 0)
		{
			result.error = "output buffer exhausted" ;
			break ;
		}

		memset (&data, 0, sizeof (data)) ;
		data.data_in = input + input_pos ;
		data.input_frames = feed ;
		data.data_out = output + output_pos ;
		data.output_frames = want ;
		data.end_of_input = input_pos + feed >= input_frames ;
		data.src_ratio = test_case->ratio ;

		if ((error = src_process (state, &data)) != 0)
		{
			result.error = src_strerror (error) ;
			break ;
		}

		result.calls ++ ;
		used = data.input_frames_used ;
		generated = data.output_frames_gen ;

		if (used < 0 || used > feed || generated < 0 || generated > want)
		{
			result.error = "invalid frame counts returned by src_process" ;
			break ;
		}

		input_pos += used ;
		output_pos += generated ;
		result.input_used = input_pos ;
		result.output_generated = output_pos ;

		if (used == 0 && generated == 0)
		{
			if (input_pos == input_frames)
				break ;
			result.error = "src_process made no progress" ;
			break ;
		}
	}

	if (result.calls >= MAX_CALLS && result.error == NULL)
		result.error = "src_process call limit exceeded" ;

	src_delete (state) ;
	return result ;
}

static RUN_RESULT
run_jump_stream (const JUMP_CASE *jump_case, const float *input, long input_frames,
	float *output, long output_frames, long in_chunk, long out_chunk)
{
	SRC_STATE *state ;
	SRC_DATA data ;
	RUN_RESULT result = { 0, 0, 0, NULL } ;
	long input_pos = 0 ;
	long output_pos = 0 ;
	double ratio = jump_case->base.ratio ;
	int next_jump = 0 ;
	int error = 0 ;

	if ((state = src_new (jump_case->base.converter, 1, &error)) == NULL)
	{
		result.error = src_strerror (error) ;
		return result ;
	}

	if ((error = src_set_ratio (state, ratio)) != 0)
	{
		result.error = src_strerror (error) ;
		src_delete (state) ;
		return result ;
	}

	while (result.calls < MAX_CALLS)
	{
		long feed = input_pos < input_frames ? input_frames - input_pos : 0 ;
		long available = output_frames - output_pos ;
		long want ;
		long used, generated ;

		while (next_jump < jump_case->jump_count && input_pos >= jump_case->jump_at [next_jump])
		{
			ratio = jump_case->jump_ratio [next_jump] ;
			if ((error = src_set_ratio (state, ratio)) != 0)
			{
				result.error = src_strerror (error) ;
				break ;
			}
			next_jump ++ ;
		}
		if (result.error != NULL)
			break ;

		if (feed > in_chunk)
			feed = in_chunk ;

		want = out_chunk < available ? out_chunk : available ;
		if (want <= 0)
		{
			result.error = "output buffer exhausted" ;
			break ;
		}

		memset (&data, 0, sizeof (data)) ;
		data.data_in = input + input_pos ;
		data.input_frames = feed ;
		data.data_out = output + output_pos ;
		data.output_frames = want ;
		data.end_of_input = input_pos + feed >= input_frames ;
		data.src_ratio = ratio ;

		if ((error = src_process (state, &data)) != 0)
		{
			result.error = src_strerror (error) ;
			break ;
		}

		result.calls ++ ;
		used = data.input_frames_used ;
		generated = data.output_frames_gen ;

		if (used < 0 || used > feed || generated < 0 || generated > want)
		{
			result.error = "invalid frame counts returned by src_process" ;
			break ;
		}

		input_pos += used ;
		output_pos += generated ;
		result.input_used = input_pos ;
		result.output_generated = output_pos ;

		if (used == 0 && generated == 0)
		{
			if (input_pos == input_frames)
				break ;
			result.error = "src_process made no progress" ;
			break ;
		}
	}

	if (result.calls >= MAX_CALLS && result.error == NULL)
		result.error = "src_process call limit exceeded" ;

	src_delete (state) ;
	return result ;
}

static void
evaluate_fixed (const TEST_CASE *test_case, STATS *stats, EVALUATION *evaluation)
{
	STATS local_stats ;
	float *input_storage = NULL ;
	float *input ;
	float *reference = NULL ;
	float *stream_a = NULL ;
	float *stream_b = NULL ;
	long capacity ;
	long tolerance ;
	long common_frames ;
	long difference ;
	RUN_RESULT run ;

	memset (evaluation, 0, sizeof (*evaluation)) ;
	evaluation->first_difference = -1 ;
	if (stats == NULL)
	{
		memset (&local_stats, 0, sizeof (local_stats)) ;
		stats = &local_stats ;
	}

#ifdef PROPERTY_TEST_INJECT
	if (test_case->input_frames >= 50 && test_case->ratio >= 2.0)
	{
		mark_failure (evaluation, "injected shrinker fault", 0, 1.0, 0.0) ;
		return ;
	}
#endif

	capacity = output_capacity (test_case->input_frames, test_case->ratio) ;
	input_storage = calloc ((size_t) test_case->input_frames + GUARD_FRAMES, sizeof (float)) ;
	reference = calloc ((size_t) capacity, sizeof (float)) ;
	stream_a = calloc ((size_t) capacity, sizeof (float)) ;
	stream_b = calloc ((size_t) capacity, sizeof (float)) ;

	if (input_storage == NULL || reference == NULL || stream_a == NULL || stream_b == NULL)
	{
		mark_failure (evaluation, "allocation failed", 0, 0.0, 0.0) ;
		goto cleanup ;
	}

	input = input_storage + GUARD_FRAMES ;
	for (difference = 0 ; difference < GUARD_FRAMES ; difference++)
		input_storage [difference] = 2.0f ;
	fill_input (input, test_case->input_frames, test_case->seed) ;

	run = run_fixed_stream (test_case, input, test_case->input_frames, reference, capacity,
		test_case->input_frames, capacity) ;
	if (run.error != NULL)
	{
		mark_failure (evaluation, run.error, run.input_used, 0.0, 0.0) ;
		goto cleanup ;
	}
	evaluation->reference_frames = run.output_generated ;
	if (run.input_used != test_case->input_frames)
	{
		mark_failure (evaluation, "reference run left input unconsumed", run.input_used,
			(double) run.input_used, (double) test_case->input_frames) ;
		goto cleanup ;
	}

	run = run_fixed_stream (test_case, input, test_case->input_frames, stream_a, capacity,
		test_case->in_chunk, test_case->out_chunk) ;
	if (run.error != NULL)
	{
		mark_failure (evaluation, run.error, run.input_used, 0.0, 0.0) ;
		goto cleanup ;
	}
	evaluation->stream_frames = run.output_generated ;
	if (run.input_used != test_case->input_frames)
	{
		mark_failure (evaluation, "streaming run left input unconsumed", run.input_used,
			(double) run.input_used, (double) test_case->input_frames) ;
		goto cleanup ;
	}

	run = run_fixed_stream (test_case, input, test_case->input_frames, stream_b, capacity,
		test_case->in_chunk, test_case->out_chunk) ;
	if (run.error != NULL)
	{
		mark_failure (evaluation, run.error, run.input_used, 0.0, 0.0) ;
		goto cleanup ;
	}

	check_output (reference, evaluation->reference_frames, stats, evaluation) ;
	check_output (stream_a, evaluation->stream_frames, stats, evaluation) ;

	if (run.output_generated != evaluation->stream_frames)
	{
		mark_failure (evaluation, "deterministic run length mismatch", 0,
			(double) run.output_generated, (double) evaluation->stream_frames) ;
		goto cleanup ;
	}

	difference = first_float_difference (stream_a, stream_b, evaluation->stream_frames) ;
	if (difference >= 0)
	{
		mark_failure (evaluation, "two runs differ sample by sample", difference,
			stream_a [difference], stream_b [difference]) ;
		goto cleanup ;
	}

	tolerance = (long) ceil (2.0 * fmax (test_case->ratio, 1.0 / test_case->ratio)) ;
	if (labs (evaluation->reference_frames - evaluation->stream_frames) > tolerance)
	{
		mark_failure (evaluation, "chunked latency outside tolerance", 0,
			(double) evaluation->stream_frames, (double) evaluation->reference_frames) ;
		goto cleanup ;
	}

	common_frames = evaluation->reference_frames < evaluation->stream_frames ?
		evaluation->reference_frames : evaluation->stream_frames ;
	difference = first_float_difference (reference, stream_a, common_frames) ;
	if (difference >= 0)
	{
		evaluation->first_difference = difference ;
		mark_failure (evaluation, "chunk size changes output samples", difference,
			reference [difference], stream_a [difference]) ;
	}

cleanup:
	free (input_storage) ;
	free (reference) ;
	free (stream_a) ;
	free (stream_b) ;
}

static void
evaluate_jump (const JUMP_CASE *jump_case, STATS *stats, EVALUATION *evaluation)
{
	STATS local_stats ;
	float *input_storage = NULL ;
	float *input ;
	float *stream_a = NULL ;
	float *stream_b = NULL ;
	double max_ratio = jump_case->base.ratio ;
	long capacity ;
	long difference ;
	RUN_RESULT run ;
	int k ;

	memset (evaluation, 0, sizeof (*evaluation)) ;
	evaluation->first_difference = -1 ;
	if (stats == NULL)
	{
		memset (&local_stats, 0, sizeof (local_stats)) ;
		stats = &local_stats ;
	}

	for (k = 0 ; k < jump_case->jump_count ; k++)
		if (jump_case->jump_ratio [k] > max_ratio)
			max_ratio = jump_case->jump_ratio [k] ;

	capacity = output_capacity (jump_case->base.input_frames, max_ratio) ;
	input_storage = calloc ((size_t) jump_case->base.input_frames + GUARD_FRAMES, sizeof (float)) ;
	stream_a = calloc ((size_t) capacity, sizeof (float)) ;
	stream_b = calloc ((size_t) capacity, sizeof (float)) ;

	if (input_storage == NULL || stream_a == NULL || stream_b == NULL)
	{
		mark_failure (evaluation, "allocation failed", 0, 0.0, 0.0) ;
		goto cleanup ;
	}

	input = input_storage + GUARD_FRAMES ;
	for (difference = 0 ; difference < GUARD_FRAMES ; difference++)
		input_storage [difference] = 2.0f ;
	fill_input (input, jump_case->base.input_frames, jump_case->base.seed) ;

	run = run_jump_stream (jump_case, input, jump_case->base.input_frames, stream_a, capacity,
		jump_case->base.in_chunk, jump_case->base.out_chunk) ;
	if (run.error != NULL)
	{
		mark_failure (evaluation, run.error, run.input_used, 0.0, 0.0) ;
		goto cleanup ;
	}
	evaluation->stream_frames = run.output_generated ;
	if (run.input_used != jump_case->base.input_frames)
	{
		mark_failure (evaluation, "ratio-jump run left input unconsumed", run.input_used,
			(double) run.input_used, (double) jump_case->base.input_frames) ;
		goto cleanup ;
	}

	run = run_jump_stream (jump_case, input, jump_case->base.input_frames, stream_b, capacity,
		jump_case->base.in_chunk, jump_case->base.out_chunk) ;
	if (run.error != NULL)
	{
		mark_failure (evaluation, run.error, run.input_used, 0.0, 0.0) ;
		goto cleanup ;
	}

	check_output (stream_a, evaluation->stream_frames, stats, evaluation) ;

	if (run.output_generated != evaluation->stream_frames)
	{
		mark_failure (evaluation, "ratio-jump deterministic length mismatch", 0,
			(double) run.output_generated, (double) evaluation->stream_frames) ;
		goto cleanup ;
	}

	difference = first_float_difference (stream_a, stream_b, evaluation->stream_frames) ;
	if (difference >= 0)
	{
		mark_failure (evaluation, "ratio-jump runs differ sample by sample", difference,
			stream_a [difference], stream_b [difference]) ;
	}

cleanup:
	free (input_storage) ;
	free (stream_a) ;
	free (stream_b) ;
}

static int
known_linear_edge (const TEST_CASE *test_case)
{
	return test_case->converter == SRC_LINEAR &&
		(test_case->input_frames == 1 || test_case->in_chunk == 1) ;
}

static int
case_fails (const TEST_CASE *test_case)
{
	EVALUATION evaluation ;

	evaluate_fixed (test_case, NULL, &evaluation) ;
	return evaluation.failed ;
}

static void
shrink_case (TEST_CASE *test_case, int *steps)
{
	static const double ratio_candidates [] =
	{
		1.0, 2.0, 0.5, 4.0, 0.25, 8.0, 0.125, 16.0,
		0.0625, 32.0, 0.03125, 64.0, 0.015625, 128.0,
		0.0078125, 256.0, 0.00390625
	} ;
	TEST_CASE candidate ;
	int current_ratio_index = -1 ;
	int changed = 1 ;
	int k ;

	*steps = 0 ;
	for (k = 0 ; k < (int) (sizeof (ratio_candidates) / sizeof (ratio_candidates [0])) ; k++)
		if (fabs (test_case->ratio - ratio_candidates [k]) < 1e-15)
		{
			current_ratio_index = k ;
			break ;
		}

	while (changed)
	{
		changed = 0 ;

		while (test_case->input_frames > 1)
		{
			candidate = *test_case ;
			candidate.input_frames /= 2 ;
			if (! case_fails (&candidate))
				break ;
			*test_case = candidate ;
			(*steps) ++ ;
			changed = 1 ;
		}

		for (k = 0 ; k < (int) (sizeof (ratio_candidates) / sizeof (ratio_candidates [0])) ; k++)
		{
			if (current_ratio_index >= 0 && k >= current_ratio_index)
				break ;
			if (fabs (test_case->ratio - ratio_candidates [k]) < 1e-15)
				continue ;

			candidate = *test_case ;
			candidate.ratio = ratio_candidates [k] ;
			if (! case_fails (&candidate))
				continue ;
			*test_case = candidate ;
			current_ratio_index = k ;
			(*steps) ++ ;
			changed = 1 ;
			break ;
		}

		while (test_case->in_chunk > 1)
		{
			candidate = *test_case ;
			candidate.in_chunk /= 2 ;
			if (! case_fails (&candidate))
				break ;
			*test_case = candidate ;
			(*steps) ++ ;
			changed = 1 ;
		}

		while (test_case->out_chunk > 1)
		{
			candidate = *test_case ;
			candidate.out_chunk /= 2 ;
			if (! case_fails (&candidate))
				break ;
			*test_case = candidate ;
			(*steps) ++ ;
			changed = 1 ;
		}
	}
}

static void
print_case (FILE *file, const TEST_CASE *test_case)
{
	fprintf (file, "converter=%d (%s) input_frames=%ld ratio=%.17g "
		"in_chunk=%ld out_chunk=%ld seed=0x%016llx\n",
		test_case->converter, src_get_name (test_case->converter),
		test_case->input_frames, test_case->ratio,
		test_case->in_chunk, test_case->out_chunk,
		(unsigned long long) test_case->seed) ;
}

static void
save_case (SAVED_CASES *saved, const TEST_CASE *test_case)
{
	int k ;

	for (k = 0 ; k < saved->count ; k++)
		if (memcmp (&saved->cases [k], test_case, sizeof (*test_case)) == 0)
			return ;

	if (saved->count < MAX_SAVED_CASES)
		saved->cases [saved->count ++] = *test_case ;
}

static void
write_saved_cases (const char *path, const SAVED_CASES *saved)
{
	FILE *file ;
	int k ;

	if ((file = fopen (path, "w")) == NULL)
	{
		fprintf (stderr, "Could not open %s for writing\n", path) ;
		return ;
	}

	fprintf (file, "# Minimal counterexamples from libsamplerate property tests\n") ;
	fprintf (file, "# converter 4 is SRC_LINEAR\n") ;

	for (k = 0 ; k < saved->count ; k++)
	{
		EVALUATION evaluation ;
		STATS stats ;
		float input [2] = { 0.0f, 0.0f } ;

		memset (&stats, 0, sizeof (stats)) ;
		evaluate_fixed (&saved->cases [k], &stats, &evaluation) ;
		fill_input (input, 1, saved->cases [k].seed) ;
		print_case (file, &saved->cases [k]) ;
		fprintf (file, "failure=%s sample_index=%ld observed=%.9g limit=%.9g "
			"reference_frames=%ld chunked_frames=%ld first_difference=%ld input[0]=%.9g\n",
			evaluation.reason == NULL ? "none" : evaluation.reason,
			evaluation.index, evaluation.value, evaluation.limit,
			evaluation.reference_frames, evaluation.stream_frames,
			evaluation.first_difference, input [0]) ;
	}

	fclose (file) ;
}

static TEST_CASE
random_case (uint64_t *state)
{
	TEST_CASE test_case ;
	double max_input ;
	long max_frames ;
	static const long chunks [] = { 2, 3, 5, 7, 16, 31, 64, 127, 257, 509, 1024, 2048, 4096 } ;

	memset (&test_case, 0, sizeof (test_case)) ;
	test_case.converter = (int) (rng_next (state) % 5) ;
	test_case.ratio = exp2 (-8.0 + 16.0 * rng_unit (state)) ;
	test_case.seed = rng_next (state) ;

	max_input = 32768.0 ;
	if (test_case.ratio > 1.0)
		max_input = fmin (max_input, 120000.0 / test_case.ratio) ;
	max_frames = (long) floor (max_input) ;
	if (max_frames < 4)
		max_frames = 4 ;

	test_case.input_frames = rng_range (state, 2, max_frames) ;
	test_case.in_chunk = chunks [rng_next (state) % (sizeof (chunks) / sizeof (chunks [0]))] ;
	test_case.out_chunk = chunks [rng_next (state) % (sizeof (chunks) / sizeof (chunks [0]))] ;

	return test_case ;
}

static JUMP_CASE
random_jump_case (uint64_t *state)
{
	JUMP_CASE jump_case ;
	int k ;

	memset (&jump_case, 0, sizeof (jump_case)) ;
	jump_case.base.converter = (int) (rng_next (state) % 5) ;
	jump_case.base.ratio = exp2 (-5.0 + 10.0 * rng_unit (state)) ;
	jump_case.base.input_frames = rng_range (state, 2048, 8192) ;
	jump_case.base.in_chunk = rng_range (state, 16, 1024) ;
	jump_case.base.out_chunk = rng_range (state, 16, 1024) ;
	jump_case.base.seed = rng_next (state) ;
	jump_case.jump_count = 4 ;

	for (k = 0 ; k < jump_case.jump_count ; k++)
	{
		jump_case.jump_at [k] = jump_case.base.input_frames * (2 * k + 1) / 8 ;
		jump_case.jump_ratio [k] = exp2 (-5.0 + 10.0 * rng_unit (state)) ;
	}

	return jump_case ;
}

static TEST_CASE
random_short_case (uint64_t *state, int index)
{
	TEST_CASE test_case ;

	memset (&test_case, 0, sizeof (test_case)) ;
	test_case.converter = index % 5 ;
	test_case.ratio = exp2 (-8.0 + 16.0 * rng_unit (state)) ;
	test_case.input_frames = rng_range (state, 1, 3) ;
	test_case.in_chunk = rng_range (state, 1, 4) ;
	test_case.out_chunk = rng_range (state, 1, 4) ;
	test_case.seed = rng_next (state) ;

	return test_case ;
}

static void
report_failure (const TEST_CASE *test_case, const EVALUATION *evaluation, const char *kind)
{
	printf ("  %s failure: ", kind) ;
	print_case (stdout, test_case) ;
	printf ("    reason=%s index=%ld observed=%.9g limit=%.9g "
		"reference_frames=%ld chunked_frames=%ld first_difference=%ld\n",
		evaluation->reason == NULL ? "unknown" : evaluation->reason,
		evaluation->index, evaluation->value, evaluation->limit,
		evaluation->reference_frames, evaluation->stream_frames,
		evaluation->first_difference) ;
}

static int
contains_unexpected (int random_unexpected, int jump_unexpected, int long_unexpected, int short_unexpected)
{
	return random_unexpected + jump_unexpected + long_unexpected + short_unexpected ;
}

#ifdef PROPERTY_TEST_INJECT
static int
shrinker_selftest (const char *save_path)
{
	SAVED_CASES saved = { { { 0 } }, 0 } ;
	int passed = 0 ;
	int k ;

	for (k = 0 ; k < 3 ; k++)
	{
		static const int converters [3] = { 2, 3, 0 } ;
		TEST_CASE test_case ;
		EVALUATION evaluation ;
		int steps = 0 ;

		memset (&test_case, 0, sizeof (test_case)) ;
		test_case.converter = converters [k] ;
		test_case.ratio = exp2 (7.0 - 2.0 * k) ;
		test_case.input_frames = 8192L >> k ;
		test_case.in_chunk = 300 - 100 * k ;
		test_case.out_chunk = 300 - 100 * k ;
		test_case.seed = UINT64_C (7) ;

		evaluate_fixed (&test_case, NULL, &evaluation) ;
		if (! evaluation.failed)
		{
			fprintf (stderr, "Injected case %d did not fail before shrinking\n", k) ;
			continue ;
		}

		shrink_case (&test_case, &steps) ;
		evaluate_fixed (&test_case, NULL, &evaluation) ;

		printf ("injected case %d: frames=%ld ratio=%.17g in_chunk=%ld out_chunk=%ld "
			"steps=%d failure=%s\n",
			k, test_case.input_frames, test_case.ratio,
			test_case.in_chunk, test_case.out_chunk, steps,
			evaluation.reason == NULL ? "none" : evaluation.reason) ;

		if (test_case.input_frames <= 64 && fabs (test_case.ratio - 2.0) < 1e-15 &&
			test_case.in_chunk == 1 && test_case.out_chunk == 1)
			passed ++ ;

		save_case (&saved, &test_case) ;
	}

	if (save_path != NULL)
		write_saved_cases (save_path, &saved) ;

	printf ("shrinker self test: %d/3 reached the minimal fault region\n", passed) ;
	return passed == 3 ? 0 : 1 ;
}
#endif

int
main (int argc, char **argv)
{
	uint64_t seed = DEFAULT_SEED ;
	const char *save_path = NULL ;
	STATS stats ;
	SAVED_CASES saved = { { { 0 } }, 0 } ;
	int strict = 0 ;
	int random_passed = 0, random_known = 0, random_unexpected = 0 ;
	int jump_passed = 0, jump_unexpected = 0 ;
	int long_passed = 0, long_unexpected = 0 ;
	int short_passed = 0, short_known = 0, short_unexpected = 0 ;
	int explicit_known = 0 ;
	int shrunk_known = 0 ;
	int shrink_steps = 0 ;
	uint64_t state ;
	int i ;

#ifdef PROPERTY_TEST_INJECT
	int selftest = 0 ;
#endif

	for (i = 1 ; i < argc ; i++)
	{
		if (strncmp (argv [i], "--seed=", 7) == 0)
			seed = strtoull (argv [i] + 7, NULL, 0) ;
		else if (strncmp (argv [i], "--save-cases=", 13) == 0)
			save_path = argv [i] + 13 ;
		else if (strcmp (argv [i], "--strict") == 0)
			strict = 1 ;
#ifdef PROPERTY_TEST_INJECT
		else if (strcmp (argv [i], "--selftest") == 0)
			selftest = 1 ;
#endif
		else
		{
			fprintf (stderr, "Unknown argument: %s\n", argv [i]) ;
			return 2 ;
		}
	}

#ifdef PROPERTY_TEST_INJECT
	if (selftest)
		return shrinker_selftest (save_path) ;
#endif

	memset (&stats, 0, sizeof (stats)) ;
	state = seed ;

	printf ("libsamplerate property test, seed = 0x%016llx, library version %s\n",
		(unsigned long long) seed, src_get_version ()) ;

	printf ("\nPhase 1: %d random (converter, ratio, length, chunk) cases\n", RANDOM_CASES) ;
	for (i = 0 ; i < RANDOM_CASES ; i++)
	{
		TEST_CASE test_case = random_case (&state) ;
		EVALUATION evaluation ;

		evaluate_fixed (&test_case, &stats, &evaluation) ;
		if (! evaluation.failed)
			random_passed ++ ;
		else if (known_linear_edge (&test_case))
		{
			random_known ++ ;
			if (! shrunk_known)
			{
				shrink_case (&test_case, &shrink_steps) ;
				evaluate_fixed (&test_case, NULL, &evaluation) ;
				report_failure (&test_case, &evaluation, "known SRC_LINEAR startup") ;
				printf ("    shrunk in %d step(s)\n", shrink_steps) ;
				save_case (&saved, &test_case) ;
				shrunk_known = 1 ;
			}
		}
		else
		{
			random_unexpected ++ ;
			report_failure (&test_case, &evaluation, "random") ;
		}

		if ((i + 1) % 100 == 0)
			printf ("  ... random case %d/%d\n", i + 1, RANDOM_CASES) ;
	}
	printf ("  random suite: %d/%d passed, %d known, %d unexpected; "
		"worst step %.6f, worst |x| %.6f\n",
		random_passed, RANDOM_CASES, random_known, random_unexpected,
		stats.max_step, stats.max_abs) ;

	printf ("\nPhase 2: %d mid-stream ratio jump cases\n", JUMP_CASES) ;
	for (i = 0 ; i < JUMP_CASES ; i++)
	{
		JUMP_CASE jump_case = random_jump_case (&state) ;
		EVALUATION evaluation ;

		evaluate_jump (&jump_case, &stats, &evaluation) ;
		if (! evaluation.failed)
			jump_passed ++ ;
		else
		{
			jump_unexpected ++ ;
			report_failure (&jump_case.base, &evaluation, "ratio-jump") ;
		}
	}
	printf ("  ratio jump suite: %d/%d passed, %d unexpected\n",
		jump_passed, JUMP_CASES, jump_unexpected) ;

	printf ("\nPhase 3: very long streams (%ld input frames per converter)\n", LONG_FRAMES) ;
	for (i = 0 ; i < 5 ; i++)
	{
		TEST_CASE test_case ;
		EVALUATION evaluation ;

		memset (&test_case, 0, sizeof (test_case)) ;
		test_case.converter = i ;
		test_case.ratio = 1.0 ;
		test_case.input_frames = LONG_FRAMES ;
		test_case.in_chunk = 4096 ;
		test_case.out_chunk = 4096 ;
		test_case.seed = seed ^ UINT64_C (0x5a5a5a5a00000000) ^ (uint64_t) i ;

		evaluate_fixed (&test_case, &stats, &evaluation) ;
		if (! evaluation.failed)
			long_passed ++ ;
		else
		{
			long_unexpected ++ ;
			report_failure (&test_case, &evaluation, "long-stream") ;
		}
	}
	printf ("  long stream suite: %d/5 passed, %d unexpected\n", long_passed, long_unexpected) ;

	printf ("\nPhase 4: %d very short streams (1..3 input frames)\n", SHORT_CASES) ;
	for (i = 0 ; i < SHORT_CASES ; i++)
	{
		TEST_CASE test_case = random_short_case (&state, i) ;
		EVALUATION evaluation ;

		evaluate_fixed (&test_case, &stats, &evaluation) ;
		if (! evaluation.failed)
			short_passed ++ ;
		else if (known_linear_edge (&test_case))
		{
			short_known ++ ;
			if (! shrunk_known)
			{
				shrink_case (&test_case, &shrink_steps) ;
				evaluate_fixed (&test_case, NULL, &evaluation) ;
				report_failure (&test_case, &evaluation, "known SRC_LINEAR startup") ;
				printf ("    shrunk in %d step(s)\n", shrink_steps) ;
				save_case (&saved, &test_case) ;
				shrunk_known = 1 ;
			}
		}
		else
		{
			short_unexpected ++ ;
			report_failure (&test_case, &evaluation, "short-stream") ;
		}
	}
	printf ("  short stream suite: %d/%d passed, %d known, %d unexpected\n",
		short_passed, SHORT_CASES, short_known, short_unexpected) ;

	printf ("\nPhase 5: explicit SRC_LINEAR single-frame startup edge\n") ;
	{
		TEST_CASE edge ;
		EVALUATION evaluation ;

		memset (&edge, 0, sizeof (edge)) ;
		edge.converter = SRC_LINEAR ;
		edge.ratio = 7.2303493271516563 ;
		edge.input_frames = 2 ;
		edge.in_chunk = 1 ;
		edge.out_chunk = 2 ;
		edge.seed = seed ^ UINT64_C (0x4c494e4541520001) ;

		evaluate_fixed (&edge, NULL, &evaluation) ;
		if (evaluation.failed)
		{
			explicit_known = 1 ;
			shrink_case (&edge, &shrink_steps) ;
			evaluate_fixed (&edge, NULL, &evaluation) ;
			report_failure (&edge, &evaluation, "explicit SRC_LINEAR edge") ;
			printf ("    minimal case after %d shrink step(s)\n", shrink_steps) ;
			save_case (&saved, &edge) ;
		}
		else
		{
			printf ("  edge did not reproduce\n") ;
		}
	}

	if (save_path != NULL)
		write_saved_cases (save_path, &saved) ;

	printf ("\nSummary: %d passed, %d known SRC_LINEAR failures, %d unexpected failures\n",
		random_passed + jump_passed + long_passed + short_passed,
		random_known + short_known + explicit_known,
		contains_unexpected (random_unexpected, jump_unexpected, long_unexpected, short_unexpected)) ;
	printf ("         saved minimal cases: %d\n", saved.count) ;

	if (contains_unexpected (random_unexpected, jump_unexpected, long_unexpected, short_unexpected))
		return 1 ;
	if (strict && random_known + short_known + explicit_known > 0)
		return 1 ;

	return 0 ;
}
