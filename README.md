## tfrec - A SDR tool for receiving wireless sensor data
(c) 2017 Georg Acher & Deti Fliegl, {acher|fliegl}(at)baycom.de

SEO Keywords: TFA KlimaLogg LaCrosse decoder SDR sensor Linux :)

This tool uses a RTL2832-based SDR stick to decode data sent by the
KlimaLogg Pro (and recently some other) temperature sensors made by TFA
Dostmann (http://tfa-dostmann.de) or other LaCrosse-compatible sensors.

Runs on Linux (tested on x86 and ARM/Raspberry PI3) and MacOS. Required HW
is a RTL2832-based DVB-T-stick, preferrably with a R820T-tuner. 

Received values must be externaly fed to a database, FHEM, etc!

Supported sensors are (see sensors.txt for more details):

- NRZS/38400baud: 30.3180.IT, 30.3181.IT and probably 30.3199 (pool sensor)
- NRZ/9600baud:   30.3155.WD, 30.3156.WD
- NRZ/17240baud:  30.3143.IT, 30.3144.IT, 30.3147.IT, 30.3157.IT, 30.3159.IT and probably 30.3146.IT

It is likely that the other LaCrosse-based sensors with 9600/17240baud also
work:

 TX21IT, TX25IT, TX27IT, TX29IT, TX35, TX37 (label Technoline)

Please let me know if you have any success :)

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
 
- make, C++ compiler (g++/llvm), pkg-config

- For the optional visualisation: apache, MySQL and Perl

## Installation

For x86/x86_64/MacOS simply type make. For RasPI3 use "make -f
Makefile.arm". For other architectures you need to adjust the makefile
flags.

The resulting executable is "tfrec". All options can be seen with "tfrec -h".

## Overview and internals

KlimaLogg Pro consists of a standalone base station (30.3039), up to 8
wireless sensors (30.3180.IT) and a USB-stick to download the base station
data to a PC. It is very likely that the HW is also available under the
original label LaCrosse, like some other TFA sensors.

But unlike other TFA sensors, the 30.3180.IT uses a protocol that cannot be
received by the popular JeeLink USB Stick with a RFM69 module, as it does
NRZS coding. RFM69 is only able to decode NRZ. Since initial byte
synchronisation also depends on the proper coding, this difference cannot be
corrected afterwards.

tfrec receives at 868.250MHz at 1.536MHz bandwidth, downsampled to get
sensitivity, filters appropriately, performs FSK demodulation and
NRZ(S)-decoding. The resulting telegrams (see tfa1.c for more technical
details) contain an unique15bit sensor ID, temperature (-40...+60degC),
humidity (0...100%), a low battery indicator and a CRC to detect
transmission errors.

The sensor ID is fixed and cannot be changed. It is printed on the backside
of the sensor and is also shown when inserting the battery (the last two
numbers before the temperature appears). 

The telegrams with updated sensor values are sent every 10s.

The sensitivity of tfrec with a R820T based stick is at least as good as the
TFA base station.

Advantages of tfrec vs. original PC software:

- direct access to data for home automation etc.
- no restriction to 8 sensors
- no learning procedure required
- sample time 10s
- other sensors also supported (even mixed setup)

Disadvantages:

- No storage if tfrec does not run...
  (if you need this, maybe https://github.com/matthewwall/weewx-klimalogg
  helps, it reads out the base station)

## Usage

For 868MHz the usually supplied 10cm DVB-T antenna stick is well suited.
Make sure that it is placed on a small metallic ground plane. Avoid near
WLAN, DECT or other RF stuff that may disturb the SDR stick.

To reduce other disturbances, place a sensor in about 2m distance for the
first test.

By default, all three sensor types are enabled (see -T option).

Run "tfrec -D". This uses default values and should print a hexdump of
received telegrams and the decoded values like this (KlimaLogg Pro sensor):

 #000 1485215350  2d d4 65 b0 86 20 23 60 e0 56 97           ID 65b0 +22.0 35%  seq e lowbat 0 RSSI 81

This message is sensor 65b0 (hex), 22degC, 35% relative humidity, battery OK.

The sequence counts from 0 to f for every message and allows to detect
missing messages.

RSSI is the receiver strength (values between 50 and 82). Typically
telegrams with RSSI below 55 tend to have errors.  Please note that in the
default auto gain mode RSSI values among different sensors can not be
compared.

Non-KlimaLoggPro sensors (3143,...) have an additional offset value in the
debug output. Subtract that from 868250 if you have reception problems (see
below).

