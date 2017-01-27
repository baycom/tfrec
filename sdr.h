/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#ifndef _INCLUDE_SDR_H
#define _INCLUDE_SDR_H

#include <string>
#include <vector>
#include <thread>

using std::thread;
using std::string;
using std::vector;

#include <rtl-sdr.h>


#include "utils.h"

class sdr 
{
public:
	sdr(int serial=0, int dbg=0,int dumpmode=0,char* dumpfile=NULL);
	~sdr(void);

	void get_properties(string &vend, string &prod,string &ser) {vend=vendor;prod=product;ser=serial;}
	int set_buffer_len(int l);
	int start(void);
	int stop(void);
	int set_frequency(uint32_t f);
	int set_gain(int mode, float g);
	int set_ppm(int p);
	int set_samplerate(int s);

//	virtual	handle_data();	
	virtual void read_data(unsigned char *buf, uint32_t len);

	int wait(int16_t* &d, int &l);
	void done(int len);
	static int search_device(char *substr);
  private:
	string vendor,product,serial;
	void read_thread(void);
	int nearest_gain(int g);

	thread *r_thread;
	int running;
	int16_t *buffer;
	int buffer_len;
	rtlsdr_dev_t *dev;
	int dbg;

	int cur_gain;
	int cur_gain_mode;
	uint32_t cur_frequ;
	int cur_ppm;
	int cur_sr;
	pthread_cond_t ready;
	pthread_mutex_t ready_m;
	FILE *dump_fd;

  public:
	volatile int wr_ptr,rd_ptr;
};

#endif
