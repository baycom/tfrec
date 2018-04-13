#include "crc32.h"
#include <stdio.h>
//-------------------------------------------------------------------------
crc32::crc32(uint32_t poly)
{
        int n,m;
        for(n=0;n<256;n++){
                uint32_t t=n;
                for(m=0;m<32;m++) {
                        if ((t&0x80000000)!=0)
                                t=(t<<1)^poly;
                        else
                                t=t<<1;
                }
                lookup[n]=t;
        }
}
//-------------------------------------------------------------------------
uint32_t crc32::calc(uint8_t *data, int len, uint32_t init)
{
        int n;
        uint32_t crc=init;

        for(n=0;n<len;n++) {
		//		printf("%08x %08x %08x\n",crc>>8, lookup[(crc ^ *data)&0xff], (crc>>8) ^ lookup[(crc ^ *data)&0xff]);
                crc=(crc<<8) ^ lookup[(crc>>24)^ *data];
		data++;
        }
        return crc; 
}
