#ifndef _INCLUDE_TFA5H
#define _INCLUDE_TFA5H

#include <string>
#include "dsp_stuff.h"
#include "decoder.h"
#include "crc32.h"

using std::string;

class whb_decoder: public decoder
{
 public:
	whb_decoder(sensor_e type=TFA_WHB);
	void store_bit(int bit);
	void flush(int rssi,int offset=0);
 private:
	uint32_t sr;
        int sr_cnt;
        int byte_cnt;
        uint8_t rdata[64];
        int snum;
	crc32 *crc;
	int packet_len;

	// Decoding	
	int last_bit;
	int psk,last_psk;
	int nrzs;
	uint32_t lfsr;
};

class whb_demod: public demodulator {
 public:
	whb_demod(decoder *_dec, int spb);
	void reset(void);
	int demod(int thresh, int pwr, int index, int16_t *iq);

 private:
	int spb;
	int bitcnt;
	int offset;
	int timeout_cnt;
	int last_i, last_q;
	uint64_t step;
	uint64_t last_peak;
	int rssi;
	iir2 *iir;
	iir2 *iir_avg;
        int last_ld;
	int avg_of;
};
#endif
