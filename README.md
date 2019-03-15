## tfrec - A SDR tool for receiving wireless sensor data
(c) 2017-2018 Georg Acher & Deti Fliegl, {acher|fliegl}(at)baycom.de

SEO Keywords: TFA KlimaLogg LaCrosse decoder SDR WeatherHub sensor Linux :)

This tool uses a RTL2832-based SDR stick to decode data sent by the
KlimaLogg Pro (and recently some other) temperature sensors made by TFA
Dostmann (http://tfa-dostmann.de) or other Technoline/LaCrosse-compatible sensors.

Runs on Linux (tested on x86 and ARM/Raspberry PI3) and MacOS. Required HW
is a RTL2832-based DVB-T-stick, preferrably with a R820T-tuner. 

Received values must be externaly fed to a database, FHEM, etc!

Supported sensors are (see sensors.txt for more details):

- NRZS/38400baud: 30.3180.IT, 30.3181.IT and probably 30.3199 (pool sensor)
- NRZ/9600baud:   30.3155.WD, 30.3156.WD
- NRZ/17240baud:  30.3143.IT, 30.3144.IT, 30.3147.IT, 30.3157.IT, 30.3159.IT and probably 30.3146.IT
- NRZ/8842baud:   Technoline TX22
- NRZS/6000baud:  WeatherHub sensors (TFA 30.3303.02, 30.3305.02, 30.3306.02, 30.3307.02 30.3311.02, 
                  MA10410/TFA 35.1147.01, TFA 35.1147.01, 30.3304.02, 30.5043.01 
                  probably others like Technoline Mobile Alerts)

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
Makefile.arm", for RasPI 2 or Zero  "make -f Makefile.raspi2". For other
architectures you need to adjust the makefile flags.

The resulting executable is "tfrec". All options can be seen with "tfrec -h".

## Overview and internals for KlimaLogg Pro sensors

(see below for other sensors)

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

By default, three sensor types (TFA_1/2/3) are enabled (see -T option
for enabling TX22 and WeatherHub).

Run "tfrec -D". This uses default values and should print a hexdump of
received telegrams and the decoded values like this (KlimaLogg Pro sensor):

 #000 1485215350  2d d4 65 b0 86 20 23 60 e0 56 97           ID 65b0 +22.0 35%  seq e lowbat 0 RSSI 81

This message is sensor 65b0 (hex), 22degC, 35% relative humidity, battery OK.

The sequence counts from 0 to f for every message and allows to detect
missing messages.

RSSI is the receiver strength (usually values between 50 and 85). Typically
telegrams with RSSI below 55 tend to have errors.  Please note that in the
default auto gain mode RSSI values among different sensors can not be
compared. Use fixed gain for determining weaker sensors.

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
- -W: Use wider filter (+-80kHz vs +-44kHz), tolerates more frequency offset, but is less sensitive
- -T <type>: Enable individual sensor types, bitmask ORing the following choices as hex value:
  - 1: TFA_1 (KlimaLoggPro) 
  - 2: TFA_2 (17240bit/s)
  - 4: TFA_3 (9600bit/s)
  - 8: TX22 (8842bit/s)          NOT ENABLED BY DEFAULT!
  - 20: WeatherHub (6000bit/s)    NOT ENABLED BY DEFAULT!
  - Example: "-T 2a" enables TFA_2 (2), TX22 (8) and WeatherHub (20)
- -t <trigger>: Manually set trigger level (usually 200-1000). If 0, the level is adjusted
                automatically (default)

## Support for non-KlimaLoggPro TFA/LaCrosse sensors:

### Non-WeatherHub (TFA_2, TFA_3, TX22)
(See tfa2.c for more details on the protocol)

These sensors do not have a unique ID. Each time the battery is inserted, a
random ID is generated. The 2-hex-digit-ID is displayed after powerup as the last
number before the temperature appears. tfrec generates a 8-hex-digit ID with
the following scheme:

 a000bccd

- a = type 1 (17240baud types), 2 (9600 baud types), 3 (TX22)
- b = static value (0 for TX22, usually 9 for others)
- cc = random ID
- d = subtype
  - d = 0 (internal temperature sensor)
  - d = 1 (external temperature sensor for 3143)
  - d = 1 only humidity (TX22)
  - d = 2 rain counter as temperature value (TX22)
  - d = 3 speed as temperature in m/s, direction as humidity (TX22)
  - d = 4 gust speed as temperature in m/s (TX22)

The output format for the handler is identical to the other sensors. The
sequence counter is set to 0. If the sensor does not support humidity, it is
set to 0.

Please note that the TX22 system can deliver multiple outputs for each
of its subsystems. With -D, only the actually sent values are printed.

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

### WeatherHub sensors

These sensors have unique 6 byte IDs, usually printed on sensor. The
first byte describes the sensor type. Sensors values for rain, wind
and door states are mapped like TX22 by the last nibble of the ID 
at the temperature and humidity values:

IIIIIIIIIIIIT

- I: 6 Byte ID, T: Type (4bit)
- Subtype
  - T=0: temperature, humidity (0 if not available), indoor values for stations with multiple sensors
  - T=1: External (cable) temperature sensor (WHB type 7)
  - T=2: Rain sensor: tempval=rain-counter, hum=time in s since last pulse
  - T=3: Wind sensor: tempval=speed (m/s), hum=direction (degree)
  - T=4: Wind sensor: tempval=gust speed (m/s)
  - T=5: Door/water sensor: tempval=state (1=open/wet), hum=time in s since last event (only door sensor)
  - T=0xc to 0xe: temperature/humidity of extra sensors (or stations)
  
Thus, some sensor types deliver two or more output messages in one go, see the following examples:

Wind sensor with ID 0b3d9ddeeabc:

```
0b3d9ddeeabc2 +0.7 270 950 0 92 0 1525996300    -> 2=Wind 0.7m/s from west (270deg)
0b3d9ddeeabc3 +5.8 0 950 0 92 0 1525996300      -> 3=Gust 5.8m/s
```

Temperature/Humidity/Wetness sensor with ID 0469fe50dabc:
```
0469fe50dabc0 +21.3 51 16394 0 90 0 1525979222  -> 0= Temp +21.3, humidity 51%
0469fe50dabc5 +1.0 0 16394 0 90 0 1525979222    -> 5= State=1 (wet)
```

Rain sensor with ID 0833c2708abc (0.25mm/m^2 rain per count)
```
0833c2708abc2 +9.0 774 10 0 92 0 1525998514   -> 2= Counter 9, last event 774s ago
0833c2708abc0 +21.0 0 10 0 92 0 1525998514    -> 0= Temp 21.0
```

Station ID 117addaf2ff6 with one internal and 3 external T/H sensors (TFA 30.3060.01)
```
117addaf2ff60 +22.0 55 1772 0 0 0 1540944521    -> 0=indoor
117addaf2ff6c +11.6 86 1772 0 0 0 1540944521    -> c=sensor#1
117addaf2ff6d +22.2 51 1772 0 0 0 1540944521    -> d=sensor#2
117addaf2ff6e +18.8 62 1772 0 0 0 1540944521    -> e=sensor#3
```

Station ID 124a42a3d8e3 with internal T/H and averaged H (TFA 30.5043.01)
```
124a42a3d8e30 +21.5 52 1263 0 0 0 1552610966    -> 0= indoor temp/hum.
124a42a3d8e31 +0.0 51 1263 0 0 0 1552610966     -> 1= 3h average hum.
124a42a3d8e3c +0.0 53 1263 0 0 0 1552610966     -> c= 24h average hum.
124a42a3d8e3d +0.0 51 1263 0 0 0 1552610966     -> d= 7d average hum.
124a42a3d8e3e +0.0 0 1263 0 0 0 1552610966      -> e= 30d average hum.
```

Some sensors (rain, wind, door) send a history of previous values. This history is
currently just internally decoded but not used. You can see if with the "-DD" option.

## Troubleshooting

- No messages at all

 If the sensors should be supported, please send me a dump (see below).

 Run with -DDD. This prints the trigger events like 400/32768. If there
 is too much noise, the first number reaches the second. It is very
 unlikely that decoding works in this case, so the trigger
 sensitivity is automatically adjusted without altering the hardware
 gain. If this doesn't work for you, try the manual gain option or set
 the trigger level manually with '-t 1000' or even higher values.

- Missing sensor IDs

 Use manual gain and optimize antenna position depending on RSSI level

- Too many CRC errors 

 At strong RSSI: Reduce gain (manual gain), reduce strong RF sources nearby
 At weak RSSI:   Use other antenna position

 My 3144/TX22 sensors have a frequency offset and may require a slightly
 different frequency for reliable reception: Either try with '-f 868225' or use
 "-W" for a wider filter. You can also look at the offset value in the debug
 output for correctly received messages and use that as a correction value.

- Unknown IDs

 Possibly there are other sensors are in the neighborhood. If they are of
 different type than yours, disable these with -T. If not, maybe reducing
 the gain helps.

- High CPU load

 Disable unwanted sensor types with -T or increase trigger with -t.

- "Unsupported sensor type" for WeatherHub sensors

There is a "secret" type specific value (CRC init) that can only be safely
calculated with at least 2 *different* raw messages (3 are better). I have
determined it for 8 types (02, 03, 04, 06, 07, 08, 0b, 10, and 11) and also
implemented the value decoding just for these sensors. For other types
please send me a few (different) raw messages received by the -D option.

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
