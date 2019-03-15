#include <math.h>
#include <map>

#include "whb.h"
#include "dsp_stuff.h"

/*
  TFA WeatherHub
  868.250MHz
  6000 bits/second
  PSK-NRZS-G3RUH-scrambled

  RF layer:

  4b 2d d4 2b LL II II II II II II <Payload> CC CC CC CC

  4b 2d d4 2b: Sync word

  LL: Packet length from LL to last CC (depends on type)

  II: 6 Byte ID (printed on the sensor, first byte type)

  Thanks to CRC reverse-engineering tool "reveng": http://reveng.sourceforge.net/
  CC: CRC-32, poly=0x04c11db7  init=<see below>  refin=false  refout=false  xorout=0x00000000
  Init-value depends on type (see crc_initvals)!

  See https://github.com/sarnau/MMMMobileAlerts/blob/master/MobileAlertsGatewayBinaryUpload.markdown
  for more details on the sensor payload!    

  ID-Mapping: Sensor-ID gets an appended nibble like TX22 (-> 6.5byte ID!)
  ID.0: temperature, humidity (0 if not available) (indoor)
  ID.2: Rain sensor: tempval=rain-counter, hum=time since last pulse
  ID.3: Wind sensor: tempval=speed (m/s), hum=direction (degree)
  ID.4: Wind sensor: tempval=gust speed (m/s)
  ID.5: Door/water-sensor: tempval=state, hum=time since last event
  ID.c: temperature, humidity (outdoor, sensor #1)
  ID.d: temperature, humidity (sensor #2)
  ID.e: temperature, humidity (sensor #3)

  Notes:
  WHB-Type 7 (MA10410/TFA 35.1147.01) has indoor and outdoor values -> mapped to ID.0 and ID.c
  WHB-Type 11 (TFA 30.3060.01) has indoor and 3 other sensor values -> mapped to ID.0 and ID.c to ID.e
 */
#include <stdlib.h>
using std::map;

map<uint32_t, uint32_t> crc_initvals = {
        { 0x02, 0x97d97a26}, // Only temp
	{ 0x03, 0xf59c5a1e}, // Temp/hum
	{ 0x04, 0x98e1d11f}, // Temp/hum/water
	{ 0x06, 0xa7a41254}, // Temp/hum + temp2
	{ 0x07, 0x3303fb1d}, // Station MA10410 (TFA 35.1147.01)
	{ 0x08, 0x29f0f49b}, // Rain sensor (+ temp)
	{ 0x0b, 0xe7720ae4}, // Wind sensor
	{ 0x10, 0x62d0afc1}, // Door sensor
	{ 0x11, 0x8cba0708}, // 4 Thermo-hygro-sensors (TFA 30.3060.01)
};

// Translates time units in seconds multiplier
uint32_t timeunit_tab[4]= {
	24*60*60, // day
	60*60,    // hour
	60,       // minutes
	1         // seconds
};

#define BE16(x) ( ((*(x))<<8)  |  (*(x+1)) )
#define BE24(x) ( ((*(x))<<16) | ((*(x+1))<<8) |  (*(x+2)) )
#define BE32(x) ( ((*(x))<<24) | ((*(x+1))<<16) | ((*(x+2))<<8) | (*(x+3)) )

#define BE48(x) ( (((uint64_t)*(x))<<40) | (((uint64_t)*(x+1))<<32) | (((uint64_t)*(x+2))<<24) | \
	                  (((uint64_t)*(x+3))<<16) |  (((uint64_t)*(x+4))<<8) | (((uint64_t)*(x+5)))  )

