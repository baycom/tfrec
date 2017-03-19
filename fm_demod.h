/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#ifndef _INCLUDE_FM_DEMOD_H
#define _INCLUDE_FM_DEMOD_H

#include <string>
#include <vector>

using std::vector;

#include "decoder.h"

class fsk_demod {
  public:
	fsk_demod(vector<demodulator*> *_demods, int _thresh, int _dbg);
	void process(int16_t *data_iq, int len);
	
 private:
	int thresh;
	int16_t last_i, last_q;
	int dbg;
	vector<demodulator*> *demods;
};
#endif
