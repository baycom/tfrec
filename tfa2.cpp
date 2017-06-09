#include <math.h>

#include "tfa2.h"
#include "dsp_stuff.h"

// Protocol for IT+ Sensors 30.3143/30.3144 and 30.3155
//
// FSK - modulation
// NRZ
// 30.3143,30.3144 (=TFA_2)
//    Bitrate ~17240 bit/s -> @ 1.535MHz & 4x decimation -> 22.2 samples/bit
//    Training sequence: 4* 1-0 toggles (8 bit)
//
// 30.3155 (=TFA_3)
//    Bitrate ~9600 bit/s -> @ 1.535MHz & 4x decimation -> 40 samples/bit
//    Training sequence: 12* 1-0 toggles (24 bit)
//
// Bytes transfered with MSB first
// 7 bytes total (inkl. sync, excl. training)
//
// Telegram format
// 0x2d 0xd4 II IT TT HH CC
// 2d d4: Sync bytes
// III(11:8)=0x9 (at least for 3143/44/55)
// III(7:2)= ID(7:2) (displayed at startup, last 2 bits of ID always 0)
// III(1:0)= ? (3155=2)
// TTT: Temperature BCD in 1/10deg, offset +40deg
// HH: Humidity or sensor-index (binary)
//    HH=6a -> internal temperature sensor (3143)
//    HH=7d -> external temperature sensor (3143)
// CC: CRC8 from I to HH (polynome x^8 + x^5 + x^4  + 1)

//-------------------------------------------------------------------------
tfa2_decoder::tfa2_decoder(sensor_e _type)
{
	type=_type;
	invert=0;
	sr=0;
	sr_cnt=-1;
	byte_cnt=0;
	snum=0;
	bad=0;
	crc=new crc8(0x31); // x^8 +   x^5 + x^4  + 1
}
//-------------------------------------------------------------------------
void tfa2_decoder::flush(int rssi, int offset)
{
	//printf(" CNT %i\n",byte_cnt);
	if (byte_cnt>=7) {
		if (dbg>0) {
			printf("#%03i %u  ",snum++,(uint32_t)time(0));
			for(int n=0;n<7;n++)
				printf("%02x ",rdata[n]);
			printf("                      ");
		}

		int id=(type<<28)|(rdata[2]<<8)|(rdata[3]&0xc0);
		double temp=((rdata[3]&0xf)*100+(rdata[4]>>4)*10+(rdata[4]&0xf));
		temp=temp/10-40;
		int hum=rdata[5];
		int crc_val=rdata[6];
		int crc_calc=crc->calc(&rdata[2],4);

		int sub_id=0;
		if (hum==0x7d)
			sub_id=1;
		id|=sub_id;

		if (crc_val==crc_calc
		    ) {
			if (hum>100)
				hum=0;

			printf("ID %06x %+.1lf %i %02x %02x RSSI %i Offset %.0lfkHz\n",
			       id,temp,hum,crc_val,crc_calc,rssi,
			       -1536.0*offset/131072);

			sensordata_t sd;
			sd.type=type;
			sd.id=id;
			sd.temp=temp;
			sd.humidity=hum;
			sd.sequence=0;
			sd.alarm=0;
			sd.rssi=rssi;
			sd.flags=0;
			sd.ts=time(0);
			store_data(sd);
		}
		else {
			bad++;
			if (dbg>0) {
				if (crc_val!=crc_calc)
					printf("BAD %i RSSI %i  Offset %.0lfkHz (CRC %02x %02x)\n",bad,rssi,
					       -1536.0*offset/131072,
					       crc_val,crc_calc);
				else
					printf("BAD %i RSSI %i  Offset %.0lfkHz (SANITY)\n",bad,rssi,-1536.0*offset/131072);
			}
		}
	}
	sr_cnt=-1;
	sr=0;
	byte_cnt=0;
}
//-------------------------------------------------------------------------
void tfa2_decoder::store_bit(int bit)
{
	//printf("%i %04x\n",bit,sr&0xffff);
	sr=(sr<<1)|(bit);
	if ((sr&0xffff)==0x2dd4) { // Sync start
		//printf("SYNC\n");
		sr_cnt=0;
		rdata[0]=(sr>>8)&0xff;
		byte_cnt=1;
		invert=0;
	}

	// Tolerate inverted sync (maybe useful later...)
	if (((~sr)&0xffff)==0x2dd4) {
		printf("Inverted SYNC\n");
		sr_cnt=0;
		rdata[0]=~((sr>>8)&0xff);
		byte_cnt=1;
		invert=1;
	}

	if (sr_cnt==0) {
		if (byte_cnt<(int)sizeof(rdata)) {
			if (invert)
				rdata[byte_cnt]=~(sr&0xff);
			else
				rdata[byte_cnt]=sr&0xff;
		}
		//printf(" %i %02x\n",byte_cnt,rdata[byte_cnt]);
		byte_cnt++;
	}
	if (sr_cnt>=0)
		sr_cnt=(sr_cnt+1)&7;
}
//-------------------------------------------------------------------------
tfa2_demod::tfa2_demod(decoder *_dec, int _spb) : demodulator( _dec)
{
	spb=_spb;
	timeout_cnt=0;
	reset();
	iir=new iir2(0.5/spb); // Lowpass at bit frequency (01-pattern)
	printf("Samples per bit: %i\n",spb);
}
//-------------------------------------------------------------------------
void tfa2_demod::reset(void)
{
	offset=0;
	bitcnt=0;
	dmin=32767;
	dmax=-32767;
	last_bit=0;
	rssi=0;
}
//-------------------------------------------------------------------------
// More debugging
FILE *fx=NULL;
FILE *fy=NULL;
int fc=0;

