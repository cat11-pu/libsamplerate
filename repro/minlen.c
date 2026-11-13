#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main(int argc,char**argv){
	int conv=atoi(argv[1]); double ratio=atof(argv[2]);
	int err=0;
	for(long N=1;N<=600;N++){
		float *in=malloc(N*sizeof(float)), *out=malloc((N*8+64)*sizeof(float));
		for(long i=0;i<N;i++) in[i]=0.25f*(i+1);
		SRC_STATE* st=src_new(conv,1,&err);
		SRC_DATA d; memset(&d,0,sizeof d);
		d.data_in=in; d.data_out=out; d.input_frames=N;
		d.output_frames=N*8+64; d.src_ratio=ratio; d.end_of_input=1;
		src_process(st,&d);
		if(d.output_frames_gen>0){ printf("conv=%d ratio=%.2f minN=%ld gen=%ld\n",conv,ratio,N,d.output_frames_gen); free(in);free(out);src_delete(st); return 0;}
		src_delete(st); free(in); free(out);
	}
	printf("conv=%d none\n",conv); return 0;}
