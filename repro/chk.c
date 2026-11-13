#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>
int main(void){
int convs[]={0,1,2,3,4}; double rs[]={0.5,1,2,4}; int chs[]={1,2,8};
for(int ci=0;ci<5;ci++)for(int ri=0;ri<4;ri++)for(int hi=0;hi<3;hi++){
int ch=chs[hi]; float*in=malloc(ch*4),*o=malloc(ch*64*4); for(int k=0;k<ch;k++)in[k]=0.5f;
int e=0;SRC_STATE*s=src_new(convs[ci],ch,&e);SRC_DATA d;memset(&d,0,sizeof d);
d.data_in=in;d.input_frames=1;d.data_out=o;d.output_frames=64;d.src_ratio=rs[ri];d.end_of_input=1;
src_process(s,&d);
if(d.output_frames_gen>0) printf("conv=%d r=%.1f ch=%d g=%ld out0=%.6g\n",convs[ci],rs[ri],ch,d.output_frames_gen,o[0]);
src_delete(s);free(in);free(o);}
return 0;}
