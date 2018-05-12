#include <math.h>

#include "tfa2.h"
#include "dsp_stuff.h"

/*
 Protocol for IT+ Sensors 30.3143/30.3144 and 30.3155 and TX22

FSK - modulation / NRZ 
30.3143,30.3144 (=TFA_2)
    Bitrate ~17240 bit/s -> @ 1.535MHz & 4x decimation -> 22.2 samples/bit
    Training sequence: 4* 1-0 toggles (8 bit)

30.3155 (=TFA_3)
    Bitrate ~9600 bit/s -> @ 1.535MHz & 4x decimation -> 40 samples/bit
    Training sequence: 12* 1-0 toggles (24 bit)

Bytes transfered with MSB first
7 bytes total (inkl. sync, excl. training)

Telegram format
            3  4 5  6 
 0x2d 0xd4 II IT TT HH CC 
 2d d4: Sync bytes
 III(11:8)=0x9 (at least for 3143/44/55)
 III(7:2)= ID(7:2) (displayed at startup, last 2 bits of ID always 0)
 III(1:0)= ? (3155=2)
 III(1)=  New battery, set to zero after some hours
 TTT: Temperature BCD in 1/10deg, offset +40deg
 HH: Humidity or sensor-index (binary)
    HH=6a -> internal temperature sensor (3143)
    HH=7d -> external temperature sensor (3143)
 HH(7) Lowbatt?
 CC: CRC8 from I to HH (polynome x^8 + x^5 + x^4  + 1)


TX22 (documentation from FHEM/Jeelink TX22IT.cpp)
8842 bit/s
Telegram format
  0x2d 0xd4 SI IQ TV VV [TV VV] CC

  S = 0xa
  I(7:2): ID
  I(1): Learning bit
  I(0): Error
  Q(3): Low Bat
  Q(2:0): Count of data words
  T: Type, 0: temp (BCD/10+40 deg C), 1: humidity (BCD %rH), 
           2: rain (12bit cnt), 3: wind (4bit * 22.5deg, 8bit speed 0.1*m/s), 4: gust (12bit 0.1*m/s)

*/


