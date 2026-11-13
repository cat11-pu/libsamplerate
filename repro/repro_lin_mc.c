#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main (int argc, char **argv)
{	int ch = atoi (argv[1]) ;
	float *in = calloc (ch, sizeof(float)), *out = calloc(64*ch, sizeof(float)) ;
	for (int i=0;i<ch;i++) in[i]=(float)(i+1) ;
	int err=0; SRC_STATE *st=src_new(SRC_LINEAR,ch,&err) ;
	SRC_DATA d; memset(&d,0,sizeof d);
	d.data_in=in; d.data_out=out; d.input_frames=1; d.output_frames=64;
	d.src_ratio=1.0; d.end_of_input=0 ;
	err=src_process(st,&d);
	printf("err=%d used=%ld gen=%ld out0=%g\n",err,d.input_frames_used,d.output_frames_gen,d.output_frames_gen?out[0]:0.f);
	src_delete(st); free(in); free(out); return 0; }
