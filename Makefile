##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

.PHONY: clean package_clean help_check mzx mzx.debug

-include platform.inc
include version.inc

all: mzx
debuglink: all mzx.debug

-include arch/${PLATFORM}/Makefile.in

CC      ?= gcc
CXX     ?= g++
AR      ?= ar
STRIP   ?= strip --strip-unneeded
OBJCOPY ?= objcopy

CHMOD   ?= chmod
CP      ?= cp
MKDIR   ?= mkdir
RM      ?= rm

SDL_CFLAGS  ?= `sdl-config --cflags`
SDL_LDFLAGS ?= `sdl-config --libs`

VORBIS_CFLAGS  ?= -I${PREFIX}/include
ifneq (${TREMOR},1)
VORBIS_LDFLAGS ?= -L${PREFIX}/lib -lvorbisfile -lvorbis -logg
else
VORBIS_LDFLAGS ?= -L${PREFIX}/lib -lvorbisidec
endif

MIKMOD_CFLAGS  ?= -I${PREFIX}/include
MIKMOD_LDFLAGS ?= -L${PREFIX}/lib -lmikmod

ZLIB_CFLAGS  ?= -I${PREFIX}/include
ZLIB_LDFLAGS ?= -L${PREFIX}/lib -lz

ifeq (${LIBPNG},1)
LIBPNG_CFLAGS  ?= `libpng12-config --cflags`
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
LDFLAGS  += ${ARCH_LDFLAGS}

ifeq (${shell ${CC} -dumpversion | cut -d. -f1},4)

#
# Symbols in COFF binaries are implicitly hidden unless exported; this
# flag just confuses GCC and must be disabled.
#
ifneq (${PLATFORM},mingw)
CFLAGS   += -fvisibility=hidden
CXXFLAGS += -fvisibility=hidden
endif

#
# Skip the stack protector on embedded platforms; it just unnecessarily
# slows things down, and there's no easy way to write a convincing
# __stack_chk_fail function.
#
ifeq ($(or ${BUILD_GP2X},${BUILD_NDS},${BUILD_PSP},${BUILD_WII}),)
CFLAGS   += -fstack-protector-all
CXXFLAGS += -fstack-protector-all
ifeq (${PLATFORM},mingw)
ARCH_LDFLAGS += -lssp
endif
endif

endif

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

CHMOD   := @${CHMOD}
CP      := @${CP}
MKDIR   := @${MKDIR}
RM      := @${RM}
endif

#
# The SUPPRESS_BUILD hack is required to allow the placebo "dist"
# Makefile to provide an 'all:' target, which allows it to print
# a message. We don't want to pull in other targets, confusing Make.
#
ifneq (${SUPPRESS_BUILD},1)

mzxrun = mzxrun${BINEXT}
mzx = megazeux${BINEXT}

mzx: ${mzxrun} ${mzx}
mzx.debug: ${mzxrun}.debug ${mzx}.debug

ifeq (${BUILD_MODPLUG},1)
BUILD_GDM2S3M=1
endif

%/.build:
	$(if ${V},,@echo "  MKDIR   " $@)
	${MKDIR} $@

%.debug: %
	$(if ${V},,@echo "  OBJCOPY " --only-keep-debug $< $@)
	${OBJCOPY} --only-keep-debug $< $@
	${CHMOD} a-x $@
	$(if ${V},,@echo "  STRIP   " $<)
	${STRIP} $<
	$(if ${V},,@echo "  OBJCOPY " --add-gnu-debuglink $@ $<)
	${OBJCOPY} --add-gnu-debuglink=$@ $<
	@touch $@

include src/Makefile.in

clean: mzx_clean

package_clean:
	-@mv ${mzxrun}       ${mzxrun}.backup
	-@mv ${mzxrun}.debug ${mzxrun}.debug.backup
ifeq (${BUILD_EDITOR},1)
	-@mv ${mzx}          ${mzx}.backup
	-@mv ${mzx}.debug    ${mzx}.debug.backup
endif
ifeq (${BUILD_MODULAR},1)
	-@mv ${core_target}          ${core_target}.backup
	-@mv ${core_target}.debug    ${core_target}.debug.backup
	-@mv ${editor_target}        ${editor_target}.backup
	-@mv ${editor_target}.debug  ${editor_target}.debug.backup
ifeq (${BUILD_UPDATER},1)
	-@mv ${network_target}       ${network_target}.backup
	-@mv ${network_target}.debug ${network_target}.debug.backup
endif
endif
	-@${MAKE} mzx_clean
	@rm -f src/config.h
	@echo "PLATFORM=none" > platform.inc
ifeq (${BUILD_MODULAR},1)
	-@mv ${core_target}.backup          ${core_target}
	-@mv ${core_target}.debug.backup    ${core_target}.debug
	-@mv ${editor_target}.backup        ${editor_target}
	-@mv ${editor_target}.debug.backup  ${editor_target}.debug
ifeq (${BUILD_UPDATER},1)
	-@mv ${network_target}.backup       ${network_target}
	-@mv ${network_target}.debug.backup ${network_target}.debug
endif
endif
	-@mv ${mzxrun}.backup       ${mzxrun}
	-@mv ${mzxrun}.debug.backup ${mzxrun}.debug
ifeq (${BUILD_EDITOR},1)
	-@mv ${mzx}.backup          ${mzx}
	-@mv ${mzx}.debug.backup    ${mzx}.debug
endif

ifeq (${BUILD_UTILS},1)
include src/utils/Makefile.in
package_clean: utils_package_clean
debuglink: utils utils.debug
clean: utils_clean
all: utils
endif

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
