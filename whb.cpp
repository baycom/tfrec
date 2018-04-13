#include <math.h>

#include "whb.h"
#include "dsp_stuff.h"
/*
  TFA WeatherHub
  868.250MHz
  6000 bits/second
  PSK-NRZS-G3RUH-scrambled

  Format:
  4b 2d d4 2b LL II II II II II II TV VV ... CC CC CC CC

  4b 2d d4 2b: Sync word

  LL: Packet length from LL to last CC (fixed 37?)

  II: 6 Byte ID (printed on the sensor)

  T: 5? bit type
  VVV: 11 bit value

  CC: CRC-32, poly=0x04c11db7  init=0x29f0f49b  refin=false  refout=false  xorout=0x00000000
  thanks to CRC reverse-engineering tool "reveng": http://reveng.sourceforge.net/
  
 */

//-------------------------------------------------------------------------
whb_decoder::whb_decoder(sensor_e _type) : decoder(_type)
{
	sr=0;
	sr_cnt=-1;
	byte_cnt=0;
	snum=0;
	bad=0;
	crc=new crc32(0x04c11db7);
	last_bit=0;
	lfsr=0;
	psk=last_psk=0;
	nrzs=0;
}
//-------------------------------------------------------------------------
void whb_decoder::flush(int rssi, int offset)
{
	uint32_t crc_calc=0;
	uint32_t crc_val=0;
	
	if (byte_cnt<11) // Sync+ID
		goto ok;
	
	// FIXME: Variable packet length?
	if (dbg && byte_cnt) {
		printf("#%03i %u %i  ",snum++,(uint32_t)time(0),byte_cnt);
		for(int n=0;n<41;n++)
			printf("%02x ",rdata[n]);
		printf("   %i",rssi);
		puts("");
	}       	
	crc_calc=crc->calc(&rdata[4], 33, 0x29f0f49b);
	crc_val=(rdata[37]<<24) | (rdata[38]<<16) | (rdata[39]<<8) | rdata[40];
	if (crc_calc==crc_val) {
		goto ok;
	}
 bad:
	bad++;
	if (dbg) {
		if (crc_val!=crc_calc)
			printf("WHB BAD %i RSSI %i (CRC %08x %08x, len %i)\n", bad, rssi,
			       crc_val,crc_calc, byte_cnt);
		else
			printf("WHB BAD %i RSSI %i (SANITY)\n",bad,rssi);
	}
 ok:
	sr_cnt=-1;
	sr=0;
	byte_cnt=0;
}
//-------------------------------------------------------------------------
void whb_decoder::store_bit(int bit)
{
	// De-PSK
	if (bit == last_bit)
		psk=1-psk;
	// De-NRZS
	if (psk == last_psk)
		nrzs=1-nrzs;

	last_bit=bit;
	last_psk=psk;

	// G3RUH descrambler
	int bit_descrambled=nrzs ^ ((lfsr>>16)&1) ^ ((lfsr>>11)&1);
	lfsr=(lfsr<<1)|nrzs;
	
	sr=(sr>>1)|(bit_descrambled<<23);
	sr=sr&0xffffff;

	if ( ((sr&0xffffff)==0xd42d4b) ) { // FIXME 3 or 4 bytes?
		//printf("######################### SYNC\n");
		sr_cnt=0;
		rdata[0]=sr&0xff;
		rdata[1]=(sr>>8)&0xff;
		byte_cnt=2;
	}
	
	if (sr_cnt==0) {
		if (byte_cnt<(int)sizeof(rdata)) {
			rdata[byte_cnt]=(sr>>16)&0xff;
		}
		//printf(" %i %02x\n",byte_cnt,rdata[byte_cnt]);
		byte_cnt++;
	}
	if (sr_cnt>=0)
		sr_cnt=(sr_cnt+1)&7;
}
//-------------------------------------------------------------------------
whb_demod::whb_demod(decoder *_dec, int _spb) : demodulator( _dec)
{
	spb=_spb;	
	timeout_cnt=0;
	reset();
	iir=new iir2(3.0/spb); // Pulse filter
	iir_avg=new iir2(0.002/spb); // Phase discriminator filter
	printf("WHB: Samples per bit: %i\n",spb);
}
//-------------------------------------------------------------------------
void whb_demod::reset(void)
{
	offset=0;
	bitcnt=0;
	last_peak=0;
	rssi=0;
	step=last_peak=0;
}
//-------------------------------------------------------------------------
// More debugging
static FILE *fx=NULL;
static FILE *fy=NULL;
static int fc=0;
int last_dev=0;
static int ld=0;

int whb_demod::demod(int thresh, int pwr, int index, int16_t *iq)
{
	int triggered=0;

	if (pwr>thresh) {
		if (!timeout_cnt) {
			reset();
		}
		
		timeout_cnt=8*spb;		
	}

	if (timeout_cnt) {
		triggered++;
		int dev;
		dev=fm_dev(iq[0],iq[1],last_i,last_q);
		int x=dev;
		dev=fabs(dev-last_dev); // Calc FM deviation difference -> phase
		last_dev=x;
		ld=iir->step(dev); // smooth it a bit
		avg_of=iir_avg->step(1.5*ld); // minimum phase shift

		int bit=0;
		timeout_cnt--;
		
		dev=ld;

		int tdiff=step-last_peak;
		int tdiff_mod=tdiff%spb;

		if (dev>avg_of && // phase shift occured
		    (tdiff>spb/2) && // far away from last peak
		    ((tdiff_mod<spb/4) || (tdiff_mod>(3*spb/4))) ) { // falling into bit period
			bit=avg_of;
			int bit0=(tdiff+spb/2)/spb;
			for(int n=1;n<bit0;n++)
				dec->store_bit(1);
			dec->store_bit(0);
			last_peak=step;
			//printf("%i %i %i\n",fc,tdiff,bit0);
		}
		
#if 0
		if (!fx)
			fx=fopen("blub","w");
		if (!fy)
			fy=fopen("blub1","w");
		if (fx)
			fprintf(fx,"%i %i %i %i\n",fc,dev, bit, tdiff_mod*10);
		if (fy)
			fprintf(fy,"%i %i\n",fc,(bit?dmax:dmin));
		fc++;
#endif
		
		if (!timeout_cnt) {
			// Flush descrambler
			for(int n=0;n<16;n++)
				dec->store_bit(0);
			dec->flush(0,offset); // no rssi for now
			reset();
		}
	}
	last_i=iq[0];
	last_q=iq[1];

	last_ld=ld;
	step++;
	return triggered;
}
//-------------------------------------------------------------------------