If that works, you can omit the -D. Then only successfully decoded data is
printed. If you want to store the data, you can use the option -e <cmd>. For
every received message <cmd> is called with the arguments

	id temp hum seq batfail rssi timestamp

The timestamp is the typical Unix timestamp of the reception time.

For a start, try 'tfrec -q -e /bin/echo'. Then all output comes from the
echo-command. 

Other useful options:

- -g <gain>: Manual (for 820T-tuner 0...50, -1: auto gain=default)
- -d <index or name>: Use specified stick (use -d ? to list all sticks)
- -w <timeout>: Run for timeout seconds and exit
- -m <mode>: If 1, store data and only execute one handler for each ID at exit (use with -w)
- -T <type>: Enable individual sensor types, bitmask ORing the following choices:
  -		1: TFA_1 (KlimaLoggPro) 
  -		2: TFA_2 (17240bit/s)
  -		4: TFA_3 (9600bit/s)
- -t <trigger>: Manually set trigger level (usually 200-1000). If 0, the level is adjusted
                automatically (default)

## Support for non-KlimaLoggPro TFA/LaCrosse sensors:

(See tfa2.c for more details on the protocol)

These sensors do not have a unique ID. Each time the battery is inserted, a
random ID is generated. The 2-hex-digit-ID is displayed after powerup as the last
number before the temperature appears. tfrec generates a 8-hex-digit ID with
the following scheme:

 a000bccd

- a = type 1 (17240baud types), 2 (9600 baud types), 3 (TX22)
- b = static value (0 for TX22, usually 9 for others)
- cc = random ID
- d = 0 (internal temperature sensor)
  d = 1 (external temperature sensor for 3143)
  d = 1 only humidity (TX22)
  d = 2 rain counter as temperature value (TX22)
  d = 3 speed as temperature, direction as humidity (TX22)
  d = 4 gust speed as temperature (TX22)

The output format for the handler is identical to the other sensors. The
sequence counter is set to 0. If the sensor does not support humidity, it is
set to 0.

The debug output also shows the frequency offset like this:

ID 20009900 +17.2 47 12 12 RSSI 83 Offset 11kHz

This indicates that the real frequency is actually 11kHz below the center
frequency. About +-30kHz are tolerated with decreasing sensitivity at the
edges. If you have reception problems, adjust the center frequency with -f.

Note #1: The frequency offset is very inaccurate for real offsets beyond
30kHz and may indicate even the wrong direction. So better try in 10kHz
steps...

Note #2: My 3144 sensor is about 25kHz lower (at 868225kHz), maybe this
affects all sensors.

## Troubleshooting

- No messages at all

 If the sensors should be supported, please send me a dump (see below).

 Run with -DD. This prints the trigger events like 400/32768. If there
 is too much noise, the first number reaches the second. It is very
 unlikely that decoding works in this case, so the trigger
 sensitivityis automatically adjusted without altering the hardware
 gain. If this doesn't work for you, try the manual gain option or set
 the trigger level manually with '-t 1000' or even higher values.

- Missing sensor IDs

 Use manual gain and optimize antenna position depending on RSSI level

- Too many CRC errors 

 At strong RSSI: Reduce gain (manual gain), reduce strong RF sources nearby
 At weak RSSI:   Use other antenna position

 My 3144 sensor has a frequency offset and may require a slightly different
 frequency for reliable reception: Try with '-f 868225' or look at the
 offset value in the debug output.

- Unknown IDs

 Possibly there are other sensors are in the neighborhood. If they are of
 different type than yours, disable these with -T. If not, maybe reducing
 the gain helps.

- High CPU load

 Disable unwanted sensor types with -T or increase trigger with -t.


## Dump save/load

You can record an IQ dump with '-S <file>'. This file can be read instead of
real time data with '-L <file>'.

## Visualisation

There is a simple web visualisation tool in contrib/. It stores data in a
MySQL database and draws charts via Javascript.

Short setup:

- Create a database 'smarthome' with a user 'smarthome' and some password
  -   mysql> create database smarthome;  
  -   mysql> create user 'smarthome'@'localhost' identified by 'some password';
  -   mysql> grant all privileges on smarthome.* to 'smarthome'@'localhost';

- Create table with 'mysql -u smarthome -p -A smarthome < sensors.sql'

- Copy pushvals.cgi, sensors.cgi and sensors.html to a web accessible
  directory with CGI execution. Fix MySQL password in both CGIs.
  Adjust example chart setup at the beginning of sensors.html.

- Run 'tfrec -e pushvals', maybe with '-w 60' every 10minutes.

- Todos: Consolidation of database values
