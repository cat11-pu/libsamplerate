#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <samplerate.h>
int main(int argc,char**argv){
	int conv=atoi(argv[1]); long chunk=atol(argv[2]); double ratio=atof(argv[3]); long N=64;
	float in[64],out[512]; for(int i=0;i<64;i++) in[i]=0.25f*(i+1);
	int err=0; SRC_STATE* st=src_new(conv,1,&err);
	SRC_DATA d; long pos=0,tg=0,tu=0;
	while(pos<N){
		long take=N-pos<chunk?N-pos:chunk;
		memset(&d,0,sizeof d);
		d.data_in=in+pos; d.data_out=out+tg; d.input_frames=take;
		d.output_frames=512-tg; d.src_ratio=ratio; d.end_of_input=0;
		int e=src_process(st,&d); if(e){printf("ERR %d\n",e);return 1;}
		pos+=d.input_frames_used; tu+=d.input_frames_used; tg+=d.output_frames_gen;
	}
	for(int g=0;g<100;g++){
		memset(&d,0,sizeof d);
		d.data_in=NULL; d.data_out=out+tg; d.input_frames=0;
		d.output_frames=512-tg; d.src_ratio=ratio; d.end_of_input=1;
		int e=src_process(st,&d); if(e){printf("DRAIN ERR %d\n",e);return 1;}
		tg+=d.output_frames_gen;
		if(d.output_frames_gen==0) break;
	}
	printf("chunk=%ld used=%ld gen=%ld out0=%.7f\n",chunk,tu,tg,tg?out[0]:0.f);
	return 0;}
