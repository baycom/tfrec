/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#include <string.h>
#include "engine.h"

//-------------------------------------------------------------------------
engine::engine(int _device, uint32_t freq, int gain, fsk_demod *_fsk, int _dbg,int _dumpmode, char* _dumpfile)
{	
        freq= freq*1000; // kHz->Hz
        srate=1536000;
	filter_type=0;
	fsk=_fsk;
	dbg=_dbg;
	dumpfile=_dumpfile;
	dumpmode=_dumpmode;

	if (dumpmode)
		printf("Dumpmode %i (%s), dumpfile %s\n",dumpmode,
							dumpmode==1?"SAVE":(dumpmode==-1?"LOAD":"NONE"),
							dumpfile);
			
	if (dumpmode>=0) {
		s=new sdr(_device,dbg,dumpmode,dumpfile);	
		if (gain==-1)
			s->set_gain(0,0);
		else
			s->set_gain(1,gain);
		
		s->set_frequency(freq);
		s->set_samplerate(srate);
	}
}
//-------------------------------------------------------------------------
engine::~engine(void)
{
}
//-------------------------------------------------------------------------
void engine::run(int timeout)
{
	FILE *dump_fd=NULL;

	if (dumpmode>=0)
		s->start();
	else {
		dump_fd=fopen(dumpfile,"rb");
		if (!dump_fd) {
			perror(dumpfile);
			exit(-1);
		}
	}
        downconvert dc(2);

	time_t start=time(0);
	
        while(1) {
		int16_t *data;
		int len;

		if (dump_fd) {
#define RLS 65536
			int16_t datab[RLS];
			{
				unsigned char buf[RLS];
				int xx=fread(buf,RLS,1,dump_fd);
				if (xx<1) {
					printf("done reading dump\n");
					exit(0);
				}
				for(int n=0;n<RLS;n++)
					datab[n] = ((buf[n]) - 128)<<6; // scale like in sdr.cpp
				data=datab;
				len=RLS;				
			}
		} else 
			s->wait(data,len); // len=total sample number = #i+#q

                int ld=dc.process_iq(data,len,filter_type);
		fsk->process(data,ld);

		if (!dump_fd)
			s->done(len);

		if (timeout && (time(0)-start>timeout))
			break;
        }
}
//-------------------------------------------------------------------------
