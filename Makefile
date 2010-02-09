##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

.PHONY: clean package_clean help_check

include platform.inc
include version.inc

all: mzx utils

include arch/${PLATFORM}/Makefile.in

CC      ?= gcc
CXX     ?= g++
AR      ?= ar
STRIP   ?= strip --strip-unneeded
OBJCOPY ?= objcopy
MKDIR   ?= mkdir
CP      ?= cp
RM      ?= rm

#
# We don't want these commands to be echo'ed in non-verbose mode
#
ifneq (${V},1)
override V:=
CC      := @${CC}
CXX     := @${CXX}
AR      := @${AR}
STRIP   := @${STRIP}
OBJCOPY := @${OBJCOPY}
MKDIR   := @${MKDIR}
CP      := @${CP}
RM      := @${RM}
endif

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

OPTIMIZE_CFLAGS ?= -O2

ifeq (${DEBUG},1)
#
# Disable the optimizer for "true" debug builds
#
CFLAGS   = -O0 -DDEBUG
CXXFLAGS = -O0 -DDEBUG
else
#
# Optimized builds have assert() compiled out
#
CFLAGS   += ${OPTIMIZE_CFLAGS} -DNDEBUG
CXXFLAGS += ${OPTIMIZE_CFLAGS} -DNDEBUG
endif

#
# Always generate debug information; this may end up being stripped
# stripped (on embedded platforms) or objcopy'ed out.
#
CFLAGS   += -g -Wall -std=gnu99 ${ARCH_CFLAGS}
CXXFLAGS += -g -Wall ${ARCH_CXXFLAGS}

#
# The SUPPRESS_BUILD hack is required to allow the placebo "dist"
# Makefile to provide an 'all:' target, which allows it to print
# a message. We don't want to pull in other targets, confusing Make.
#
ifneq (${SUPPRESS_BUILD},1)

mzxrun = mzxrun${BINEXT}
mzx = megazeux${BINEXT}

mzx: ${mzxrun}.debug ${mzx}.debug

ifeq (${BUILD_MODPLUG},1)
BUILD_GDM2S3M=1
endif

%/.build:
	$(if ${V},,@echo "  MKDIR   " $@)
	${MKDIR} $@

%.debug: %
	$(if ${V},,@echo "  OBJCOPY " --only-keep-debug $< $@)
	${OBJCOPY} --only-keep-debug $< $@
	@chmod a-x $@
	$(if ${V},,@echo "  STRIP   " $<)
	${STRIP} $<
	$(if ${V},,@echo "  OBJCOPY " --add-gnu-debuglink $@ $<)
	${OBJCOPY} --add-gnu-debuglink=$@ $<
	@touch $@

include src/Makefile.in
include src/utils/Makefile.in
include src/network/Makefile.in

package_clean: utils_package_clean
	-@mv ${mzxrun}       ${mzxrun}.backup
	-@mv ${mzxrun}.debug ${mzxrun}.debug.backup
ifeq (${BUILD_EDITOR},1)
	-@mv ${mzx}          ${mzx}.backup
	-@mv ${mzx}.debug    ${mzx}.debug.backup
endif
ifeq (${BUILD_MODULAR},1)
	-@mv ${core_target}         ${core_target}.backup
	-@mv ${core_target}.debug   ${core_target}.debug.backup
	-@mv ${editor_target}       ${editor_target}.backup
	-@mv ${editor_target}.debug ${editor_target}.debug.backup
endif
	@${MAKE} mzx_clean
	@rm -f src/config.h
	@echo "PLATFORM=none" > platform.inc
ifeq (${BUILD_MODULAR},1)
	-@mv ${core_target}.backup         ${core_target}
	-@mv ${core_target}.debug.backup   ${core_target}.debug
	-@mv ${editor_target}.backup       ${editor_target}
	-@mv ${editor_target}.debug.backup ${editor_target}.debug
endif
	-@mv ${mzxrun}.backup       ${mzxrun}
	-@mv ${mzxrun}.debug.backup ${mzxrun}.debug
ifeq (${BUILD_EDITOR},1)
	-@mv ${mzx}.backup          ${mzx}
	-@mv ${mzx}.debug.backup    ${mzx}.debug
endif

clean: mzx_clean utils_clean

distclean: clean
	@echo "  DISTCLEAN"
	@rm -f src/config.h
	@echo "PLATFORM=none" > platform.inc

mzx_help.fil: ${txt2hlp} docs/WIPHelp.txt
	@src/utils/txt2hlp docs/WIPHelp.txt $@

help_check: ${hlp2txt} mzx_help.fil
	@src/utils/hlp2txt mzx_help.fil help.txt
	@diff -q docs/WIPHelp.txt help.txt
	@rm -f help.txt

endif
