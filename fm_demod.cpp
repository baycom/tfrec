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
	if (thresh==0)
		thresh=150; // default
	last_i=last_q=0;	
}
//-------------------------------------------------------------------------
void fsk_demod::process(int16_t *data_iq, int len)
{
	int max_val=0;
	int last_dev=0;
	int triggered=0;

	for(int n=0;n<demods->size();n++) {
		demods->at(n)->start(len);
	}
	
	for(int i=0;i<len;i+=2) {
		
		// Trigger decoding at power level and check some bit periods
		int pwr=abs(data_iq[i])+abs(data_iq[i+1]);
		int t=0;
		
		for(int n=0;n<demods->size();n++)
			t+=demods->at(n)->demod(thresh, pwr, i, data_iq+i);

		if (t)
			triggered++;
		
		last_i=data_iq[i];
		last_q=data_iq[i+1];
	}
	
	if (dbg==2)
		printf("Trigger ratio %i/%i \n",triggered,len/2);
	if (triggered >= len/8) {
		thresh+=5;
		if (dbg==2)
			printf("Adjust trigger level to %i\n",thresh);
	}
		
}
//-------------------------------------------------------------------------
