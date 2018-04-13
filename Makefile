PACKAGES= librtlsdr

CXX=g++

PROFILING= #-g
DEFINES=
PKG_INCLUDES= $(shell pkg-config --cflags $(PACKAGES))
PKG_LIBS= $(shell pkg-config --libs $(PACKAGES))

CXXFLAGS=-O3 $(PROFILING) -msse3 -mfpmath=sse -ffast-math \
	      -std=c++0x -Wall \
	      -Wno-unused-function  -Wno-unused-variable -Wno-unused-but-set-variable \
	      -Wno-write-strings

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
