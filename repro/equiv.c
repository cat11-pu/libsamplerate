#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>
/* one-shot vs chunked (with drain) on a LONG input; chunk invariance must hold */
int main(int argc,char**argv){
	int conv=atoi(argv[1]); long chunk=atol(argv[2]); double ratio=atof(argv[3]);
	long N=20000;
	float *in=malloc(N*sizeof(float));
	long cap=(long)(N*ratio)+1024;
	float *o1=malloc(cap*sizeof(float)),*o2=malloc(cap*sizeof(float));
	for(long i=0;i<N;i++) in[i]=0.5f*sinf(i*0.01f)+0.1f*sinf(i*0.037f);
	int err; SRC_DATA d;
	SRC_STATE* s1=src_new(conv,1,&err);
	memset(&d,0,sizeof d); d.data_in=in; d.data_out=o1; d.input_frames=N;
	d.output_frames=cap; d.src_ratio=ratio; d.end_of_input=1;
	src_process(s1,&d); long g1=d.output_frames_gen,u1=d.input_frames_used;
	SRC_STATE* s2=src_new(conv,1,&err); long pos=0,g2=0;
	while(pos<N){
		long take=N-pos<chunk?N-pos:chunk;
		memset(&d,0,sizeof d); d.data_in=in+pos; d.data_out=o2+g2;
		d.input_frames=take; d.output_frames=cap-g2; d.src_ratio=ratio; d.end_of_input=0;
		src_process(s2,&d); pos+=d.input_frames_used; g2+=d.output_frames_gen;
	}
	for(int z=0;z<100000;z++){
		memset(&d,0,sizeof d); d.data_in=NULL; d.data_out=o2+g2;
		d.input_frames=0; d.output_frames=cap-g2; d.src_ratio=ratio; d.end_of_input=1;
		src_process(s2,&d); g2+=d.output_frames_gen;
		if(d.output_frames_gen==0) break;
	}
	long mismatch=0; long mmin=g1<g2?g1:g2;
	for(long i=0;i<mmin;i++) if(memcmp(&o1[i],&o2[i],sizeof(float))){mismatch++; if(mismatch<=3) printf("  diff@%ld %.9g vs %.9g\n",i,o1[i],o2[i]);}
	printf("conv=%d chunk=%ld r=%.2f oneshot(u=%ld g=%ld) chunked(g=%ld) compared=%ld mismatches=%ld\n",
		conv,chunk,ratio,u1,g1,g2,mmin,mismatch);
	return 0;}
