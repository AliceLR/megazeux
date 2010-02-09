##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

.PHONY: clean help_check mzx mzx.debug build build_clean source

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
LN      ?= ln
MKDIR   ?= mkdir
MV      ?= mv
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

ifneq (${PLATFORM},mingw)
ifeq (${shell ${CC} -dumpversion | cut -d. -f1},4)

#
# Symbols in COFF binaries are implicitly hidden unless exported; this
# flag just confuses GCC and must be disabled.
#
CFLAGS   += -fvisibility=hidden
CXXFLAGS += -fvisibility=hidden

#
# Skip the stack protector on embedded platforms; it just unnecessarily
# slows things down, and there's no easy way to write a convincing
# __stack_chk_fail function.
#
ifeq ($(or ${BUILD_GP2X},${BUILD_NDS},${BUILD_PSP},${BUILD_WII}),)
CFLAGS   += -fstack-protector-all
CXXFLAGS += -fstack-protector-all
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
LN      := @${LN}
MKDIR   := @${MKDIR}
MV      := @${MV}
RM      := @${RM}
endif

build_clean:
	$(if ${V},,@echo "  RM      " build)
	${RM} -r build

source: build/${TARGET}src

build/${TARGET}src:
	${RM} -r build/${TARGET}
	${MKDIR} -p build/dist/source
	@svn export . build/${TARGET}
	@cd build/${TARGET} && make distclean
	@tar -C build -jcvf build/dist/source/${TARGET}src.tar.bz2 ${TARGET}

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

ifeq (${BUILD_UTILS},1)
include src/utils/Makefile.in
debuglink: utils utils.debug
clean: utils_clean
all: utils
endif

ifeq (${build},)
build := build/${SUBPLATFORM}
endif

build: ${build}

${build}: all
	${MKDIR} -p ${build}
	${MKDIR} -p ${build}/docs
	${CP} config.txt mzx_ascii.chr mzx_blank.chr mzx_default.chr ${build}
	${CP} mzx_edit.chr mzx_smzx.chr smzx.pal ${build}
	${CP} docs/COPYING.DOC docs/changelog.txt ${build}/docs
	${CP} docs/port.txt docs/macro.txt ${build}/docs
	${CP} ${mzxrun} ${build}
	@if test -f ${mzxrun}.debug; then \
		${CP} ${mzxrun}.debug ${build}; \
	fi
ifeq (${BUILD_EDITOR},1)
	${CP} ${mzx} ${build}
	@if test -f ${mzx}.debug; then \
		${CP} ${mzx}.debug ${build}; \
	fi
endif
ifeq (${BUILD_HELPSYS},1)
	${CP} mzx_help.fil ${build}
endif
ifeq (${BUILD_MODULAR},1)
	${CP} ${core_target} ${editor_target} ${build}
	@if test -f ${core_target}.debug; then \
		${CP} ${core_target}.debug ${build}; \
	fi
	@if test -f ${editor_target}.debug; then \
		${CP} ${editor_target}.debug ${build}; \
	fi
ifeq (${BUILD_UPDATER},1)
	${CP} ${network_target} ${build}
	@if test -f ${network_target}.debug; then \
		${CP} ${network_target}.debug ${build}; \
	fi
endif
endif
ifeq (${BUILD_UTILS},1)
	${MKDIR} ${build}/utils
	${CP} ${checkres} ${downver} ${build}/utils
	${CP} ${hlp2txt} ${txt2hlp} ${build}/utils
	@if test -f ${checkres}.debug; then \
		${CP} ${checkres}.debug ${downver}.debug ${build}/utils; \
		${CP} ${hlp2txt}.debug  ${txt2hlp}.debug ${build}/utils; \
	fi
endif
ifeq (${BUILD_RENDER_GLSL},1)
	${MKDIR} ${build}/shaders
	${CP} shaders/*.vert shaders/*.frag ${build}/shaders
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
