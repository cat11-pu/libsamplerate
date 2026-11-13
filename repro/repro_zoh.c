#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main (int argc, char **argv)
{	int ch = atoi (argv[1]) ; double ratio = atof (argv[2]) ; long n = atol (argv[3]) ;
	float *in = calloc (n*ch, sizeof(float)), *out = calloc(64*ch, sizeof(float)) ;
	for (long i=0;i<n*ch;i++) in[i]=0.25f*(float)(i+1) ;
	int err=0; SRC_STATE *st=src_new(SRC_ZERO_ORDER_HOLD,ch,&err) ;
	SRC_DATA d; memset(&d,0,sizeof d);
	d.data_in=in; d.data_out=out; d.input_frames=n; d.output_frames=64;
	d.src_ratio=ratio; d.end_of_input=1 ;
	err=src_process(st,&d);
	printf("err=%d used=%ld gen=%ld out0=%.7f\n",err,d.input_frames_used,d.output_frames_gen,d.output_frames_gen?out[0]:0.f);
	src_delete(st); free(in); free(out); return 0; }
