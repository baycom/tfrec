#ifndef _INCLUDE_CRC32_H
#define _INCLUDE_CRC32_H

#include <stdint.h>

class crc32 {
 public:
        crc32(uint32_t poly);
        uint32_t calc(uint8_t *data, int len, uint32_t init=0);
 private:
        uint32_t lookup[256];            
};

#endif

