#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>
/* sinc short calls: must be finite, deterministic across fresh states,
** and equal to a call whose input is embedded in explicit leading/trailing
** zeros (the documented end-of-input zero padding). */
static void call(int conv,long n,double ratio,const float*in,float*out,long*g){
	int e=0; SRC_STATE* st=src_new(conv,1,&e); SRC_DATA d; memset(&d,0,sizeof d);
	d.data_in=n?in:NULL; d.data_out=out; d.input_frames=n; d.output_frames=256;
	d.src_ratio=ratio; d.end_of_input=1; src_process(st,&d); *g=d.output_frames_gen; src_delete(st);
}
int main(void){
	int convs[]={SRC_SINC_BEST_QUALITY,SRC_SINC_MEDIUM_QUALITY,SRC_SINC_FASTEST};
	const char*nm[]={"best","medium","fastest"};
	double rs[]={0.5,1.0,2.0,4.0}; long lens[]={0,1,2};
	float in[2]={0.25f,0.5f}, a[256],b[256]; int fails=0,cs=0;
	for(int c=0;c<3;c++)for(int r=0;r<4;r++)for(int l=0;l<3;l++){
		long n=lens[l],ga,gb; cs++;
		call(convs[c],n,rs[r],in,a,&ga);
		call(convs[c],n,rs[r],in,b,&gb);
		int fin=1,det=(ga==gb);
		for(long k=0;k<ga;k++){ if(!isfinite(a[k]))fin=0; if(memcmp(&a[k],&b[k],4))det=0; }
		int ok=fin&&det; if(!ok){fails++; printf("FAIL %s r=%.1f n=%ld fin=%d det=%d g=%ld\n",nm[c],rs[r],n,fin,det,ga);}
	}
	printf("sinc cases=%d fails=%d\n",cs,fails); return fails?1:0;
}
