##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

include Makefile.platform

.PHONY: clean

##
# Version info
##

VERSION=2.81g
TARGET=mzx281g

##
# Global definitions
##

CC  ?= gcc
CXX ?= g++
AR  ?= ar

# base includes for SDL, override as necessary
INCLUDES ?= `sdl-config --cflags`
LIBS     ?= `sdl-config --libs`

ifneq (${DATE},0)
VERSTRING = ${VERSION}\ \(`date +%Y%m%d`\)
else
VERSTRING = ${VERSION}
endif

ifeq (${DEBUG},1)
CFLAGS    = -g -Wall -DDEBUG
CXXFLAGS  = -g -Wall -DDEBUG
o         = dbg.o
else
CFLAGS   += -O2 -Wall
CXXFLAGS += -O2 -Wall
o         = o
endif

##
# Magical targets
##

.SUFFIXES: .cpp .c

%.o %.dbg.o: %.cpp
ifeq (${V},1)
	${CXX} ${CXXFLAGS} ${INCLUDES} -c $< -o $@
else
	@echo "  CXX     " $<
	@${CXX} ${CXXFLAGS} ${INCLUDES} -c $< -o $@
endif

%.o %.dbg.o: %.c
ifeq (${V},1)
	${CC} ${CFLAGS} ${INCLUDES} -c $< -o $@
else
	@echo "  CC      " $<
	@${CC} ${CFLAGS} ${INCLUDES} -c $< -o $@
endif

##
# Fragment includes, and most of the work
##

ifneq (${SUPPRESS_BUILD},1)

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
