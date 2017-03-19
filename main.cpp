/*
	tfrec - Receiver for TFA IT+ (and compatible) sensors
	(c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

	#include <GPL-v2>
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

#include "tfa1.h"
#include "tfa2.h"

//-------------------------------------------------------------------------
void usage(void)
{
	fprintf(stderr,
		"tfrec - Receiver for TFA IT+ (and compatible) sensors\n"
		"Options:\n"
		" -d <deviceindex or substring> : SDR stick to use (default: index 0, list all with -d ?)\n"
		" -D          : Debug, print raw messages and some other stuff\n"
		" -f <freq>   : Set frequency in kHz (default 868250)\n"		
		" -g <gain>   : Set gain (-1: auto/default, 0...50: manual)\n"
		" -e <exec>   : Executable to be called for every message (try echo)\n"
		" -t <thresh> : Set RF trigger threshold (default 150, @autogain 350)\n"
		" -m <mode>   : 0: exec handler for every message (default), 1: summary at program exit\n"
		" -w <timeout>: Run for <timeout> seconds (default: 0=forever)\n"
		" -T <types>  : Bitmask of sensor types (1: TFA_1/KlimaLogg Pro, 2: TFA_2/17240, 4: TFA_3/9600), default: all\n"
		" -q          : Quiet, do not print message to stdout\n"
		" -S <file>   : Save IQ-file for later debugging\n"
		" -L <file>   : Load IQ-file instead of stick data\n"
		);
}
//-------------------------------------------------------------------------
int main(int argc, char **argv)
{
	int gain=-1;
	int freq=868250;
	int thresh=350;
	char *exec=NULL;
	int debug=0; // -1: quiet, 0: normal, 1: debug
	int timeout=0;
	int mode=0; // :1 store all data, dump at program exit
	int dumpmode=0; // -1: read dump file, 0: normal (data from stick), 1: save data
	char *dumpfile=NULL;
	int deviceindex=0;
	char *device=NULL;
	int types=0xff;
	
	while(1) {
		signed char c=getopt(argc,argv,
			      "d:Df:g:e:t:m:w:qT:S:L:h");
		if (c==-1)
			break;
		switch (c) {
		case 'd':
			deviceindex=strtol(optarg,NULL,10);
			if (errno==EINVAL || errno==ERANGE) {
				deviceindex=sdr::search_device(optarg);
			}
			break;
		case 'D':
			debug++;
			break;
		case 'f':
			freq=atoi(optarg);
			break;
		case 'g':
			gain=atoi(optarg);
			break;
		case 'e':
			exec=strdup(optarg);
			break;
		case 't':
			thresh=atoi(optarg);
			break;
		case 'm':
			mode=atoi(optarg);
			break;
		case 'w':
			timeout=atoi(optarg);
			break;
		case 'q':
			debug=-1;
			break;
		case 'T':
			types=atoi(optarg);
			break;
		case 'S':
			dumpfile=strdup(optarg);
			dumpmode=1;
			break;
		case 'L':
			dumpfile=strdup(optarg);
			dumpmode=-1;
			break;
		case 'h':
		default:
			usage();
			return 0;
		}
	}
	if (deviceindex==-1) {
		fprintf(stderr,"Invalid SDR device\n");
		exit(-1);
	}
	if (gain!=-1 && thresh==350) //
		thresh=150;

	vector<demodulator*> demods;

	if (types&(1<<TFA_1)) {
		// TFA1 (KlimaLoggPro)	
		printf("Registering demod for TFA_1 KlimaLoggPro\n");
		decoder *tfa1_dec=new tfa1_decoder();	
		tfa1_dec->set_params(exec, mode, debug);
		demods.push_back(new tfa1_demod(tfa1_dec));
	}

	if (types&(1<<TFA_2)) {
		// TFA2 (17.24)
		printf("Registering demod for TFA_2 sensors, 17240 bit/s\n");
		decoder *tfa2_dec=new tfa2_decoder(TFA_2);
		tfa2_dec->set_params(exec, mode, debug);
		demods.push_back(new tfa2_demod( tfa2_dec, (1536000/4.0)/17240));
	}

	if (types&(1<<TFA_3)) {
		// TFA3 (9600)
		printf("Registering demod for TFA_3 sensors, 9600 bit/s\n");
		decoder *tfa3_dec=new tfa2_decoder(TFA_3);
		tfa3_dec->set_params(exec, mode, debug);
		demods.push_back(new tfa2_demod( tfa3_dec, (1536000/4.0)/9600));
	}
	
	fsk_demod fsk(&demods, thresh, debug);

	engine e(deviceindex,freq,gain,&fsk,debug,dumpmode,dumpfile);
	e.run(timeout);

	if (mode) {
		for(int n=0;n<demods.size();n++)
			demods.at(n)->dec->flush_storage();
	}
}
