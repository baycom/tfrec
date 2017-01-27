/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>
*/

#ifndef _INCLUDE_UTILS_H
#define _INCLUDE_UTILS_H

#include <pthread.h>
#include <stdint.h>

#define safe_cond_signal(n, m) {pthread_mutex_lock(m); pthread_cond_signal(n); pthread_mutex_unlock(m);}
#define safe_cond_wait(n, m) {pthread_mutex_lock(m); pthread_cond_wait(n, m); pthread_mutex_unlock(m);}

void dump8(char *name, unsigned char *buf, int len);
void dump16(char* name, int16_t *data, int len);
void dump16i(char* name, int16_t *data, int len);

#endif
