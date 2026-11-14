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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <samplerate.h>

static int
reproduce_with_guard (void)
{
	float storage [2] = { 2.0f, 0.43841898f } ;
	float input = storage [1] ;
	float output = 0.0f ;
	SRC_STATE *state ;
	SRC_DATA data ;
	int error = 0 ;
	int reproduced ;

	storage [0] = 2.0f ;
	memset (&data, 0, sizeof (data)) ;
	data.data_in = &storage [1] ;
	data.input_frames = 1 ;
	data.data_out = &output ;
	data.output_frames = 1 ;
	data.end_of_input = 1 ;
	data.src_ratio = 1.0 ;

	if ((state = src_new (SRC_LINEAR, 1, &error)) == NULL)
	{
		fprintf (stderr, "src_new failed: %s\n", src_strerror (error)) ;
		return 2 ;
	}

	error = src_process (state, &data) ;
	src_delete (state) ;

	printf ("SRC_LINEAR minimal startup counterexample\n") ;
	printf ("  converter=4 ratio=1 input_frames=1 in_chunk=1 out_chunk=1\n") ;
	printf ("  input[0]=%.9g previous_guard=%.9g\n", input, storage [0]) ;
	printf ("  src_process=%d used=%ld generated=%ld output[0]=%.9g\n",
		error, data.input_frames_used, data.output_frames_gen, output) ;

	reproduced = error == 0 && data.output_frames_gen == 1 && fabsf (output) > 1.0f ;
	printf ("  reproduced=%s\n", reproduced ? "yes" : "no") ;
	return reproduced ? 0 : 1 ;
}

static int
reproduce_without_guard (void)
{
	float *input = malloc (sizeof (float)) ;
	float output = 0.0f ;
	SRC_STATE *state ;
	SRC_DATA data ;
	int error = 0 ;

	if (input == NULL)
		return 2 ;

	input [0] = 0.43841898f ;
	memset (&data, 0, sizeof (data)) ;
	data.data_in = input ;
	data.input_frames = 1 ;
	data.data_out = &output ;
	data.output_frames = 1 ;
	data.end_of_input = 1 ;
	data.src_ratio = 1.0 ;

	if ((state = src_new (SRC_LINEAR, 1, &error)) == NULL)
	{
		free (input) ;
		return 2 ;
	}

	printf ("Running unguarded one-sample probe; use an ASan build to detect the read.\n") ;
	fflush (stdout) ;
	error = src_process (state, &data) ;
	src_delete (state) ;
	printf ("  src_process=%d used=%ld generated=%ld output[0]=%.9g\n",
		error, data.input_frames_used, data.output_frames_gen, output) ;
	free (input) ;
	return error == 0 ? 0 : 1 ;
}

int
main (int argc, char **argv)
{
	if (argc == 2 && strcmp (argv [1], "--asan-probe") == 0)
		return reproduce_without_guard () ;

	if (argc != 1)
	{
		fprintf (stderr, "Usage: %s [--asan-probe]\n", argv [0]) ;
		return 2 ;
	}

	return reproduce_with_guard () ;
}