//-------------------------------------------------------------------------
whb_decoder::whb_decoder(sensor_e _type) : decoder(_type)
{
	sr=0;
	sr_cnt=-1;
	byte_cnt=0;
	snum=0;
	bad=0;
	crc=new crc32(0x04c11db7);
#if 0	
	{
		uint8_t msg[4];
		for(uint32_t i=0;i<0xffffffff;i++) {
			msg[0]=i>>24;
			msg[1]=i>>16;
			msg[2]=i>>8;
			msg[3]=i;
			uint32_t crc_calc=crc->calc(msg, 4, 0);
			if (crc_calc== 0x83f50b46)
				printf("%x\n",i);
		}
		exit(0);
	}
#endif
	last_bit=0;
	lfsr=0;
	psk=last_psk=0;
	nrzs=0;
}
//-------------------------------------------------------------------------
double whb_decoder::cvt_temp(uint16_t raw)
{
	if (raw&0x400)
		return -((raw^0x7ff)+1)/10.0;
	else
		return raw/10.0;
}
//-------------------------------------------------------------------------
// Temp/hum
void whb_decoder::decode_02(uint8_t *msg,  uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint16_t temp=BE16(msg+2)&0x7ff;
	uint16_t temp_prev=BE16(msg+4)&0x7ff;

	if (dbg>=0) {
		printf("WHB02 ID %llx TEMP %g, PTEMP %g\n",
		       id, cvt_temp(temp), cvt_temp(temp_prev));
		fflush(stdout);
	}		
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL);
	sd.temp=cvt_temp(temp);
	sd.humidity=0;
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);
}
//-------------------------------------------------------------------------
// Temp/hum
void whb_decoder::decode_03(uint8_t *msg,  uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint16_t temp=BE16(msg+2)&0x7ff;
	uint16_t hum=BE16(msg+4)&0xff;

	uint16_t temp_prev=BE16(msg+6)&0x7ff;
	uint16_t hum_prev=BE16(msg+8)&0xff;

	uint16_t unknown=msg[10];
	if (dbg>=0) {
		printf("WHB03 ID %llx TEMP %g HUM %i, PTEMP %g PHUM %i\n",
		       id, cvt_temp(temp), hum, cvt_temp(temp_prev), hum_prev);
		fflush(stdout);
	}
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL);
	sd.temp=cvt_temp(temp);
	sd.humidity=hum;
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);
}
//-------------------------------------------------------------------------
// Temp/hum/water
void whb_decoder::decode_04(uint8_t *msg,  uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint16_t temp=BE16(msg+2)&0x7ff;
	uint16_t hum=BE16(msg+4)&0xff;
	uint8_t wet=*(msg+6);

	uint16_t temp_prev=BE16(msg+7)&0x7ff;
	uint16_t hum_prev=BE16(msg+9)&0xff;
	uint16_t wet_prev=*(msg+11);

	if (dbg>=0) {
		printf("WHB04 ID %llx TEMP %g HUM %i WET %i, PTEMP %g PHUM %i PWET %i\n",
		       id, cvt_temp(temp),hum,(wet&1)^1, cvt_temp(temp_prev), hum_prev, (wet_prev&1)^1);
		fflush(stdout);
	}
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL);
	sd.temp=cvt_temp(temp);
	sd.humidity=hum;
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);

	sd.id=(id<<4LL)|5;
	sd.temp=(wet&1)^1;
	sd.humidity=0;
	store_data(sd);
}
//-------------------------------------------------------------------------
// Temp/hum + temp2
void whb_decoder::decode_06(uint8_t *msg,  uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;;
	uint16_t temp=BE16(msg+2)&0x7ff;
	uint16_t temp2=BE16(msg+4)&0x7ff;
	uint16_t hum=BE16(msg+6)&0xff;

	uint16_t temp_prev=BE16(msg+8)&0x7ff;
	uint16_t temp2_prev=BE16(msg+10)&0x7ff;
	uint16_t hum_prev=BE16(msg+12)&0xff;
	if (dbg>=0) {
		printf("WHB06 ID %llx TEMP %g HUM %i TEMP2 %g, PTEMP %g PHUM %i PTEMP2 %g\n",
		       id, cvt_temp(temp),hum,cvt_temp(temp2), cvt_temp(temp_prev), hum_prev, cvt_temp(temp2_prev));
		fflush(stdout);
	}
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL);
	sd.temp=cvt_temp(temp);
	sd.humidity=hum;
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);

	sd.id=(id<<4LL)|1;
	sd.temp=cvt_temp(temp2);
	sd.humidity=0;
	store_data(sd);
}
//-------------------------------------------------------------------------
// Station MA10410 (TFA 35.1147.01)
void whb_decoder::decode_07(uint8_t *msg, uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint16_t temp[4]; // 0=indoor, 2/3: previous
	uint16_t hum[4];
	for(int n=0;n<4;n++) {
		temp[n]=BE16(msg+2+4*n)&0x07ff;
		hum[n]=BE16(msg+4+4*n)&0x0ff; 
	}
	if (dbg>=0) {
		printf("WHB07 ID %llx TEMP_IN %g HUM_IN %i TEMP_OUT %g HUM_OUT %i",id, cvt_temp(temp[0]), hum[0], cvt_temp(temp[1]), hum[1]);
		if (dbg>1)
			printf(" PTEMP_IN %g PHUM_IN %i PTEMP_OUT %g PHUM_OUT %i", cvt_temp(temp[2]), hum[2], cvt_temp(temp[3]), hum[3]);
		puts("");
		fflush(stdout);
	}

	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL); // ID.0 = indoor
	sd.temp=cvt_temp(temp[0]);
	sd.humidity=hum[0];
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);

	sd.id=(id<<4LL)|0xc; // ID.c = outdoor
	sd.temp=cvt_temp(temp[1]);
	sd.humidity=hum[1];
	store_data(sd);
}
//-------------------------------------------------------------------------
// Rain sensor, store counter and temperature
void whb_decoder::decode_08(uint8_t *msg, uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint8_t event=msg[2]>>6;
	uint16_t temp=BE16(msg+2)&0x07ff; // 11Bit signed, 0.1steps
	double temp_real=cvt_temp(temp);
	uint16_t cnt=BE16(msg+4);
	uint32_t times[10];
	if (dbg>=0) {
		printf("WHB08 ID %llx cnt %i\n",id, cnt);
		fflush(stdout);
	}
	for(int i=0;i<10;i++) {
		uint16_t x=BE16(msg+6+2*i);
		times[i]=timeunit_tab[(x>>14)&3] * (x&0x3fff);
		if (dbg>1)
			printf("WHB08 ID %llx #%i time %i\n",id,i,times[i]);
	}

	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL)|2;
	sd.temp=cnt;
	sd.humidity=times[1];
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);

	sd.id=(id<<4);
	sd.temp=temp_real;
	sd.humidity=0;
	store_data(sd);
}
//-------------------------------------------------------------------------
// Wind sensor
void whb_decoder::decode_0b(uint8_t *msg, uint64_t id, int rssi, int offset)
{
	uint32_t seq=BE24(msg);
	float dir[6],speed[6],gust[6];
	uint32_t times[6];
	for(int i=0;i<6;i++) {
		uint32_t v=BE32(msg+3+4*i);
		dir[i]=22.5*(v>>28); // 0: N 90:E 180:S
		speed[i]= ((v>>16)&0xff + 256*((v>>25)&1))/10.0;
		gust[i]=  ((v>>8)&0xff + 256*((v>>24)&1))/10.0;
		times[i]=(v&0xff)*2;
		if (dbg>=0 && (i==0 || dbg>0)) {
			printf("WHB0b ID %llx #%i DIR %f SPEED %f GUST %f time %i\n",
			       id, i,dir[i],speed[i],gust[i],times[i]);
			fflush(stdout);
		}		
	}
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL)|3;
	sd.temp=speed[0];
	sd.humidity=dir[0];
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);

	sd.id=(id<<4)|4;
	sd.temp=gust[0];
	sd.humidity=0;
	store_data(sd);
}
//-------------------------------------------------------------------------
// Door sensor
void whb_decoder::decode_10(uint8_t *msg, uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint8_t state[4];
	uint32_t times[4];
	for(int i=0;i<4;i++) {
		uint16_t x=BE16(msg+2+2*i);
		state[i]=x>>15; // 0=closed
		times[i]=timeunit_tab[(x>>13)&3] * (x&0x1fff);
		if (dbg>=0 && (i==0 || dbg>0)) {
			printf("WHB10 ID %llx #%i %i %i\n",id,i,state[i],times[i]);
			fflush(stdout);
		}
	}
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL)|5;
	sd.temp=state[0];
	sd.humidity=times[1];
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);
}
//-------------------------------------------------------------------------
// 4 Thermo-hygro-sensors (TFA 30.3060.01)
void whb_decoder::decode_11(uint8_t *msg, uint64_t id, int rssi, int offset)
{
	uint16_t seq=BE16(msg)&0x3fff;
	uint16_t temp[8]; // 3 = indoor, >3: previous values
	uint16_t hum[8];
	for(int n=0;n<8;n++) {
		temp[n]=BE16(msg+2+4*n)&0x07ff;
		hum[n] =BE16(msg+4+4*n)&0xff;
	}

	if (dbg>=0) {
		printf("WHB11 %llx TEMP1 %g HUM1 %i TEMP2 %g HUM2 %i TEMP3 %g HUM3 %i TEMP_IN %g HUM_IN %i",
		       id, cvt_temp(temp[0]),hum[0], cvt_temp(temp[1]),hum[1], cvt_temp(temp[2]),hum[2], cvt_temp(temp[3]),hum[3]);
		if (dbg>1)
			printf(" PTEMP1 %g PHUM1 %i PTEMP2 %g PHUM2 %i PTEMP3 %g PHUM3 %i PTEMP_IN %g PHUM_IN %i",
			       cvt_temp(temp[4]),hum[4], cvt_temp(temp[5]),hum[5], cvt_temp(temp[6]),hum[6], cvt_temp(temp[7]),hum[7]);
		puts("");
		fflush(stdout);
	}
	sensordata_t sd;
	sd.type=type;
	sd.id=(id<<4LL);
	sd.temp=cvt_temp(temp[3]);
	sd.humidity=hum[3];
	sd.sequence=seq;
	sd.alarm=0;
	sd.rssi=rssi;
	sd.flags=0;
	sd.ts=time(0);
	store_data(sd);

	for(int n=0;n<3;n++) {
		sd.id=(id<<4LL)|(0xc+n);
		sd.temp=cvt_temp(temp[n]);
		sd.humidity=hum[n];
		store_data(sd);
	}
}
//-------------------------------------------------------------------------
void whb_decoder::flush(int rssi, int offset)
{
	uint32_t crc_calc=0;
	uint32_t crc_val=0;
	int plen; 
	uint32_t stype;
	
	if (byte_cnt<11 || byte_cnt>60) // Sanity
		goto reset;
	
	// FIXME: byte count usually 2-3 bytes longer than real payload
	if (dbg && byte_cnt) {
		printf("#%03i %u L=%i  ",snum++,(uint32_t)time(0), byte_cnt);
		for(int n=0;n<byte_cnt;n++)
			printf("%02x ",rdata[n]);
		printf(" RSSI %i ",rssi);
	}
	plen=rdata[4]; // payload length

	if (plen>60)
		goto bad;
	
	stype=rdata[5]; // sensor type
	
	if (crc_initvals.find(stype)==crc_initvals.end()) {
		if (dbg>=0)
			printf("WHB: Probably unsupported sensor type %02x! Please report\n",stype);
		goto bad;
	}
	
	crc_calc=crc->calc(&rdata[4], plen-4, crc_initvals[stype]);
	crc_val=(rdata[plen]<<24) | (rdata[plen+1]<<16) | (rdata[plen+2]<<8) | rdata[plen+3];

	if (crc_calc==crc_val) {
		uint64_t id=BE48(&rdata[5]);
		uint8_t *msg=&rdata[4+1+6];
		switch(stype) {
		case 0x02:
			decode_02(msg, id, rssi, offset);
			break;
		case 0x03:
			decode_03(msg, id, rssi, offset);
			break;
		case 0x04:
			decode_04(msg, id, rssi, offset);
			break;
		case 0x06:
			decode_06(msg, id, rssi, offset);
			break;
		case 0x07:
			decode_07(msg, id, rssi, offset);
			break;
		case 0x08:
			decode_08(msg, id, rssi, offset);
			break;
		case 0x0b:
			decode_0b(msg, id, rssi, offset);
			break;
		case 0x10:
			decode_10(msg, id, rssi, offset);
			break;
		case 0x11:
			decode_11(msg, id, rssi, offset);
			break;
		}
		goto reset;
	}
 bad:
	bad++;
	if (dbg) {
		if (crc_val!=crc_calc)
			printf("\nWHB BAD %i RSSI %i (CRC is %08x, should be %08x, len %i, plen %i)\n",
			       bad, rssi, crc_val,crc_calc, byte_cnt,plen);
		else
			printf("\nWHB BAD %i RSSI %i (SANITY)\n",bad,rssi);
	}
 reset:
	sr_cnt=-1;
	sr=0;
	byte_cnt=0;
	synced=0;
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
	
	sr=(sr>>1)|(bit_descrambled<<31);

	if ( ((sr&0xffffffff)==0x2bd42d4b) ) { // FIXME 3 or 4 bytes?
		//printf("######################### SYNC\n");
		synced=1;
		sr_cnt=0;
		rdata[0]=sr&0xff;
		rdata[1]=(sr>>8)&0xff;
		rdata[2]=(sr>>16)&0xff;
		byte_cnt=3;
	}
	
	if (sr_cnt==0) {
		if (byte_cnt<(int)sizeof(rdata)) {
			rdata[byte_cnt]=(sr>>24)&0xff;
		}
		//printf(" %i %02x\n",byte_cnt,rdata[byte_cnt]);
		byte_cnt++;
	}
	if (sr_cnt>=0)
		sr_cnt=(sr_cnt+1)&7;
}
//-------------------------------------------------------------------------
whb_demod::whb_demod(decoder *_dec, double _spb) : demodulator( _dec)
{
	spb=_spb;	
	timeout_cnt=0;
	reset();
	iir=new iir2(2.0/spb); // Pulse filter
	iir_avg=new iir2(0.0025/spb); // Phase discriminator filter
	printf("WHB: Samples per bit: %.1f\n",spb);
	last_dev=0;
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
//#define DBG_DUMP
#if DBG_DUMP
// More debugging
static FILE *fx=NULL;
static FILE *fy=NULL;
static int fc=0;
#endif
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

		/* Shaped PSK of AX5031 causes hard drop at phase changes for fm_dev_nrzs()
		   -> detected minima are 0s, fillup with 1s since last 0
		*/
		dev=fm_dev_nrzs(iq[0],iq[1],last_i,last_q);		
		dev=iir->step(dev); // reduce noise
		if (!dec->has_sync())
			avg_of=iir_avg->step(0.5*dev); // decision value for phase change

		int bit=0;
		timeout_cnt--;	       

		int tdiff=step-last_peak;

		// Phase change?
		if (dev<avg_of && 
		    dev>last_dev &&
		    (tdiff>3*spb/4)  ) {
			bit=avg_of;
			dec->store_bit(0);
			bitcnt++;
			int bit0=(tdiff+spb/2)/spb;
			for(int n=1;n<bit0;n++) {
				dec->store_bit(1);
				bitcnt++;
			}
			last_peak=step;
		}
		last_dev=dev;

		if (dec->has_sync())
			rssi+=(iq[0]*iq[0]+iq[1]*iq[1]); 

#ifdef DBG_DUMP
		//  plot "blub" using 1:2 with lines,"blub" using 1:3 with boxes
		if (!fx)
			fx=fopen("blub","w");
		if (!fy)
			fy=fopen("blub1","w");
		if (fx)
			fprintf(fx,"%i %i %i %i\n",fc,dev, bit, tdiff_mod*10);		
		fc++;
#endif
		
		if (!timeout_cnt) {
			// Flush descrambler
			if (dec->has_sync()) {
				for(int n=0;n<16;n++)
					dec->store_bit(0);
				dec->flush(10*log10(1+rssi/4000),offset); // scale to rougly match with TFA_1-RSSI
			}
			reset();
			rssi=0;
		}
	}
	last_i=iq[0];
	last_q=iq[1];

	step++;
	return triggered;
}
//-------------------------------------------------------------------------
