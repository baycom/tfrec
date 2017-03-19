#ifndef _INCLUDE_TFA2H
#define _INCLUDE_TFA2H

#include <string>
#include "dsp_stuff.h"
#include "decoder.h"
#include "crc8.h"

using std::string;

class tfa2_decoder: public decoder
{
 public:
	tfa2_decoder(sensor_e type=TFA_2);
	void store_bit(int bit);
	void flush(int rssi,int offset=0);
 private:
	sensor_e type;
	int invert;
	uint32_t sr;
        int sr_cnt;
        int byte_cnt;
        uint8_t rdata[32];
        int snum;
	crc8 *crc;
};

class tfa2_demod: public demodulator {
 public:
	tfa2_demod(decoder *_dec, int spb);
	void reset(void);
	int demod(int thresh, int pwr, int index, int16_t *iq);

 private:
	int spb;
	int bitcnt;
	int dmin,dmax;
	int offset;
	int timeout_cnt;
	int last_i, last_q;
	int last_bit;
	int rssi;
	iir2 *iir;
};
#endif
