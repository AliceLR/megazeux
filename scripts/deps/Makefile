#
# MegaZeux common dependency builder code.
#

ifeq (${PLATFORM},)
$(error usage: make PLATFORM=<platform> [ARCH=<arch>] [all|<arch>|package|clean])
endif

VERSION		= 2.93b-r0
DIST		= megazeux-dependencies-${VERSION}
TARGET		= TARGET

ZLIB		= zlib-1.3.1
PNG		= libpng-1.6.43
OGG		= libogg-1.3.5
VORBIS		= libvorbis-1.3.7
TREMOR		= tremor
SDL12		= SDL-1.2.15
SDL2		= SDL2-2.28.5
#SDL3		= SDL3-3.2.0

ZLIB_URL	= https://www.zlib.net/${ZLIB}.tar.gz
PNG_URL		= http://download.sourceforge.net/libpng/${PNG}.tar.gz
OGG_URL		= https://downloads.xiph.org/releases/ogg/${OGG}.tar.gz
VORBIS_URL	= https://downloads.xiph.org/releases/vorbis/${VORBIS}.tar.gz
SDL12_URL	= https://www.libsdl.org/release/${SDL12}.tar.gz
SDL2_URL	= https://www.libsdl.org/release/${SDL2}.tar.gz
#SDL3_URL	= https://www.libsdl.org/release/${SDL3}.tar.gz

# xiph still hasn't fixed the crash bugs in tremor yet
#TREMOR_GIT	= https://gitlab.xiph.org/xiph/tremor
TREMOR_GIT	= https://github.com/sezero/tremor

LIB		= ${PLATFORM}/${ARCH}/lib
BIN		= ${PLATFORM}/${ARCH}/bin
INCLUDE		= ${PLATFORM}/${ARCH}/include
ZLIB_TARGET	= ${LIB}/libz.a
PNG_TARGET	= ${LIB}/libpng16.a
OGG_TARGET	= ${LIB}/libogg.a
VORBIS_TARGET	= ${LIB}/libvorbis.a
TREMOR_TARGET	= ${LIB}/libvorbisidec.a
SDL12_TARGET	= ${LIB}/libSDL.a
SDL2_TARGET	= ${LIB}/libSDL2.a
#SDL3_TARGET	= ${LIB}/libSDL3.a
TARGETS		= ${ZLIB_TARGET} \
		  ${PNG_TARGET} \
		  ${OGG_TARGET} \
		  ${VORBIS_TARGET} \
		  ${SDL12_TARGET} \
		  ${SDL2_TARGET} \

all:

-include Makefile.${PLATFORM}.in

ZLIB_PACKAGE	= ${TARGET}/${LIB}/libz.a \
		  ${TARGET}/${INCLUDE}/zlib.h \
		  ${TARGET}/${INCLUDE}/zconf.h
PNG_PACKAGE	= ${TARGET}/${LIB}/libpng16.a \
		  ${TARGET}/${INCLUDE}/libpng16 \
		  ${TARGET}/${BIN}/libpng-config
VORBIS_PACKAGE	= ${TARGET}/${LIB}/libogg.a \
		  ${TARGET}/${LIB}/libvorbis.a \
		  ${TARGET}/${LIB}/libvorbisfile.a \
		  ${TARGET}/${INCLUDE}/ogg \
		  ${TARGET}/${INCLUDE}/vorbis
TREMOR_PACKAGE	= ${TARGET}/${LIB}/libvorbisidec.a \
		  ${TARGET}/${INCLUDE}/tremor
ifneq (${TREMOR_LOWMEM},1)
TREMOR_PACKAGE	+= \
		  ${TARGET}/${LIB}/libogg.a \
		  ${TARGET}/${INCLUDE}/ogg
endif
SDL12_PACKAGE	= ${TARGET}/${LIB}/libSDL.a \
		  ${TARGET}/${INCLUDE}/SDL \
		  ${TARGET}/${BIN}/sdl-config
SDL2_PACKAGE	= ${TARGET}/${LIB}/libSDL2.a \
		  ${TARGET}/${INCLUDE}/SDL2 \
		  ${TARGET}/${BIN}/sdl2-config
