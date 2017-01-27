/*
        tfrec - Receiver for TFA IT+ (and compatible) sensors
        (c) 2017 Georg Acher, Deti Fliegl {acher|fliegl}(at)baycom.de

        #include <GPL-v2>

  sdr.cpp -- wrapper around librtlsdr
 
  Parts taken from librtlsdr (rtl_fm.c)
  rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
  Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
  Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
  Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
  Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "sdr.h"

//-------------------------------------------------------------------------
sdr::sdr(int dev_index, int _dbg, int dumpmode, char *dumpfile)
{
	buffer=0;
	wr_ptr=0;
	rd_ptr=0;
	r_thread=NULL;
	running=0;
	dev=NULL;
	dump_fd=NULL;
	
	if (dumpmode>0 && dumpfile) {
		dump_fd=fopen(dumpfile,"wb");
		if (!dump_fd) {
			perror(dumpfile);
			exit(-1);
		}
	}

	set_buffer_len(2*1024*1024);
	vendor="";
	product="";
	serial="";

	dbg=_dbg;
	char vendorc[256], productc[256], serialc[256];
        rtlsdr_get_device_usb_strings(dev_index, vendorc, productc, serialc);
	vendor=vendorc;
	product=productc;
	serial=serialc;
	int r;
	while(1) {
		r=rtlsdr_open(&dev, dev_index);
		if (!r) 
			break;
		fprintf(stderr, "RET OPEN %i, retry\n",r);
		sleep(1);
	}
	set_ppm(0);
//	set_gain(0,0);	
	pthread_cond_init(&ready, NULL);
	pthread_mutex_init(&ready_m, NULL);
}
//-------------------------------------------------------------------------
sdr::~sdr(void)
{
	stop();
}
//-------------------------------------------------------------------------
int sdr::search_device(char *substr)
{
	int i, device_count, device, offset;
        char *s2;
        char vendor[256], product[256], serial[256],sum[768];
        device_count = rtlsdr_get_device_count();
        if (!device_count) {
                printf("No supported devices found.\n");
                return -1;
        }
	if (!strcmp(substr,"?"))
		printf("Found %d device(s):\n", device_count);
        for (i = 0; i < device_count; i++) {
                rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (!strcmp(substr,"?"))
			printf("  %d:  %s, %s, SN%s\n", i, vendor, product, serial);
		else {
			snprintf(sum,sizeof(sum),"%s %s SN%s",vendor, product, serial);
			if(strcasestr(sum,substr)) {
				printf("match device index %i %s\n",i,sum);
				return i;
			}
		}
        }
	return -1;
}
//-------------------------------------------------------------------------
int sdr::start(void) 
{
	if (!dev)
		return -1;

	int r;
        r=rtlsdr_reset_buffer(dev);
	running=1;
	r_thread=new thread(&sdr::read_thread,this);
	return 0;
}
//-------------------------------------------------------------------------
int sdr::stop(void)
{
	if (r_thread) {
		rtlsdr_cancel_async(dev);
		r_thread->join();
		delete r_thread;
		r_thread=0;
	}
	running=0;
	return 0;
}
//-------------------------------------------------------------------------
// l: number of samples
int sdr::set_buffer_len(int l)
{
	if (running)
		return 0;
	if (buffer)
		free(buffer);

	buffer=(int16_t*)malloc(sizeof(int16_t) * l);

	buffer_len=l;
	wr_ptr=0;
	rd_ptr=0;
	return 0;
}
//-------------------------------------------------------------------------
int sdr::set_frequency(uint32_t f)
{
	if (!dev)
		return -1;
	int r;
	r=rtlsdr_set_center_freq(dev, f);
	cur_frequ=f;
	if (dbg==1)
		printf("Frequency %.4lfMHz\n",(float)(f/1e6));
	return r;
}
//-------------------------------------------------------------------------
int sdr::nearest_gain(int g)
{
	int i, r, err1, err2, count, nearest;
        int* gains;
        r = rtlsdr_set_tuner_gain_mode(dev, 1);
        if (r < 0) {
                fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
                return r;
        }
        count = rtlsdr_get_tuner_gains(dev, NULL);
        if (count <= 0) {
                return 0;
        }
        gains =(int*) malloc(sizeof(int) * count);
        count = rtlsdr_get_tuner_gains(dev, gains);
        nearest = gains[0];
        for (i=0; i<count; i++) {
                err1 = abs(g - nearest);
                err2 = abs(g - gains[i]);
                if (err2 < err1) {
                        nearest = gains[i];
                }
        }
        free(gains);
        return nearest;
}
//-------------------------------------------------------------------------
// mode=0 -> auto gain
int sdr::set_gain(int mode,float g)
{
	if (!dev)
		return -1;
	int r;
	if (mode==0) {
		if (dbg)
			printf("AUTO GAIN\n");

		r=rtlsdr_set_tuner_gain_mode(dev,0);
		cur_gain=0;
	} else {
		cur_gain=nearest_gain(g*10);
		if (dbg)
			printf("GAIN %.1f\n",cur_gain/10.0);
		r=rtlsdr_set_tuner_gain_mode(dev, 1);
		r=rtlsdr_set_tuner_gain(dev, cur_gain);
	}
	cur_gain_mode=mode;
	return cur_gain;
}
//-------------------------------------------------------------------------
int sdr::set_ppm(int p)
{
	if (!dev)
		return -1;
	int r=rtlsdr_set_freq_correction(dev, p);
	cur_ppm=p;
	return r;
}
//-------------------------------------------------------------------------
int sdr::set_samplerate(int s)
{
	if (!dev)
		return -1;
	int r;
        r=rtlsdr_set_sample_rate(dev, s);
	if (dbg)
		printf("Samplerate %i\n",s);
	cur_sr=s;
	return r;
}
//-------------------------------------------------------------------------

void sdr::read_data(unsigned char *buf, uint32_t len)
{
//	printf("Got %i\n",len);
	int w=wr_ptr;

	if (dump_fd)
		fwrite(buf,len,1,dump_fd);

	for (uint32_t i=0;i<len;i++) {
		buffer[w++] = ((int16_t)(buf[i]) - 128)<<6; // Scale to +-8192
		if (w>=buffer_len)
			w=0;
	}
	wr_ptr=w;
	safe_cond_signal(&ready, &ready_m);
}
//-------------------------------------------------------------------------
static void sdr_read_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	if (!ctx)
		return;

	sdr *this_ptr=(sdr*)ctx;
	this_ptr->read_data(buf,len);
}
//-------------------------------------------------------------------------
void sdr::read_thread(void)
{
	if (dbg)
		printf("START READ THREAD\n");
	rtlsdr_read_async(dev, sdr_read_callback, this, 0, buffer_len/8);
}
//-------------------------------------------------------------------------
int sdr::wait(int16_t* &d, int &len)
{
	safe_cond_wait(&ready, &ready_m);
	d=buffer+rd_ptr;
	if (rd_ptr<wr_ptr)
		len=wr_ptr-rd_ptr;
	else
		len=buffer_len-rd_ptr; // ignore wraparound, defer to next read
	return 0;
}
//-------------------------------------------------------------------------
void sdr::done(int len)
{
	int r=rd_ptr+len;
	if (r>=buffer_len)
		r=r-buffer_len;
	rd_ptr=r;
}
//-------------------------------------------------------------------------
