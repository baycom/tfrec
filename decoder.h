#ifndef _INCLUDE_DECODER_H
#define _INCLUDE_DECODER_H

#include <sys/time.h>
#include <string>
#include <map>

using std::string;
using std::map;

enum sensor_e {
	TFA_1=0, // IT+ Klimalogg Pro, 30.3180, 30.3181, 30.3199(?)
	TFA_2,   // IT+ 30.3143, 30.3146(?), 30.3144
	TFA_3,   // 30.3155
	FIREANGEL=0x20 // ST-630+W2
};

typedef struct {
	sensor_e type;
	int id;
	double temp;
	int humidity;
	int alarm;
	int flags;
	int sequence;	
	time_t ts;
	int rssi;
} sensordata_t;

class decoder
{
 public:
	decoder(void);
	void set_params(char *_handler, int _mode, int _dbg);
	virtual void store_bit(int bit);
	virtual void flush(int rssi, int offset=0);
	virtual void store_data(sensordata_t &d);
	virtual void execute_handler(sensordata_t &d);
	virtual void flush_storage(void);
	int count(void) {return data.size();}

 protected:
	int dbg;
	int bad;	
 private:
	char *handler;
	int mode;
	map<int,sensordata_t> data;
};

class demodulator
{
 public:
	demodulator(decoder *_dec);
	virtual void start(int len);
	virtual void reset(void){};
        virtual int demod(int thresh, int pwr, int index, int16_t *iq);

	decoder *dec;
 protected:

	int last_bit_idx;
};

#endif
