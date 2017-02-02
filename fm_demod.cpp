/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <gplv2.h>
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fm_demod.h"
#include "dsp_stuff.h"

// Protocol for IT+ Sensors
//
// FSK - modulation
// NRZS (*not* NRZ!)
// 0: toggle frequency  (deviation > some limit)
// 1: do not toggle frequency
// Bitrate ~38400bit/s -> @ 1.535MHz -> 40 samples/bit, @ 4x decimation -> 10 samples/bit
// Training sequence >150 '0'-edges
// Bytes transfered with LSB first
// Ten bytes total (inkl. sync)
//
// Telegram format
// 0x2d 0xd4 ID ID sT TT HH BB SS 0x56 CC
// 2d d4: Sync bytes
// ID(14:0): 15 bit ID of sensor (printed on the back and displayed after powerup)
// ID(15) is either 1 or 0 (fixed, depends on the sensor)
// s(3:0): Learning sequence 0...f, after learning fixed 8
// TTT: Temperatur in BCD in .1degC steps, offset +40degC (-> -40...+60)
// HH(6:0): rel. Humidity in % (binary coded, no BCD!)
// BB(7): Low battery if =1
// BB(6:4): 110 (fixed)
// SS(7:4): sequence number (0...f)
// SS(3:0): 0000 (fixed)
// 56: Type?
// CC: CRC8 from ID to 0x56 (polynome x^8 + x^5 + x^4  + 1)


// Samplerate 1536kHz
#define BITPERIOD ((1536000/38400)/4)

