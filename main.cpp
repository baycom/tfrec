/*
	tfrec - Receiver for TFA IT+ (and compatible) sensors
	(c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

	#include <GPL-v2>
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "engine.h"

#include "tfa1.h"
#include "tfa2.h"
#include "whb.h"
#include "crc32.h"

//-------------------------------------------------------------------------
void timeout_handler(int sig)
{
	fprintf(stderr,"Read timeout, exiting\n");
	exit(-1);
}
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
		" -t <thresh> : Set RF trigger threshold (default 0=auto)\n"
		" -m <mode>   : 0: exec handler for every message (default), 1: summary at program exit\n"
		" -w <timeout>: Run for <timeout> seconds (default: 0=forever)\n"
		" -T <types>  : HEX Bitmask of sensor types (see below), default: 7 = TFA_1 |  TFA_2 | TFA_3 \n"
		" -q          : Quiet, do not print message to stdout\n"
		" -S <file>   : Save IQ-file for later debugging\n"
		" -L <file>   : Load IQ-file instead of stick data\n"
		);
	fprintf(stderr,
		"\nBitmask values for supported types (hex): \n"
		"\t%x: TFA_1/KlimaLogg Pro (TFA 30.3180, 30.3181, 30.3199)\n"
		"\t%x: TFA_2/17240 (TFA 30.3143-3147, 30.3159, 30.3187; Technoline TX21, TX25, TX29, TX37)\n"
		"\t%x: TFA_3/9600 (TFA 30.3155-3156; Technoline TX35)\n"
		"\t%x: Technoline TX22\n"
		"\t%x: TFA WeatherHub\n",
		1<<TFA_1,1<<TFA_2,1<<TFA_3,1<<TX22,1<<TFA_WHB);
}
//-------------------------------------------------------------------------
int main(int argc, char **argv)
{
	int gain=-1;
	int freq=868250;
	int thresh=0; // Auto
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
			types=strtol(optarg,NULL,16);
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

	vector<demodulator*> demods;

	if (types&(1<<TFA_1)) {
		// TFA1 (KlimaLoggPro)	
		printf("Registering demod for TFA_1 KlimaLoggPro\n");
		decoder *tfa1_dec=new tfa1_decoder(TFA_1);	
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

	if (types&(1<<TX22)) {
		// TX22
		printf("Registering demod for TX22, 8842 bit/s\n");
		decoder *tx22_dec=new tfa2_decoder(TX22);
		tx22_dec->set_params(exec, mode, debug);
		demods.push_back(new tfa2_demod( tx22_dec, (1536000/4.0)/8842));
	}
	/*
	if (types&(1<<TFA_WHP)) {
		// Rain sensor pulse
		printf("Registering demod for TFA_WHP , 4000 bit/s\n");
		decoder *tfa4_dec=new tfa2_decoder(TFA_WHP);
		tfa4_dec->set_params(exec, mode, debug);
		demods.push_back(new tfa2_demod( tfa4_dec, (1536000/4.0)/4000));
	}
	*/
	if (types&(1<<TFA_WHB)) {
		printf("Registering demod for TFA_WHB sensors, 6000 bit/s\n");
		decoder *whb_dec=new whb_decoder(TFA_WHB);
		whb_dec->set_params(exec, mode, debug);
		demods.push_back(new whb_demod( whb_dec, (1536000/4.0)/6000));
	}
	
	fsk_demod fsk(&demods, thresh, debug);

	engine e(deviceindex,freq,gain,&fsk,debug,dumpmode,dumpfile);
	signal(SIGALRM, timeout_handler);
	e.run(timeout);

	if (mode) {
		for(int n=0;n<demods.size();n++)
			demods.at(n)->dec->flush_storage();
	}
}
