PACKAGES= librtlsdr

CXX=g++

PROFILING= #-g
DEFINES=
PKG_INCLUDES= $(shell pkg-config --cflags $(PACKAGES))
PKG_LIBS= $(shell pkg-config --libs $(PACKAGES))

# Default flags, should work for all
CXXFLAGS=-O3 $(PROFILING) -std=c++0x -Wall \
              -Wno-unused-function  -Wno-unused-variable -Wno-unused-but-set-variable \
              -Wno-write-strings
              
ARCH=$(shell uname -m)
$(info Found system type '$(ARCH)')
 
# x86_64
ifeq ($(ARCH), x86_64)
 CXXFLAGS +=   -msse3 -mfpmath=sse -ffast-math
 $(info Flags for x86-64)
endif
# x32
ifeq ($(ARCH), x86)
 CXXFLAGS +=   -msse3 -mfpmath=sse -ffast-math
 $(info Flags for x86) 	
endif

# RPi 3/4 64Bit
ifeq ($(ARCH), aarch64)
 # not sure if it really makes a difference
 CXXFLAGS += -funsafe-math-optimizations -ftree-vectorize -ffast-math 
 $(info Flags for Raspberry Pi 3/4 64 Bit)	
endif	 

# RPi 3/4 32Bit
ifeq ($(ARCH), armv7l)
 CXXFLAGS += -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -ftree-vectorize -ffast-math
 $(info Flags for Raspberry Pi 3/4 32 Bit)	
endif

# RPi 2 + Zero
ifeq ($(ARCH), armv6l)
 CXXFLAGS += -mfpu=vfp -march=armv6 -mfloat-abi=hard -ftree-vectorize -ffast-math 	
 $(info Flags for Raspberry Pi 2/Zero 32 Bit)	
endif

INCLUDES= $(PKG_INCLUDES) 

LIBS=-lm $(PKG_LIBS) -lpthread
LDFLAGS=$(PROFILING) -rdynamic

CFILES=main.cpp engine.cpp  dsp_stuff.cpp fm_demod.cpp decoder.cpp crc8.cpp \
	tfa1.cpp tfa2.cpp whb.cpp utils.cpp sdr.cpp crc32.cpp

OBJS = $(CFILES:.cpp=.o)

all: tfrec

tfrec: $(OBJS)
	 $(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $<

ifneq ($(MAKECMDGOALS),clean)
include .depend
endif

.depend: Makefile
	touch .depend
	makedepend $(DEF) $(INCL)  $(CFLAGS) $(CFILES) -f .depend >/dev/null 2>&1 || /bin/true

clean :
	$(RM) $(OBJS) *~
	$(RM) tfrec
	$(RM) .depend
