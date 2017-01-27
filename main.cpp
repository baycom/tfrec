/*
	tfrec - Receiver for TFA IT+ (and compatible) sensors
	(c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

	#include <GPL-v2>
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

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
		" -t <thresh> : Set RF trigger threshold (default 150, @autogain 250)\n"
		" -w <timeout>: Run for <timeout> seconds (default: 0=forever)\n"
		" -q          : Quiet, do not print message to stdout\n"
		" -S <file>   : Save IQ-file for later debugging\n"
		" -L <file>   : Load IQ-file instead of stick data\n"
		);
}

int main(int argc, char **argv)
{
	int gain=-1;
	int freq=868250;
	int thresh=350;
	char *exec=NULL;
	int debug=0; // -1: quiet, 0: normal, 1: debug
	int timeout=0;
	int dumpmode=0; // -1: read dump file, 0: normal (data from stick), 1: save data
	char *dumpfile=NULL;
	int deviceindex=0;
	char *device=NULL;
	
	while(1) {
		signed char c=getopt(argc,argv,
			      "d:Df:g:e:t:w:qS:L:h");
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
		case 'w':
			timeout=atoi(optarg);
			break;
		case 'q':
			debug=-1;
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

	tfa_decoder dec(exec,debug);
	fsk_demod fsk(thresh, &dec,debug);

	engine e(deviceindex,freq,gain,&fsk,debug,dumpmode,dumpfile);
	e.run(timeout);
}
