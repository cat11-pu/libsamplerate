#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main (void)
{
	float in[2]={0.25f,0.5f}, out[512];
	int err=0; SRC_STATE *st=src_new(SRC_SINC_FASTEST,1,&err);
	SRC_DATA d;
	/* feed frame 1 */
	memset(&d,0,sizeof d); d.data_in=in; d.data_out=out; d.input_frames=1;
	d.output_frames=512; d.src_ratio=1.0; d.end_of_input=0;
	printf("feed1: %d used=%ld gen=%ld\n", src_process(st,&d), d.input_frames_used, d.output_frames_gen);
	/* feed frame 2 */
	memset(&d,0,sizeof d); d.data_in=in+1; d.data_out=out; d.input_frames=1;
	d.output_frames=512; d.src_ratio=1.0; d.end_of_input=0;
	printf("feed2: %d used=%ld gen=%ld\n", src_process(st,&d), d.input_frames_used, d.output_frames_gen);
	/* drain attempts */
	for (int i=0;i<5;i++){
	memset(&d,0,sizeof d); d.data_in=NULL; d.data_out=out; d.input_frames=0;
	d.output_frames=512; d.src_ratio=1.0; d.end_of_input=1;
	int e=src_process(st,&d);
	printf("drain%d: %d used=%ld gen=%ld\n", i,e,d.input_frames_used,d.output_frames_gen);
	if(d.output_frames_gen==0) break;
	}
	src_delete(st);
	/* one shot */
	st=src_new(SRC_SINC_FASTEST,1,&err);
	memset(&d,0,sizeof d); d.data_in=in; d.data_out=out; d.input_frames=2;
	d.output_frames=512; d.src_ratio=1.0; d.end_of_input=1;
	printf("oneshot: %d used=%ld gen=%ld out0=%g out1=%g\n", src_process(st,&d),
		d.input_frames_used,d.output_frames_gen,out[0],d.output_frames_gen>1?out[1]:0.f);
	src_delete(st);
	return 0;
}
