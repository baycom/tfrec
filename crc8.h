#ifndef _INCLUDE_CRC8_H
#define _INCLUDE_CRC8_H

#include <stdint.h>

class crc8 {
 public:
        crc8(int poly);
        uint8_t calc(uint8_t *data, int len);
 private:
        uint8_t lookup[256];            
};

#endif

