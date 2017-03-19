#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decoder.h"

//-------------------------------------------------------------------------
// mode=0 -> handle individual messages
// mode=1 -> handle summary (only one message per id)

decoder::decoder(void)
{
	handler=NULL;
	mode=0;
	dbg=0;
	bad=0;
}
//-------------------------------------------------------------------------
void decoder::set_params(char *_handler, int _mode, int _dbg)
{
	handler=_handler;
	mode=_mode;
        dbg=_dbg;	
}
//-------------------------------------------------------------------------
void decoder::store_bit(int bit)
{
}
//-------------------------------------------------------------------------
void decoder::flush(int rssi, int offset)
{
}
//-------------------------------------------------------------------------
void decoder::store_data(sensordata_t &d)
{	
	// only first appearance of id message is stored
	if (data.find(d.id)==data.end())
		data.insert(std::pair<int,sensordata_t>(d.id,d));

	if (!mode)
		execute_handler(d);
}
//-------------------------------------------------------------------------
void decoder::execute_handler(sensordata_t &d)
{	
	if (handler && strlen(handler)) {
		char cmd[512];
		int nid=d.id|(d.type<<24);
		//                                  t   h   s  a  r  f ts
		snprintf(cmd,sizeof(cmd),"%s %04x %+.1f %i %i %i %i %i %i",
			 handler,
			 nid, d.temp, d.humidity,
			 d.sequence, d.alarm, d.rssi,
			 d.flags,
			 d.ts);
		if (dbg==1)
			printf("EXEC %s\n",cmd);
		(void)system(cmd);
	}
}
//-------------------------------------------------------------------------
void decoder::flush_storage(void)
{
	if (!mode)
		return;
	map<int,sensordata_t>::iterator it;
	it=data.begin();
	while(it!=data.end()) {
		execute_handler(it->second);
		it++;
	}
	data.clear();
}
//-------------------------------------------------------------------------
demodulator::demodulator(decoder *_dec)
{
        dec=_dec;
	last_bit_idx=0;
}
//-------------------------------------------------------------------------
void demodulator::start(int len)
{
        if (last_bit_idx) // handle data wrap from last block processing
                last_bit_idx-=len;      
}
//-------------------------------------------------------------------------
int demodulator::demod(int thresh, int pwr, int index, int16_t *iq)
{
	return 0;
}
//-------------------------------------------------------------------------