//-------------------------------------------------------------------------
tfa2_decoder::tfa2_decoder(sensor_e _type) : decoder(_type)
{
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
	if (type==TX22)
		flush_tx22(rssi,offset);
	else
		flush_tfa(rssi,offset);
}
//-------------------------------------------------------------------------
void tfa2_decoder::flush_tx22(int rssi, int offset)
{
	uint8_t crc_val,crc_calc;
	
	if (byte_cnt>=7 && byte_cnt<64) {
		if (dbg) {
			printf("#%03i %u  ",snum++,(uint32_t)time(0));
			for(int n=0;n<byte_cnt;n++)
				printf("%02x ",rdata[n]);
			printf("                      ");
		}
		if ((rdata[2]>>4)!=0xa)
			goto bad;
		int id=((rdata[2]&0xf)<<2)|(rdata[3]>>6);
		int error=!((rdata[3]>>4)&1);
		int lowbat=(rdata[3]>>3)&1;
		int num=rdata[3]&7;

		crc_val=rdata[2*num+4];
		crc_calc=crc->calc(&rdata[2],2+2*num);
		if (crc_val!=crc_calc)
			goto bad;
		if (num>8)
			goto bad;

		int have_temp=0,have_hum=0, have_rain=0, have_wind=0, have_gust=0;
		double temp=0,hum=0,rain=0,wdir=0,wspeed=0,wgust=0;
				
		for(int n=0;n<num;n++) {
			int t=rdata[4+n*2]>>4;


			switch (t) {
			case 0:  { // Temp
				double v=(rdata[4+n*2]&0xf)*100+
					(rdata[4+n*2+1]>>4)*10+
					(rdata[4+n*2+1]&0xf);
				temp=(v/10)-40;
				have_temp=1;
			}
				break;
			case 1: { // Hum
				int v=(rdata[4+n*2]&0xf)*100+
					(rdata[4+n*2+1]>>4)*10+
					(rdata[4+n*2+1]&0xf);
				hum=v;
				have_hum=1;
			}
				break;
			case 2: { // rain counter
				int v=((rdata[4+n*2]&0xf)<<8)+
					rdata[4+n*2+1];
				rain=v;
				have_rain=1;
			}
				break;
			case 3: { // wind dir/speed
				double d=(rdata[4+n*2]&0xf)*22.5;
				double s=rdata[4+n*2+1]/10.0;
				wdir=d;
				wspeed=s;
				have_wind=1;
			}
				break;
			case 4: { // gust
				double s=(((rdata[4+n*2]&0xf)<<8)+
					  rdata[4+n*2+1])/10.0;
				wgust=s;
				have_gust=1;
			}
				break;
			default:
				break;
			}	
		}
		sensordata_t sd;
		sd.type=type;
		sd.temp=0;
		sd.humidity=0;
		sd.sequence=0;
		sd.alarm=error|lowbat;
		sd.rssi=rssi;
		sd.flags=0;
		sd.ts=time(0);
		
		int new_id=(type<<28)|(id<<4);

		if (have_temp) {
			sd.id=new_id;
			sd.temp=temp;
			sd.humidity=hum;
			store_data(sd);
		}
		if (have_rain) {
			sd.id=new_id|2;
			sd.temp=rain;
			sd.humidity=0;
			store_data(sd);
		}
		if (have_wind) {
			sd.id=new_id|3;
			sd.temp=wspeed;
			sd.humidity=wdir;
			store_data(sd);
		}
		if (have_gust) {
			sd.id=new_id|4;
			sd.temp=wgust;
			sd.humidity=0;
			store_data(sd);
		}

		if (dbg>1)
			printf("TX22 ID %x, temp %g (%i), hum %g (%i), rain %g (%i), w_speed %g (%i), w_dir %g, w_gust %g (%i)\n",
			       new_id, temp, have_temp, hum, have_hum, rain, have_rain, wspeed, have_wind, wdir, wgust, have_gust);
		sr_cnt=-1;
		sr=0;
		byte_cnt=0;
		return;
	}
 bad:

	if (dbg && byte_cnt>=7 && byte_cnt<64) {
		bad++;
		if (crc_val!=crc_calc)
			printf("TX22(%02x) BAD %i RSSI %i  Offset %.0lfkHz (CRC %02x %02x) len %i\n",1<<type,bad,rssi,
			       -1536.0*offset/131072,
			       crc_val,crc_calc,byte_cnt);
		else
			printf("TX22(%02x) BAD %i RSSI %i  Offset %.0lfkHz len %i (SANITY)\n",1<<type,bad,rssi,-1536.0*offset/131072, byte_cnt);
		fflush(stdout);
	}
	sr_cnt=-1;
	sr=0;
	byte_cnt=0;
}
//-------------------------------------------------------------------------
void tfa2_decoder::flush_tfa(int rssi, int offset)
{
	//printf(" CNT %i\n",byte_cnt);
	if (byte_cnt>=7) {
		if (dbg) {
			printf("#%03i %u  ",snum++,(uint32_t)time(0));
			for(int n=0;n<7;n++)
				printf("%02x ",rdata[n]);
			printf("                      ");
		}

		int id=(type<<28)|(rdata[2]<<8)|(rdata[3]&0xc0);
		double temp=((rdata[3]&0xf)*100+(rdata[4]>>4)*10+(rdata[4]&0xf));
		temp=temp/10-40;
		int hum=rdata[5];
		uint8_t crc_val=rdata[6];
		uint8_t crc_calc=crc->calc(&rdata[2],4);

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
			if (dbg) {
				if (crc_val!=crc_calc)
					printf("TFA2/3(%02x) BAD %i RSSI %i  Offset %.0lfkHz (CRC %02x %02x)\n",1<<type,bad,rssi,
					       -1536.0*offset/131072,
					       crc_val,crc_calc);
				else
					printf("TFA2/3(%02x) BAD %i RSSI %i  Offset %.0lfkHz (SANITY)\n",1<<type,bad,rssi,-1536.0*offset/131072);
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
	//	printf("%i %04x\n",bit,sr&0xffff);
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
tfa2_demod::tfa2_demod(decoder *_dec, int _spb, double _iir_fac) : demodulator( _dec)
{
	spb=_spb;	
	timeout_cnt=0;
	reset();
	iir=new iir2(_iir_fac/spb); // iir_fac=0.5 -> Lowpass at bit frequency (01-pattern)
	printf("type 0x%x: Samples per bit: %i\n", _dec->get_type(), spb);
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
	est_spb=spb;
}
//-------------------------------------------------------------------------
#define DBG_DUMP
#ifdef DBG_DUMP
// More debugging
FILE *fx=NULL;
FILE *fy=NULL;
int fc=0;
#endif

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
		int noffset=0.9*offset; 
		
		int bit=0;
		int margin=32;

		// Hard decision
		if (dev>noffset+(dmax/margin))
			bit=1;

		int bdbg=0;
		if ((dev>noffset+dmax/margin || dev< noffset+dmin/margin) && bit!=last_bit) {
			if (index>(last_bit_idx+8)) { // Ignore glitches
				bitcnt++;
				int tdiff=index-last_bit_idx;
				// Determine number of bits depending on time between edges
				if (tdiff>spb/4 && tdiff<16*spb) {
					#if 0
					if (bitcnt>2 && bitcnt<16) {
						est_spb=(3*est_spb + (tdiff)/2)/4;
						//printf("%i EST %.1f %i\n",fc, est_spb,tdiff);
					}
					#endif
					//printf("%i %i %i \n",bit,last_bit,(index-last_bit_idx)/2);
					int numbits=(index-last_bit_idx)/2;
					numbits=(numbits+est_spb/2)/est_spb;
					if (numbits<16) { // Sanity
						//printf("   %i %i %i %i nb %i\n",bit,last_bit,dev,(index-last_bit_idx)/2,numbits);
						for(int n=1;n<numbits;n++) {
							dec->store_bit(last_bit);
						}
					}
					dec->store_bit(bit);
					last_bit=bit;
					bdbg=bit;
				}
			}
			if (index-last_bit_idx>2)
				last_bit_idx=index;
		}
		
#ifdef DBG_DUMP
		if (!fx)
			fx=fopen("blub","w");
		if (!fy)
			fy=fopen("blub1","w");
		if (fx)
			fprintf(fx,"%i %i %i\n",fc,ld,bdbg*(noffset+dmin/margin));
		if (fy)
			fprintf(fy,"%i %i\n",fc,(bit?dmax:dmin));

		fc++;
#endif

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
