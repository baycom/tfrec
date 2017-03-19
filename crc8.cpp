#include "crc8.h"

//-------------------------------------------------------------------------
crc8::crc8(int poly)
{
        int n,m;
        for(n=0;n<256;n++){
                int t=n;
                for(m=0;m<8;m++) {
                        if ((t&0x80)!=0)
                                t=(t<<1)^poly;
                        else
                                t=t<<1;
                }
                lookup[n]=t;
        }
}
//-------------------------------------------------------------------------
uint8_t crc8::calc(uint8_t *data, int len)
{
        int n;
        uint8_t crc=0;

        for(n=0;n<len;n++) {
                crc=lookup[crc ^ *data];
                data++;
        }
        return crc; 
}
