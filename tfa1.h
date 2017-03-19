#ifndef _INCLUDE_TFA1H
#define _INCLUDE_TFA1H

#include <string>
#include "decoder.h"
#include "crc8.h"

using std::string;

class tfa1_decoder: public decoder
{
 public:
	tfa1_decoder(void);
	void store_bit(int bit);
	void flush(int rssi, int offset=0);
 private:
	uint32_t sr;
        int sr_cnt;
        int byte_cnt;
        uint8_t rdata[32];
        int snum;
	crc8 *crc;
};

class tfa1_demod: public demodulator {
 public:
	tfa1_demod( decoder *_dec);
	int demod(int thresh, int pwr, int index, int16_t *iq);

 private:
	int timeout_cnt;
	int last_i, last_q;
	int mark_lvl;
	int rssi;
};
#endif