// Real FM/NRZ demodulation (compared to tfa1.cpp...)
// Note: index increases by 2 for each IQ-sample!
int tfa2_demod::demod(int thresh, int pwr, int index, int16_t *iq)
{
	int triggered=0;
	int ld=0;

	if (pwr>thresh) {
		if (!timeout_cnt)
			reset();

		timeout_cnt=16*spb;
	}

	if (timeout_cnt) {
		triggered++;

		int dev=fm_dev(iq[0],iq[1],last_i,last_q);
		ld=iir->step(dev);

		// Find deviation limits during sync word
		if (bitcnt<10) {
			if (ld>dmax)
				dmax=(7*dmax+ld)/8;
			if (ld<dmin)
				dmin=(7*dmin+ld)/8;
			offset=(dmax+dmin)/2;
			// Estimate power
			if (bitcnt>4)
				rssi+=(rssi+iq[0]*iq[0]+iq[1]*iq[1])/100;
		}
		timeout_cnt--;

		dev=ld;

		// cheap compensation of 0/1 asymmetry if deviation limited in preceeding filter
		int noffset=offset*0.75;

		int bit=0;
		int margin=32;

		// Hard decision
		if (dev>noffset+(dmax/margin))
			bit=1;

#if 0
		if (!fx)
			fx=fopen("blub","w");
		if (!fy)
			fy=fopen("blub1","w");
		if (fx)
			fprintf(fx,"%i %i\n",fc,ld);
		if (fy)
			fprintf(fy,"%i %i\n",fc,(bit?dmax:dmin));
#endif

		if ((dev>noffset+dmax/margin || dev< noffset+dmin/margin) && bit!=last_bit) {
			if (index>(last_bit_idx+8)) { // Ignore glitches
				bitcnt++;

				// Determine number of bits depending on time between edges
				if (index-last_bit_idx>spb/4) {

					//printf("%i %i %i \n",bit,last_bit,(index-last_bit_idx)/2);
					int numbits=(index-last_bit_idx)/2;
					numbits=(numbits+spb/2)/spb;
					if (numbits<16) { // Sanity
						//printf("   %i %i %i %i nb %i\n",bit,last_bit,dev,(index-last_bit_idx)/2,numbits);
						for(int n=1;n<numbits;n++) {
							dec->store_bit(last_bit);
						}
					}
					dec->store_bit(bit);
					last_bit=bit;
				}
			}
			if (index-last_bit_idx>2)
				last_bit_idx=index;
		}
		fc++;
		// Flush data
		if (!timeout_cnt) {
			// Add some trailing bits
			for(int n=0;n<16;n++)
				dec->store_bit(last_bit);

			//printf("MIN %i MAX %i OFFSET %i RSSI-Raw %i\n",dmin,dmax,offset,rssi);
			dec->flush(10*log10(rssi),offset);
			reset();
		}
	}
	last_i=iq[0];
	last_q=iq[1];

	return triggered;
}
//-------------------------------------------------------------------------
