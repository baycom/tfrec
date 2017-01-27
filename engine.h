/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#ifndef _INCLUDE_ENGINE_H
#define _INCLUDE_ENGINE_H

#include <unistd.h>
#include <stdlib.h>
#include <string>

using std::string;

#include "sdr.h"
#include "dsp_stuff.h"
#include "fm_demod.h"

class engine {
  public:
	engine(int device, uint32_t freq, int gain, fsk_demod *fsk, int dbg, int dmpmode, char *dumpfile);
	~engine(void);
	void run(int timeout);
	
	void get_properties(string &vendor, string &product, string &serial) {
		/*		if (s) {
			s->get_properties(vendor, product, serial);
			}
		*/
	};

 private:
	
	fsk_demod *fsk;
	int srate;
	int filter_type;
	int dbg;
	int read_dump;
	sdr* s;
	int dumpmode;
	char *dumpfile;
};
#endif
