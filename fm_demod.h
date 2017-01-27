/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#ifndef _INCLUDE_FM_DEMOD_H
#define _INCLUDE_FM_DEMOD_H

#include <string>

using std::string;

class crc8 {
 public:
	crc8(int poly);
	uint8_t calc(uint8_t *data, int len);
 private:
	uint8_t lookup[256];		
};

class tfa_decoder
{
 public:
	tfa_decoder(char *_handler, int dbg);

	void store_bit(int bit);
	void flush(int rssi);
	
 private:
	int dbg;
	char *handler;
	uint32_t sr;
	int sr_cnt;
	int byte_cnt;
	uint8_t rdata[32];
	int snum;	
	int bad;
	crc8 *crc;
};

class fsk_demod {
  public:
	fsk_demod(int _thresh, tfa_decoder *_dec, int _dbg);
	void process(int16_t *data_iq, int len);
	
 private:
	tfa_decoder *dec;
	int thresh;
	int16_t last_i, last_q;
	int last_bit_idx;
	int timeout_cnt;
	int mark_lvl;
	int rssi;
	int dbg;
};
#endif
