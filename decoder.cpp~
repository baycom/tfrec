#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decoder.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//-------------------------------------------------------------------------
// mode=0 -> handle individual messages
// mode=1 -> handle summary (only one message per id)

decoder::decoder(sensor_e _type)
{
	type=_type;
	handler=NULL;
	mode=0;
	dbg=0;
	bad=0;
	synced=0;
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
// Shortcut for testing
void decoder::store_bytes(uint8_t *d, int len)
{
	memcpy(rdata, d, len);
	byte_cnt=len;
	synced=1;
}
//-------------------------------------------------------------------------
void decoder::flush(int rssi, int offset)
{
}
//-------------------------------------------------------------------------
void decoder::store_data(sensordata_t &d)
{
	int found=0;
	/* only first appearance of id message is stored
	   also check for identical sequence for WHB (suppress double exec)
	*/
	auto ret=data.find(d.id);
	
	if (ret==data.end())
		data.insert(std::pair<uint64_t,sensordata_t>(d.id,d));
	else if (ret->second.type==TFA_WHB) {
		if (ret->second.sequence==d.sequence)
			found=1;
		else
			ret->second.sequence=d.sequence; // track sequence	
	}

	if (!mode && !found)
		execute_handler(d);
}
//-------------------------------------------------------------------------
void decoder::execute_handler(sensordata_t &d)
{	
	if (handler && strlen(handler)) {
		char cmd[512];
		uint64_t nid;
		if (type!=TFA_WHB) {
			nid=d.id|(d.type<<24);
			//                                        t     h  s  a  r  f ts
			snprintf(cmd,sizeof(cmd),"%s %04" PRIx64 " %+.1f %g %i %i %i %i %li",
				 handler,
				 nid, d.temp, d.humidity,
				 d.sequence, d.alarm, d.rssi,
				 d.flags,
				 d.ts);
		}
		else { // WHB has really long IDs...
			nid=d.id;
			//                                     t   h  s  a  r  f ts
			snprintf(cmd,sizeof(cmd),"%s %013" PRIx64 " %+.1f %g %i %i %i %i %li",
				 handler,
				 nid, d.temp, d.humidity,
				 d.sequence, d.alarm, d.rssi,
				 d.flags,
				 d.ts);
		}
		if (dbg>=1)
			printf("EXEC %s\n",cmd);
		(void)system(cmd);
	}
}
//-------------------------------------------------------------------------
void decoder::flush_storage(void)
{
	if (!mode)
		return;
	map<uint64_t,sensordata_t>::iterator it;
	it=data.begin();
	while(it!=data.end()) {
		execute_handler(it->second);
		it++;
	}
	data.clear();
}
//-------------------------------------------------------------------------
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