SDL3_PACKAGE	= ${TARGET}/${LIB}/libSDL3.a \
		  ${TARGET}/${INCLUDE}/SDL3

.PHONY: all
.PHONY: package package_dir package_clean zlib_package png_package vorbis_package tremor_package
.PHONY: SDL12_package SDL2_package SDL3_package
.PHONY: clean zlib_clean png_clean ogg_clean vorbis_clean tremor_clean
.PHONY: SDL12_clean SDL2_clean SDL3_clean

.NOTPARALLEL:
.SUFFIXES:

MAKEFLAGS += --no-builtin-rules
MAKEFLAGS += --no-builtin-variables

#
# Initial setup rules.
#

build:
	mkdir -p build/cmake

build/${ZLIB}.tar.gz: | build
	cd build && \
	wget '${ZLIB_URL}'

build/${PNG}.tar.gz: | build
	cd build && \
	wget '${PNG_URL}'

build/${OGG}.tar.gz: | build
	cd build && \
	wget '${OGG_URL}'

build/${VORBIS}.tar.gz: | build
	cd build && \
	wget '${VORBIS_URL}'

build/${SDL12}.tar.gz: | build
	cd build && \
	wget '${SDL12_URL}'

build/${SDL2}.tar.gz: | build
	cd build && \
	wget '${SDL2_URL}'

#build/${SDL3}.tar.gz: | build
#	cd build && \
#	wget '${SDL3_URL}'

build/${ZLIB}: build/${ZLIB}.tar.gz
	cd build && \
	tar -xzf '${ZLIB}.tar.gz'

build/${PNG}: build/${PNG}.tar.gz | build
	cd build && \
	tar -xzf '${PNG}.tar.gz'

build/${OGG}: build/${OGG}.tar.gz | build
	cd build && \
	tar -xzf '${OGG}.tar.gz'

build/${VORBIS}: build/${VORBIS}.tar.gz | build
	cd build && \
	tar -xzf '${VORBIS}.tar.gz'

build/${TREMOR}: | build
	cd build && \
	git clone "${TREMOR_GIT}"

build/${SDL12}: build/${SDL12}.tar.gz | build
	cd build && \
	tar -xzf '${SDL12}.tar.gz'

build/${SDL2}: build/${SDL2}.tar.gz | build
	cd build && \
	tar -xzf '${SDL2}.tar.gz'

#build/${SDL3}: build/${SDL3}.tar.gz | build
#	cd build && \
#	tar -xzf '${SDL3}.tar.gz'

zlib_clean:
	cd build/${ZLIB}; make distclean || true; make -fwin32/Makefile.gcc clean || true

png_clean:
	rm -rf build/cmake/${PNG}
	cd build/${PNG}; make distclean || true

ogg_clean:
	rm -rf build/cmake/${OGG}
	cd build/${OGG}; make distclean || true

vorbis_clean:
	rm -rf build/cmake/${VORBIS}
	cd build/${VORBIS}; make distclean || true

tremor_clean:
	cd build/${TREMOR}; make distclean || true

SDL12_clean:
	cd build/${SDL12}; make distclean || true

SDL2_clean:
	rm -rf build/cmake/${SDL2}
	cd build/${SDL2}; make distclean || true

#SDL3_clean:
#	rm -rf build/cmake/${SDL3}

ifeq (${ARCH},)
#
# Architecture meta targets.
#

all: ${ALL_ARCH}

${ALL_ARCH}:
	${MAKE} ARCH='$@'

clean: zlib_clean png_clean ogg_clean vorbis_clean SDL12_clean SDL2_clean

distclean:
	rm -rf ${TARGET}
	rm -rf build
	rm -rf djgpp
	rm -rf macos
	rm -rf mingw

