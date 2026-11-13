#include <stdio.h>
#include <string.h>
#include <samplerate.h>
int main(void){
	float in[2]={0.25f,0.5f}, out[64]; int err=0;
	SRC_STATE* st=src_new(SRC_SINC_FASTEST,1,&err);
	SRC_DATA d; memset(&d,0,sizeof d);
	d.data_in=in; d.data_out=out; d.input_frames=2; d.output_frames=64;
	d.src_ratio=1.0; d.end_of_input=1;
	int e=src_process(st,&d);
	printf("e=%d used=%ld gen=%ld out0=%.7f out1=%.7f\n",e,d.input_frames_used,d.output_frames_gen,out[0],d.output_frames_gen>1?out[1]:0.f);
	return 0;}
