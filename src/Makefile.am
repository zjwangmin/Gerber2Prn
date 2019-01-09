# what flags you want to pass to the C compiler & linker
#CFLAGS = -I.. -Wall -O2
AM_LDFLAGS = -L/usr/local/lib
AM_YFLAGS = -d
AM_LFLAGS = 
BUILT_SOURCES = gerber_bison.h


# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
bin_PROGRAMS = gerb2tiff
gerb2tiff_SOURCES = gerber_flex.ll gerber_bison.yy apertures.cpp main.cpp gerber.cpp polygon.cpp gerber.h polygon.h apertures.h
