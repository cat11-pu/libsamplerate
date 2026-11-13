#include <stdio.h>
#include <string.h>
#include <samplerate.h>
int main(void){
	float in[2]={0.25f,0.5f}, out[64]; int err=0;
	SRC_STATE* st=src_new(SRC_SINC_FASTEST,1,&err);
	SRC_DATA d;
	memset(&d,0,sizeof d);
	d.data_in=in; d.data_out=out; d.input_frames=1; d.output_frames=64;
	d.src_ratio=1.0; d.end_of_input=0;
	printf("feed1 e=%d used=%ld gen=%ld\n",src_process(st,&d),d.input_frames_used,d.output_frames_gen);
	memset(&d,0,sizeof d);
	d.data_in=in+1; d.data_out=out; d.input_frames=1; d.output_frames=64;
	d.src_ratio=1.0; d.end_of_input=0;
	printf("feed2 e=%d used=%ld gen=%ld\n",src_process(st,&d),d.input_frames_used,d.output_frames_gen);
	memset(&d,0,sizeof d);
	d.data_in=NULL; d.data_out=out; d.input_frames=0; d.output_frames=64;
	d.src_ratio=1.0; d.end_of_input=1;
	printf("drain e=%d used=%ld gen=%ld out0=%.7f\n",src_process(st,&d),d.input_frames_used,d.output_frames_gen,out[0]);
	return 0;}
