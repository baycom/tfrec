## tfrec - A SDR tool for receiving wireless sensor data
(c) 2017 Georg Acher & Deti Fliegl, {acher|fliegl}(at)baycom.de

This tool uses a RTL2832-based SDR stick to decode data sent by the
KlimaLogg Pro temperature sensors made by TFA Dostmann (tfa-dostmann.de). 

Runs on Linux (tested on x86 and ARM/Raspberry PI3) and MacOS. Required HW
is a RTL2832-based DVB-T-stick, preferrably with a R820T-tuner.

## License

GPLv2   http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html

## SW Requirements

- rtl-sdr (library/headers/utilities)

 Debian/Ubuntu: package name 'librtlsdr-dev'
 Mac ports: package name 'rtl-sdr'
 Or download+install from https://github.com/steve-m/librtlsdr
 
 Note: Make sure that the SDR stick can be accessed. The rtl_test
 utility has to run without any problems. Under Linux you need to add
 udev rules to use the stick as a non-root-user.
 
- make, C++ compiler (g++/llvm)

- For the optional visualisation: apache, MySQL and Perl

## Installation

For x86/x86_64/MacOS simply type make. For RasPI3 use "make -f
Makefile.arm". For other architectures you need to adjust the makefile
flags.

The resulting executable is "tfrec".

## Overview and internals

KlimaLogg Pro consists of a standalone base station (30.3039), up to 8
wireless sensors (30.3180.IT or 30.3181.IT) and a USB-stick to download the base station
data to a PC. It is very likely that the HW is also available under the
original label LaCrosse, like some other TFA sensors.

But unlike other TFA sensors, the 30.3180.IT or 30.3181.IT uses a protocol that cannot be
received by the popular JeeLink USB Stick with a RFM69 module, as it does
NRZS coding.  RFM69 is only able to decode NRZ.  Since initial byte
synchronisation also depends on the proper coding, this difference cannot be
corrected afterwards.

tfrec receives at 868.250MHz at 1.536MHz bandwidth, downsampled to get
sensitivity, filters appropriately, performs FSK demodulation and
NRZS-decoding.  The resulting telegrams (see fm_demod.c for more technical
details) contain a 15bit sensor ID, temperature (-40...+60degC), humidity
(0...100%), a low battery indicator and a CRC to detect transmission errors.

The 30.3181.IT does not provide a humidity sensor and tfrec reports 0% instead. 

The sensor ID is fixed and cannot be changed. It is printed on the backside
of the sensor and is also shown when inserting the battery (the last two
numbers before the temperature appears). 

The telegrams with updated sensor values are sent every 10s.

The sensitivity of a R820T based stick is at least as good as the TFA base
station. 

Advantages of tfrec vs. original PC software:

- Direct access to data for home automation etc.
- no restriction to 8 sensors
- no learning procedure required
- sample time 10s

Disadvantages:

- No storage if tfrec does not run...


## Usage

For 868MHz the supplied 10cm DVB-T antenna stick is well suited.  Make sure
that it is placed on a small ground plane.  Avoid near WLAN, DECT or other
RF stuff that may disturb the SDR stick.

To reduce other disturbances, place a sensor in about 2m distance for the
first test.

Run "tfrec -D". This uses default values and should print a hexdump of
received telegrams and the decoded values like this:

 #000 1485215350  2d d4 65 b0 86 20 23 60 e0 56 97           ID 65b0 +22.0 35%  seq e lowbat 0 RSSI 81

This message is sensor 65b0 (hex), 22degC, 35% relative humidity, battery OK.

The sequence counts from 0 to f for every message and allows to detect
missing messages.

RSSI is the receiver strength (values between 50 and 82).  Typically
telegrams with RSSI below 55 tend to have errors.  Please note that in the
default auto gain mode RSSI values among different sensors can not be
compared.

If that works, you can omit the -D. Then only successfully decoded data is
printed. If you want to store the data, you can use the option -e <cmd>. For
every received message <cmd> is called with the arguments
id, temp, hum, seq, batfail, rssi. 

For a start, try 'tfrec -q -e /bin/echo'. Then all output comes from the
echo-command. 

Other useful options:

-g <gain>: Manual (for 820T-tuner 0...50, -1: auto gain)
-d <index or name>: Use specified stick (use -d ? to list all sticks)
-w <timeout>: Run for timeout seconds and exit

## Troubleshooting

- No messages at all

 Only 30.3180.IT sensors are currently supported.  If there are other
 KlimaLogg Pro sensors, please send me a dump (see below).

 Run with -DD. This prints the trigger events like 400/32768. If there is too much
 noise, the first number reaches the second. It is very unlikely that
 decoding works in this case. Try the manual gain option or increase the
 trigger level with '-t 1000' or even higher values.

- Missing sensor IDs

 Use manual gain and optimize antenna position depending on RSSI level

- Too many CRC errors 

 At strong RSSI: Reduce gain (manual gain), reduce strong RF sources nearby
 At weak RSSI:   Use other antenna position

## Dump

You can record an IQ dump with '-S <file>'. This file can be read instead of
real time data with '-L <file>'.

## Visualisation

There is a simple web visualisation tool in contrib/. It stores data in a
MySQL database and draws charts via Javascript.

Short setup:

- Create a database 'smarthome' with a user 'smarthome' and some password
   mysql> create database smarthome;
   mysql> create user 'smarthome'@'localhost' identified by 'some password';
   mysql> grant all privileges on smarthome.* to 'smarthome'@'localhost';

- Create table with 'mysql -u smarthome -p -A smarthome < sensors.sql'

- Copy pushvals.cgi, sensors.cgi and sensors.html to a web accessible
  directory with CGI execution. Fix MySQL password in both CGIs.
  Adjust example chart setup at the beginning of sensors.html.

- Run 'tfrec -e pushvals', maybe with '-w 60' every 10minutes.

- Todos: Consolidation of database values




 







