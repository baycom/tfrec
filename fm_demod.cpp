/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <gplv2.h>
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "decoder.h"
#include "tfa1.h"
#include "tfa2.h"
#include "fm_demod.h"
#include "dsp_stuff.h"

//-------------------------------------------------------------------------
fsk_demod::fsk_demod(vector<demodulator*> *_demods, int _thresh, int _dbg)
{
	demods=_demods;
	dbg=_dbg;
	thresh=_thresh;
	thresh_mode=0;
	if (thresh==0) {
		thresh=500; // default
		thresh_mode=1; // auto
	}
	last_i=last_q=0;
	triggered_avg=0;
	runs=0;
	index=0;
}
//-------------------------------------------------------------------------
void fsk_demod::process(int16_t *data_iq, int len)
{
	int triggered=0;
	runs++;
	for(auto n=0u;n<demods->size();n++) {
		demods->at(n)->start(len);
	}
	
	for(int i=0;i<len;i+=2) {
		
		// Trigger decoding at power level and check some bit periods
		int pwr=abs(data_iq[i])+abs(data_iq[i+1]);
		int t=0;
		
		for(auto n=0u;n<demods->size();n++)
			t+=demods->at(n)->demod(thresh, pwr, i, data_iq+i);

		if (t)
			triggered++;
		
		last_i=data_iq[i];
		last_q=data_iq[i+1];
	}

	triggered_avg=(31*triggered_avg+triggered)/32;
	
	if (dbg>=3)
		printf("Trigger ratio %i/%i, avg %i \n",triggered,len/2,triggered_avg);
	
	if (thresh_mode==1 && (runs&3)==0) {
		if (triggered_avg >= len/32) {
			thresh+=2;
			if (dbg>=3)
				printf("Increased trigger level to %i\n",thresh);
		} else if (triggered_avg <= len/64 && thresh>50) {
			thresh-=2;
			if (dbg>=3)
				printf("Decreased trigger level to %i\n",thresh);
		}
	}		
}
//-------------------------------------------------------------------------
