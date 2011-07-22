##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system was recently re-designed to not use recursive
#       Makefiles. The rationale for this is documented here:
#                  http://aegis.sourceforge.net/auug97.pdf
##

#
# Remove all built-in rules.
#
.SUFFIXES:
ifeq ($(filter -r,$(MAKEFLAGS)),)
MAKEFLAGS += -r
endif

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
PEFIX   ?= true

CHMOD   ?= chmod
CP      ?= cp
HOST_CC ?= gcc
LN      ?= ln
MKDIR   ?= mkdir
MV      ?= mv
RM      ?= rm

SDL_CFLAGS  ?= `sdl-config --cflags`
SDL_LDFLAGS ?= `sdl-config --libs`

VORBIS_CFLAGS  ?= -I${PREFIX}/include -DOV_EXCLUDE_STATIC_CALLBACKS
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
LIBPNG_CFLAGS  ?= `libpng-config --cflags`
LIBPNG_LDFLAGS ?= `libpng-config --libs`
endif

PTHREAD_LDFLAGS ?= -lpthread

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
# Android headers are busted and we get too many warnings..
#
ifneq (${PLATFORM},android)
CFLAGS   += -Wundef -Wunused-macros
CXXFLAGS += -Wundef -Wunused-macros
endif

#
# Always generate debug information; this may end up being
# stripped (on embedded platforms) or objcopy'ed out.
#
CFLAGS   += -g -W -Wall -Wno-unused-parameter -std=gnu99
CFLAGS   += -Wdeclaration-after-statement ${ARCH_CFLAGS}
CXXFLAGS += -g -W -Wall -Wno-unused-parameter -std=gnu++98
CXXFLAGS += -fno-exceptions -fno-rtti ${ARCH_CXXFLAGS}
LDFLAGS  += ${ARCH_LDFLAGS}

ifeq (${shell ${CC} -dumpversion | cut -d. -f1},4)

ifeq (${DEBUG},1)
CFLAGS   += -fbounds-check
CXXFLAGS += -fbounds-check
endif

#
# We enable pedantic warnings here, but this ends up turning on some things
# we must disable by hand.
#
# Variadic macros are arguably less portable, but all the compilers we
# support have them.
#
# The "long long" type is only used in one platform's header files, and we
# don't use it at all in MegaZeux (even if we did it's quite portable).
#
ifneq (${PLATFORM},android)
CFLAGS   += -pedantic -Wno-variadic-macros -Wno-long-long
CXXFLAGS += -pedantic -fpermissive -Wno-variadic-macros -Wno-long-long
endif

ifneq (${PLATFORM},mingw)

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
ifneq (${PLATFORM},android)
CFLAGS   += -fstack-protector-all
CXXFLAGS += -fstack-protector-all
endif
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
PEFIX   := @${PEFIX}

CHMOD   := @${CHMOD}
CP      := @${CP}
HOST_CC := @${HOST_CC}
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
	@git checkout-index -a --prefix build/${TARGET}/
	${RM} -r build/${TARGET}/scripts
	${RM} build/${TARGET}/.gitignore build/${TARGET}/.gitattributes
	@cd build/${TARGET} && make distclean
	@tar -C build -Jcf build/dist/source/${TARGET}src.tar.xz ${TARGET}

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
	${PEFIX} $@
	${CHMOD} a-x $@
	$(if ${V},,@echo "  STRIP   " $<)
	${STRIP} $<
	$(if ${V},,@echo "  OBJCOPY " --add-gnu-debuglink $@ $<)
	${OBJCOPY} --add-gnu-debuglink=$@ $<
	${PEFIX} $<
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

${build}:
	${MKDIR} -p ${build}/docs
	${MKDIR} -p ${build}/assets
	${CP} config.txt ${build}
	${CP} assets/default.chr assets/edit.chr ${build}/assets
	${CP} assets/smzx.pal ${build}/assets
	${CP} docs/COPYING.DOC docs/changelog.txt docs/port.txt ${build}/docs
	${CP} docs/macro.txt docs/keycodes.png ${build}/docs
	${CP} docs/platform_matrix.html ${build}/docs
	${CP} ${mzxrun} ${build}
	@if test -f ${mzxrun}.debug; then \
		cp ${mzxrun}.debug ${build}; \
	fi
ifeq (${BUILD_EDITOR},1)
	${CP} assets/ascii.chr assets/blank.chr ${build}/assets
	${CP} assets/smzx.chr ${build}/assets
	${CP} ${mzx} ${build}
	@if test -f ${mzx}.debug; then \
		cp ${mzx}.debug ${build}; \
	fi
endif
ifeq (${BUILD_HELPSYS},1)
	${CP} assets/help.fil ${build}/assets
endif
ifeq (${BUILD_MODULAR},1)
	${CP} ${core_target} ${editor_target} ${build}
	@if test -f ${core_target}.debug; then \
		cp ${core_target}.debug ${build}; \
	fi
	@if test -f ${editor_target}.debug; then \
		cp ${editor_target}.debug ${build}; \
	fi
endif
ifeq (${BUILD_UTILS},1)
	${MKDIR} ${build}/utils
	${CP} ${checkres} ${downver} ${build}/utils
	${CP} ${hlp2txt} ${txt2hlp} ${build}/utils
	${CP} ${png2smzx} ${build}/utils
	@if test -f ${checkres}.debug; then \
		cp ${checkres}.debug ${downver}.debug ${build}/utils; \
		cp ${hlp2txt}.debug  ${txt2hlp}.debug ${build}/utils; \
		cp ${png2smzx}.debug ${build}/utils; \
	fi
endif
ifeq (${BUILD_RENDER_GL_PROGRAM},1)
	${MKDIR} -p ${build}/assets/shaders/extra
	${CP} assets/shaders/*.vert ${build}/assets/shaders
	${CP} assets/shaders/*.frag ${build}/assets/shaders
	${CP} assets/shaders/extra/*.frag assets/shaders/extra/README.txt \
	 	${build}/assets/shaders/extra
endif

distclean: clean
	@echo "  DISTCLEAN"
	@rm -f src/config.h
	@echo "PLATFORM=none" > platform.inc

assets/help.fil: ${txt2hlp} docs/WIPHelp.txt
	@src/utils/txt2hlp docs/WIPHelp.txt $@

help_check: ${hlp2txt} assets/help.fil
	@src/utils/hlp2txt assets/help.fil help.txt
	@diff -q docs/WIPHelp.txt help.txt
	@rm -f help.txt

endif
