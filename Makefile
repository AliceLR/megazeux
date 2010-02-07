##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

.PHONY: clean

VERSION=2.81g
TARGET=mzx281g

all: mzx

include Makefile.platform

CC  ?= gcc
CXX ?= g++
AR  ?= ar

SDL_CFLAGS  ?= `sdl-config --cflags`
SDL_LDFLAGS ?= `sdl-config --libs`

VORBIS_CFLAGS  ?= -I${PREFIX}/include
VORBIS_LDFLAGS ?= -L${PREFIX}/lib -lvorbisfile -lvorbis -logg

ifeq (${DEBUG},1)
CFLAGS    = -g -Wall -std=gnu99 -DDEBUG
CXXFLAGS  = -g -Wall -DDEBUG
o         = dbg.o
else
CFLAGS   += -O2 -Wall -std=gnu99
CXXFLAGS += -O2 -Wall
o         = o
endif

#
# The SUPPRESS_BUILD hack is required to allow the placebo "dist"
# Makefile to provide an 'all:' target, which allows it to print
# a message. We don't want to pull in other targets, confusing Make.
#
ifneq (${SUPPRESS_BUILD},1)

ifneq (${DEBUG},1)
mzx = ${TARGET}${BINEXT}
else
mzx = ${TARGET}.dbg${BINEXT}
endif

mzx: ${mzx}

ifeq (${BUILD_MODPLUG},1)
BUILD_GDM2S3M=1
endif

include src/Makefile.in

clean: ${gdm2s3m}_clean ${libmodplug}_clean ${mzx}_clean

distclean: clean
	@echo "  DISTCLEAN"
	@rm -f src/config.h
	@cp -f arch/Makefile.dist Makefile.platform

endif