package:
	for ARCH in ${ALL_ARCH}; do \
		${MAKE} PLATFORM='${PLATFORM}' ARCH="$$ARCH" package_clean; \
		${MAKE} PLATFORM='${PLATFORM}' ARCH="$$ARCH" package; \
	done
	A=''; \
	for ARCH in ${ALL_ARCH}; do \
		A="$$A ${PLATFORM}/$$ARCH"; \
	done; \
	cd ${TARGET}; \
	XZ_OPT="-T0 -9" tar -cJf '../${DIST}-${PLATFORM}.tar.xz' ${TAR_FLAGS} $$A

else

#
# Per-architecture targets.
#
# For libpng, libogg, and libvorbis, an explicit --build value is
# required to prevent Autoconf's broken interaction with binfmt_misc
# when autodetecting cross compilation. This doesn't work with SDL.
#

CONFIGURE_ARGS	= --prefix='${PREFIX}'
ifneq (${HOST},)
ZLIB_CONFIGURE	= CHOST='${HOST}'
CONFIGURE_ARGS	+= --build=$$(./config.guess) --host='${HOST}'
endif
CONFIGURE_ARGS	+= ${ARCH_CONFIGURE_ARGS}

PREFIX		:= $(shell pwd)/${PLATFORM}/${ARCH}
CONFIGURE	?= ./configure
CMAKE		?= cmake

CONFIGURE	+= ${CONFIGURE_ARGS}
CMAKE		+= -S "$$SRCDIR" -B . -D CMAKE_INSTALL_PREFIX='${PREFIX}' ${ARCH_CMAKE_ARGS}

ifeq (${ARCH_INIT},)
$(error must define ARCH_INIT.)
endif

.PHONY: zlib png ogg vorbis tremor SDL12 SDL2 SDL3

zlib: ${ZLIB_TARGET}
png: ${PNG_TARGET}
ogg: ${OGG_TARGET}
vorbis: ${VORBIS_TARGET}
tremor: ${TREMOR_TARGET}
SDL12: ${SDL12_TARGET}
SDL2: ${SDL2_TARGET}
#SDL3: ${SDL3_TARGET}

# zlib has its own buildsystem.
# MinGW requires some involved special handling...
${ZLIB_TARGET}: | build/${ZLIB} zlib_clean
ifeq (${PLATFORM},mingw)
	cd build/${ZLIB}; \
	${ARCH_INIT}; \
	export DESTDIR='${PREFIX}/'; \
	export INCLUDE_PATH=include; \
	export LIBRARY_PATH=lib; \
	export BINARY_PATH=bin; \
	${MAKE} -fwin32/Makefile.gcc PREFIX='${HOST}-' prefix='${PREFIX}'; \
	${MAKE} -fwin32/Makefile.gcc PREFIX='${HOST}-' prefix='${PREFIX}' install SHARED_MODE=1;
else
	cd build/${ZLIB}; \
	${ARCH_INIT}; \
	${ZLIB_CONFIGURE} ./configure --prefix='${PREFIX}'; \
	${MAKE}; \
	${MAKE} install;
endif

# libpng supports Autotools and CMake.
# Its ./configure is barely capable of finding the correct zlib at the
# best of times and doesn't use pkg-config.
${PNG_TARGET}: | build/${PNG} png_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${PNG}
	cd build/cmake/${PNG}; \
	export SRCDIR='../../${PNG}'; \
	${ARCH_INIT}; \
	${CMAKE}; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${PNG}; \
	${ARCH_INIT}; \
	CFLAGS="$$CFLAGS -I${PREFIX}/include" \
	CPPFLAGS="$$CPPFLAGS -I${PREFIX}/include" \
	LDFLAGS="$$LDFLAGS -L${PREFIX}/lib -lz" \
	${CONFIGURE}; \
	${MAKE}; \
	${MAKE} install;
endif

# libogg supports Autotools and CMake.
# The Xiph CMake buildsystems can't build static and shared at the same time.
${OGG_TARGET}: | build/${OGG} ogg_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${OGG}
	cd build/cmake/${OGG}; \
	export SRCDIR='../../${OGG}'; \
	${ARCH_INIT}; \
	${CMAKE} -D BUILD_SHARED_LIBS=ON; \
	${MAKE}; \
	${MAKE} install; \
	${CMAKE} -D BUILD_SHARED_LIBS=OFF; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${OGG}; \
	${ARCH_INIT}; \
	${CONFIGURE}; \
	${MAKE}; \
	${MAKE} install;
