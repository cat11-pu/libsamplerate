#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

/* Boundary scan: zero-length / single-sample / exactly-one-frame, all
** converters x ratios x channel counts. Reports err, input_frames_used,
** output_frames_gen, first output; designed to run under ASan/UBSan. */

static const int convs[] = {SRC_SINC_BEST_QUALITY,SRC_SINC_MEDIUM_QUALITY,
	SRC_SINC_FASTEST,SRC_ZERO_ORDER_HOLD,SRC_LINEAR} ;
static const char* nm[]={"sinc_best","sinc_medium","sinc_fastest","zoh","linear"} ;

int main(void){
	double ratios[]={0.25,0.5,1.0,2.0,4.0} ;
	int chans[]={1,2,8} ;
	long lens[]={0,1,2} ;
	int total=0,bad=0 ;
	printf("%-12s %-6s %-4s %-3s %-5s %-5s %-5s %-12s %s\n",
		"converter","ratio","in","ch","err","used","gen","out0","finite");
	for(unsigned c=0;c<5;c++)
	for(unsigned r=0;r<5;r++)
	for(unsigned h=0;h<3;h++)
	for(unsigned l=0;l<3;l++){
		int ch=chans[h]; long n=lens[l];
		float*in=calloc(n?n*ch:1,sizeof(float));
		float*o=calloc(128*ch,sizeof(float));
		for(long k=0;k<n*ch;k++) in[k]=0.25f*(float)(k+1);
		int err=0; SRC_STATE*st=src_new(convs[c],ch,&err);
		SRC_DATA d; memset(&d,0,sizeof d);
		d.data_in=n?in:NULL; d.data_out=o; d.input_frames=n;
		d.output_frames=128; d.src_ratio=ratios[r]; d.end_of_input=1;
		err=src_process(st,&d);
		int fin=1; for(long k=0;k<d.output_frames_gen*ch;k++) if(!isfinite(o[k])) fin=0;
		int sane=(err==0 && d.input_frames_used<=n && d.output_frames_gen<=128 && fin);
		if(!sane) bad++; total++;
		printf("%-12s %-6.2f %-4ld %-3d %-5d %-5ld %-5ld %-12.7g %s\n",
			nm[c],ratios[r],n,ch,err,d.input_frames_used,d.output_frames_gen,
			d.output_frames_gen?o[0]:0.0, fin?"yes":"NO");
		src_delete(st); free(in); free(o);
	}
	printf("\nscan cases=%d non_sane=%d\n",total,bad);
	return bad?1:0;
}
