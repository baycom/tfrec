/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#ifndef _DSP_STUFF_H
#define _DSP_STUFF_H

#include <string>
#include <vector>
#include <thread>
using std::thread;
using std::string;
using std::vector;

class decimate {
  public:
	decimate(void);
	~decimate(void);
	int process2x(int16_t *data, int length); // IQ
	int process2x1(int16_t *data, int length); // IQ
  private:
	int16_t hist[128];
	int16_t hist0[128];
	int16_t hist1[128];
};

class downconvert {
  public:
	downconvert(int p);
	~downconvert(void);

	int process_iq(int16_t *buf,int len, int filter=0);
	
  private:
	vector<decimate> dec_i,dec_q;
	int passes;
};

int fm_dev(int ar, int aj, int br, int bj);


#endif
