#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
int main(void){
	int chs[]={1,2,128};
	for(int i=0;i<3;i++){int ch=chs[i];
		float*in=calloc(ch,sizeof(float)),*o=calloc(64*ch,sizeof(float));
		for(int k=0;k<ch;k++)in[k]=0.5f;
		SRC_DATA d;memset(&d,0,sizeof d);
		d.data_in=in;d.data_out=o;d.input_frames=1;d.output_frames=64;
		d.src_ratio=1.0;d.end_of_input=1;
		int e=src_simple(&d,SRC_LINEAR,ch);
		printf("src_simple LINEAR ch=%d e=%d used=%ld gen=%ld out0=%g\n",
			ch,e,d.input_frames_used,d.output_frames_gen,o[0]);
		free(in);free(o);}
	return 0;}