//-------------------------------------------------------------------------
crc8::crc8(int poly)
{
        int n,m;
        for(n=0;n<256;n++){
                int t=n;
                for(m=0;m<8;m++) {
                        if ((t&0x80)!=0)
                                t=(t<<1)^poly;
                        else
                                t=t<<1;
                }
                lookup[n]=t;
        }
}
//-------------------------------------------------------------------------
uint8_t crc8::calc(uint8_t *data, int len)
{
        int n;
        uint8_t crc=0;

        for(n=0;n<len;n++) {
                crc=lookup[crc ^ *data];
                data++;
        }
        return crc; 
}
//-------------------------------------------------------------------------
tfa_decoder::tfa_decoder(char *_handler, int _dbg)
{	
	handler=_handler;
	dbg=_dbg;
	sr=0;
	sr_cnt=-1;
	byte_cnt=0;
	snum=0;
	bad=0;
	crc=new crc8(0x31); // x^8 +   x^5 + x^4  + 1
}
//-------------------------------------------------------------------------
void tfa_decoder::flush(int rssi)
{
	if (byte_cnt>=10) {
		if (dbg) {
			printf("#%03i %u  ",snum++,(uint32_t)time(0));
			for(int n=0;n<11;n++)
				printf("%02x ",rdata[n]);
			printf("          ");
		}
		int id=((rdata[2]<<8)|rdata[3])&0x7fff;
		int batfail=(rdata[7]&0x80)>>7; 
		double temp=((rdata[4]&0xf)*100)+((rdata[5]>>4)*10)+(rdata[5]&0xf);
		temp=(temp/10)-40;
		int hum=rdata[6];
		int seq=rdata[8]>>4;
		uint8_t crc_val=rdata[10];
		uint8_t crc_calc=crc->calc(&rdata[2],8);
	       
		// CRC and sanity checks
		if ( crc_val==crc_calc
		     && ((rdata[4]&0xf0)==0x80 // ignore all learning messages except sensor fails
		     	 || hum==0x7f || hum==0x6a)
		     && hum<=0x7f              // catch sensor fail due to lowbat
		     && (rdata[7]&0x70)==0x60
		     && (rdata[8]&0xf)==0
		     && rdata[9]==0x56 // Type?
		     ) {

			// Sensor values may fail at 0xaaa/0xfff or 0x6a/0x7f due to low battery
			// even without lowbat bit
			// Set it to 2 -> sensor data not valid
			if (hum==0x7f) {
				batfail=2;
				hum=0;
				temp=0;
			}

			// If the sensor does not provide humidity values like the 30.3181.IT it sends 0x6a (106%) instead
			if (hum==0x6a) {
				hum=0;
			}
			
			if (dbg>=0){
				printf("ID %04x %+.1f %i%%  seq %x lowbat %i RSSI %i\n",id,temp,hum,seq, batfail, rssi);
				fflush(stdout);
			}

			if (handler && strlen(handler)) {
				char cmd[512];
				snprintf(cmd,sizeof(cmd),"%s %04x %+.1f %i %i %i %i",
					 handler,
					 id, temp, hum,
					 seq, batfail, rssi);
				if (dbg==1)
					printf("EXEC %s\n",cmd);
				(void)system(cmd);
			}
		}
		else {
			bad++;
			if (dbg) {
				if (crc_val!=crc_calc)
					printf("BAD %i RSSI %i (CRC %02x %02x)\n",bad,rssi,crc_val,crc_calc);
				else
					printf("BAD %i RSSI %i (SANITY)\n",bad,rssi);
			}
		}
	}
	sr_cnt=-1;
	byte_cnt=0;
	rdata[10]=0x00; // clear crc
}
//-------------------------------------------------------------------------
void tfa_decoder::store_bit(int bit)
{	
	sr=(sr>>1)|(bit<<31);
	if ((sr&0xffff)==0xd42d) { // Sync start
		sr_cnt=0;
		byte_cnt=0;
	}
	if (sr_cnt==0) {
		if (byte_cnt<(int)sizeof(rdata))
			rdata[byte_cnt]=sr&0xff;
		byte_cnt++;
	}
	if (sr_cnt>=0)
		sr_cnt=(sr_cnt+1)&7;
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
fsk_demod::fsk_demod(int _thresh, tfa_decoder *_dec, int _dbg)
{
	dbg=_dbg;
	thresh=_thresh;
	if (thresh==0)
		thresh=150; // default
	dec=_dec;
	last_i=last_q=0;
	timeout_cnt=0;
	last_bit_idx=0;
	mark_lvl=0;
	rssi=0;
}
//-------------------------------------------------------------------------
// NRZS - detect frequency changes
void fsk_demod::process(int16_t *data_iq, int len)
{
	int max_val=0;
	int last_dev=0;
	int triggered=0;
	
	if (last_bit_idx) // handle data wrap from last block processing
		last_bit_idx-=len;	

	for(int i=0;i<len;i+=2) {
		
		// Trigger decoding at power level and check some bit periods
		int pwr=abs(data_iq[i])+abs(data_iq[i+1]);		       
		if (pwr>thresh)
			timeout_cnt=40*BITPERIOD;
		
		if (timeout_cnt) {
			triggered++;
			int dev=fm_dev(data_iq[i],data_iq[i+1],last_i,last_q);
		
			// Hold maximum deviation of 0-edges for reference
			if (dev>mark_lvl)
				mark_lvl=dev;
			else
				mark_lvl=mark_lvl*0.95 ;
			
			// remember peak for RSSI look-alike
			if (mark_lvl>rssi)
				rssi=mark_lvl;
			
			timeout_cnt--;
			// '0'-pulse if deviation drops below referenced threshold
			if (dev<mark_lvl/2) {
				if (last_bit_idx) {
					// Determine number of 1-bits depending on time between 0-pulses
					if (i-last_bit_idx>4) {
						for(int n=(2*BITPERIOD)+2;n<=(i-last_bit_idx);n+=2*BITPERIOD)
							dec->store_bit(1);
						dec->store_bit(0);
					}
				}
				if (i-last_bit_idx>2)
					last_bit_idx=i;
			}
			// Flush data
			if (!timeout_cnt) {
				dec->flush(10*log10(rssi));
				mark_lvl=0;
				rssi=0;
				last_bit_idx=0;
			}
		}
		last_i=data_iq[i];
		last_q=data_iq[i+1];
	}
	if (dbg==2)
		printf("Trigger ratio %i/%i \n",triggered,len/2);
}
//-------------------------------------------------------------------------
