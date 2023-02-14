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
	double cvt_temp(uint16_t raw, int extended=0);
	void decode_02(uint8_t *msg, uint64_t id, int rssi, int offset); // temp
	void decode_03(uint8_t *msg, uint64_t id, int rssi, int offset); // temp/hum
	void decode_04(uint8_t *msg, uint64_t id, int rssi, int offset); // temp/hum/water
	void decode_06_09(uint8_t *msg, uint64_t id, int rssi, int offset, int extended=0); // temp/hum + temp (TFA 30.3304.02, extended range 30.3302.02)
	void decode_07(uint8_t *msg, uint64_t id, int rssi, int offset); // Station MA10410 (TFA 35.1147.01)
	void decode_08(uint8_t *msg, uint64_t id, int rssi, int offset); // rain
	void decode_0b(uint8_t *msg, uint64_t id, int rssi, int offset); // wind
	void decode_10(uint8_t *msg, uint64_t id, int rssi, int offset); // door
	void decode_11(uint8_t *msg, uint64_t id, int rssi, int offset); // 4 Thermo-hygro-sensors (TFA 30.3060.01)
	void decode_12(uint8_t *msg, uint64_t id, int rssi, int offset); // Humidity guard/cosy radar (TFA 30.5043.01)
	
	uint32_t sr;
        int sr_cnt;
        int snum;
	crc32 *crc;

	// Decoding	
	int last_bit;
	int psk,last_psk;
	int nrzs;
	uint32_t lfsr;
};

class whb_demod: public demodulator {
 public:
	whb_demod(decoder *_dec, double spb);
	void reset(void);
	int demod(int thresh, int pwr, int index, int16_t *iq);

 private:
	double spb;
	int bitcnt;
	int offset;
	int timeout_cnt;
	int last_i, last_q;
	int last_dev;
	uint64_t step;
	uint64_t last_peak;
	double rssi;
	iir2 *iir;
	iir2 *iir_avg;
	int avg_of;
};
#endif
