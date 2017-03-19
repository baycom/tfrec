/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
  
  dsp_stuff.cpp - Some useful DSP functions

  Some parts inspired from librtlsdr (rtl_fm.c)
  * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
  * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
  * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
  * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
  * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp_stuff.h"

//-------------------------------------------------------------------------
// 2nd order butterworth lowpass
iir2::iir2(double cutoff)
{
	double i=1.0/tan(M_PI*cutoff);
	double s=sqrt(2);
	b0=1/(1+s*i+i*i);
	b1=2*b0;
	b2=b0;
	a1=2*(i*i-1)*b0;
	a2=-(1-s*i+i*i)*b0;
	yn=yn1=yn2=0;
	dn1=dn2=0;
	//printf("%f %f %f %f %f\n",a1,a2,b0,b1,b2);
}
//-------------------------------------------------------------------------
double iir2::step(double dn)
{
	yn2=yn1;
	yn1=yn;
	yn=b0*dn + b1*dn1 + b2*dn2 + a1*yn1 + a2*yn2;
	dn2=dn1;
	dn1=dn;
	return yn;
}
//-------------------------------------------------------------------------
// In-place decimator
#define DEC_TAP_NUM1 20
#define DEC_TAP_NUM2 8

static int16_t dec_filter_taps1[DEC_TAP_NUM1] = 
{
/* http://t-filter.engineerjs.com/
	20 taps, sampling frequency 768Hz, 16bit
	0..44Hz @ 1, Ripple 2dB
	96...384Hz@0, att -35dB
*/
-1087,
  -1082,
  -1065,
  -451,
  912,
  2997,
  5556,
  8157,
  10285,
  11484,
  11484,
  10285,
  8157,
  5556,
  2997,
  912,
  -451,
  -1065,
  -1082,
  -1087
};

static int16_t dec_filter_taps2[DEC_TAP_NUM2] = 
{
	// 0..48@4, 160-384@-36, 8taps
  2443,
  6339,
  11036,
  14254,
  14254,
  11036,
  6339,
  2443
};

decimate::decimate(void) 
{
	for(int i=0;i<2*DEC_TAP_NUM1;i++) {
		hist[i]=0;
		hist0[i]=0;
		hist1[i]=0;
	}
}
//-------------------------------------------------------------------------
decimate::~decimate(void)
{
}

//-------------------------------------------------------------------------
/*
        0 2 4 6 8 0 2 4-6 8 0 2 4 6 8 0-2 4 6
0   x x X X
1       x x X X  
2           x x X X
3               x x X X-       
4                   x x-X X
5                      -x x X X
6                           x x X X
7                               x x X X-
8                                   x x-X X
 */
// Data: IQ with int16_t (-> step 2 for each sample)
int decimate::process2x(int16_t *data, int length)
{
	int shift=16;
	int16_t t0[DEC_TAP_NUM1];
	int16_t *taps=dec_filter_taps1;
	for(int i=0;i<DEC_TAP_NUM1;i++) {
		t0[i]=hist0[i];
	}
	int32_t sum;
	for (int i=0; i<length; i+=4) {

		for(int n=0;n<DEC_TAP_NUM1-2;n++)
			t0[n]=t0[n+2];

		t0[DEC_TAP_NUM1-2]=data[i];
		t0[DEC_TAP_NUM1-1]=data[i+2];

		sum=0;

		for(int n=0;n<DEC_TAP_NUM1;n++)
			sum+=(t0[n]*taps[n])>>shift;

		data[i/2] = sum;
	}
	for(int i=0;i<DEC_TAP_NUM1;i++)
		hist0[i]=t0[i];
	return 0;
}
//-------------------------------------------------------------------------
int decimate::process2x1(int16_t *data, int length)
{
	int shift=16;
	int16_t t0[DEC_TAP_NUM2];
	int16_t *taps=dec_filter_taps2;
	for(int i=0;i<DEC_TAP_NUM2;i++) 
		t0[i]=hist0[i];

	int32_t sum;
	for (int i=0; i<length; i+=4) {

		for(int n=0;n<DEC_TAP_NUM2-2;n++)
			t0[n]=t0[n+2];

		t0[DEC_TAP_NUM2-2]=data[i];
		t0[DEC_TAP_NUM2-1]=data[i+2];
		sum=0;
	
		for(int n=0;n<DEC_TAP_NUM2;n++)
			sum+=(t0[n]*taps[n])>>shift;

		data[i/2] = sum;
	}
	for(int i=0;i<DEC_TAP_NUM2;i++)
		hist0[i]=t0[i];
	return 0;
}
//-------------------------------------------------------------------------
downconvert::downconvert(int p) 
{
	passes=p;
	dec_i.resize(passes);
	dec_q.resize(passes);
}
//-------------------------------------------------------------------------
downconvert::~downconvert(void)
{
}
//-------------------------------------------------------------------------
int downconvert::process_iq(int16_t *data_iq, int len, int filter_type)
{		
	int ft=filter_type;
#if 1
	for (int i=0; i < passes-1; i++) {
		dec_i[i].process2x1(data_iq,   len);
		dec_q[i].process2x1(data_iq+1, len);
		len=len>>1;
	}
#else
	for (int i=0; i < passes-1; i++) {
		dec_i[i].process2x(data_iq,   len);
		dec_q[i].process2x(data_iq+1, len);
		len=len>>1;
	}
#endif
	dec_i[passes-1].process2x(data_iq, len);
	dec_q[passes-1].process2x(data_iq+1, len);
	len=len>>1;

	return len;
}
//-------------------------------------------------------------------------
// Vastly reduced mixed demodulator for FM-NRZS:
// It doesn't need to be linear, nor do we care about frequency shift direction
//-------------------------------------------------------------------------
int fm_dev_nrzs(int ar, int aj, int br, int bj)
{
	int cr=ar*br+aj*bj;

	// This limits also the max RSSI
	if (cr>1e9)
		cr=1e9;
	if (cr<-1e9)
		cr=-1e9;
	return cr;
}
//-------------------------------------------------------------------------
// Real FM demodulation
//-------------------------------------------------------------------------
#if 1
int fm_dev(int ar, int aj, int br, int bj)
{
        double cr, cj;
        double angle;
        cr =((double) ar)*br + ((double)aj)*bj;
        cj = ((double)aj)*br - ((double)ar)*bj;
        angle = atan2(cj,cr);   
        return (int)(angle / M_PI * (1<<14));
}
#endif
//-------------------------------------------------------------------------