endif

# libvorbis supports Autotools and CMake.
# The Xiph CMake buildsystems can't build static and shared at the same time.
${VORBIS_TARGET}: | build/${VORBIS} vorbis_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${VORBIS}
	cd build/cmake/${VORBIS}; \
	export SRCDIR='../../${VORBIS}'; \
	${ARCH_INIT}; \
	${CMAKE} -D BUILD_SHARED_LIBS=ON; \
	${MAKE}; \
	${MAKE} install; \
	${CMAKE} -D BUILD_SHARED_LIBS=OFF; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${VORBIS}; \
	${ARCH_INIT}; \
	${CONFIGURE}; \
	${MAKE}; \
	${MAKE} install;
endif

# libvorbisidec supports Autotools.
# The lowmem branch can be selected optionally.
${TREMOR_TARGET}: | build/${TREMOR} tremor_clean
ifeq (${TREMOR_LOWMEM},1)
	cd build/${TREMOR}; \
	git checkout lowmem
else
	cd build/${TREMOR}; \
	git checkout master
endif
	cd build/${TREMOR}; \
	${ARCH_INIT}; \
	./autogen.sh ${CONFIGURE_ARGS}; \
	${MAKE}; \
	${MAKE} install;

# SDL1.2 only supports Autotools.
# Rerun autogen.sh to fix ./configure's --build/--host cross compilation detection.
${SDL12_TARGET}: | build/${SDL12} SDL12_clean
	cd build/${SDL12}; \
	${ARCH_INIT}; \
	./autogen.sh; \
	ln -sf build-scripts/config.guess config.guess; \
	${CONFIGURE}; \
	${MAKE}; \
	${MAKE} install;

# SDL2 supports Autotools and CMake. Early versions have a broken CMakeFiles.txt.
${SDL2_TARGET}: | build/${SDL2} SDL2_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${SDL2}
	cd build/cmake/${SDL2}; \
	export SRCDIR='../../${SDL2}'; \
	${ARCH_INIT}; \
	${CMAKE}; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${SDL2}; \
	${ARCH_INIT}; \
	ln -sf build-scripts/config.guess config.guess; \
	${CONFIGURE} ${ARCH_CONFIGURE_SDL2_ARGS}; \
	${MAKE}; \
	${MAKE} install;
endif

# SDL3 only supports CMake.
#${SDL3_TARGET}: | build/${SDL3} sdl3_clean
#	mkdir -p build/cmake/${SDL3}
#	cd build/cmake/${SDL3}; \
#	export SRCDIR='../../${SDL3}'; \
#	${ARCH_INIT}; \
#	${CMAKE}; \
#	${MAKE}; \
#	${MAKE} install;

#
# Packaging rules.
#

package_clean:
	rm -rf ${TARGET}/${LIB}
	rm -rf ${TARGET}/${INCLUDE}
	rm -rf ${TARGET}/${BIN}

package_dir:
	mkdir -p ${TARGET}/${LIB}
	mkdir -p ${TARGET}/${INCLUDE}
	mkdir -p ${TARGET}/${BIN}

%.debug: % | package_dir
	${HOST}-objcopy --only-keep-debug $< $@
	${HOST}-strip --strip-unneeded $<
	${HOST}-objcopy --add-gnu-debuglink=$@ $<

${TARGET}/%: % | package_dir
	if [ -d "$<" ]; then \
		cp -r $< $@; \
	else \
		cp -L $< $@; \
	fi

package: ${TARGET}/${LIB}/pkgconfig | package_dir

zlib_package: ${ZLIB_PACKAGE}
png_package: ${PNG_PACKAGE}
vorbis_package: ${VORBIS_PACKAGE}
tremor_package: ${TREMOR_PACKAGE}
SDL12_package: ${SDL12_PACKAGE}
SDL2_package: ${SDL2_PACKAGE}
SDL3_package: ${SDL3_PACKAGE}

endif # ARCH != ''
