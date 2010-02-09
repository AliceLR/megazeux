##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

.PHONY: clean

VERSION=2.81h
TARGET=mzx281h

all: mzx

include Makefile.platform

CC  ?= gcc
CXX ?= g++
AR  ?= ar

#
# This hack should go away once the DS build can be created without DEBUG=1
#
ifneq (${BUILD_NDS},1)
ifeq (${DEBUG},1)
OPTIMIZE_CFLAGS = -O0
endif
endif

OPTIMIZE_CFLAGS ?= -O2

SDL_CFLAGS  ?= `sdl-config --cflags`
SDL_LDFLAGS ?= `sdl-config --libs`

VORBIS_CFLAGS  ?= -I${PREFIX}/include
ifneq (${TREMOR},1)
VORBIS_LDFLAGS ?= -L${PREFIX}/lib -lvorbisfile -lvorbis -logg
else
VORBIS_LDFLAGS ?= -L${PREFIX}/lib -lvorbisidec
endif

MIKMOD_LDFLAGS ?= -L${PREFIX} -lmikmod

ifeq (${LIBPNG},1)
LIBPNG_CFLAGS ?= `libpng12-config --cflags`
LIBPNG_LDFLAGS ?= `libpng12-config --libs`
endif

ifeq (${DEBUG},1)
CFLAGS    = ${OPTIMIZE_CFLAGS} -g -Wall -std=gnu99 -DDEBUG ${ARCH_CFLAGS}
CXXFLAGS  = ${OPTIMIZE_CFLAGS} -g -Wall -DDEBUG ${ARCH_CXXFLAGS}
a         = dbg.a
o         = dbg.o
else
CFLAGS   += ${OPTIMIZE_CFLAGS} -Wall -std=gnu99 ${ARCH_CFLAGS}
CXXFLAGS += ${OPTIMIZE_CFLAGS} -Wall ${ARCH_CXXFLAGS}
a         = a
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

mzx: ${mzx} utils

ifeq (${BUILD_MODPLUG},1)
BUILD_GDM2S3M=1
endif

include src/utils/Makefile.in
include src/Makefile.in

clean: ${mzx}_clean utils_clean

distclean: clean
	@echo "  DISTCLEAN"
	@rm -f src/config.h
	@cp -f arch/Makefile.dist Makefile.platform

endif
