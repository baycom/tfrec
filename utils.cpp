/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>


void dump16(char* name, int16_t *data, int len)
{
	printf("DUMP16 %s %i\n",name,len);
	FILE *f=fopen(name,"wb");
	fwrite(data,len,sizeof(int16_t),f);
	fclose(f);
}

void dump16i(char* name, int16_t *data, int len)
{
	printf("DUMP16i %s %i\n",name,len);
	FILE *f=fopen(name,"wb");
	for(int i=0;i<len;i+=2)
		fwrite(data+i,1,sizeof(int16_t),f);
	fclose(f);
}

void dump8(char *name, unsigned char *buf, int len)
{
	printf("DUMP8 %s %i\n",name,len);
	FILE *f=fopen(name,"wb");
	fwrite(buf,len,1,f);
	fclose(f);
}
