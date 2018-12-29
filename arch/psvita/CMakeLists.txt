cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	if(DEFINED ENV{VITASDK})
		set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
	else()
		message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
	endif()
endif()

set(BIN_NAME megazeux)
set(DIST_NAME MegaZeux)
set(MZX_VER git)
set(PKG_NAME ${DIST_NAME}-${MZX_VER}.vpk)
project(${BIN_NAME})

include("${VITASDK}/share/vita.cmake" REQUIRED)

# The Vita SDK's include path priority prioritizes GCC's limits.h instead of the
# libc one for some reason, so let's set PATH_MAX manually to allow XMP to compile
# without further modification. Sigh.
add_definitions(-DPATH_MAX=1024 -DOV_EXCLUDE_STATIC_CALLBACKS)

set(VITA_APP_NAME ${DIST_NAME})
set(VITA_TITLEID "DMZX00001")
set(VITA_VERSION "02.91")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -DDEBUG -W -Wall -Wno-unused-parameter -Wno-format-truncation")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")

set(MZX_SRC ../../src)
set(XMP_SRC ../../contrib/libxmp/src)

include_directories(
	# MegaZeux Stuff
	.
	${MZX_SRC}

	# Libraries
	${XMP_SRC}/../include
	${XMP_SRC}

	# We're not using sdl-config to smooth things over for us so we need to
	# point directly to the SDL include directory. This should work as long
	# as SDL2 is installed via the Vita SDK package manager.
	${VITASDK}/arm-vita-eabi/include/SDL2
)

add_executable(${BIN_NAME}
	# Core
	${MZX_SRC}/block.c
	${MZX_SRC}/board.c
	${MZX_SRC}/caption.c
	${MZX_SRC}/configure.c
	${MZX_SRC}/core.c
	${MZX_SRC}/counter.c
	${MZX_SRC}/data.c
	${MZX_SRC}/error.c
	${MZX_SRC}/event.c
	${MZX_SRC}/expr.c
	${MZX_SRC}/fsafeopen.c
	${MZX_SRC}/game.c
	${MZX_SRC}/game_menu.c
	${MZX_SRC}/game_ops.c
	${MZX_SRC}/game_player.c
	${MZX_SRC}/game_update.c
	${MZX_SRC}/game_update_board.c
	${MZX_SRC}/graphics.c
	${MZX_SRC}/idarray.c
	${MZX_SRC}/idput.c
	${MZX_SRC}/intake.c
	${MZX_SRC}/intake_num.c
	${MZX_SRC}/legacy_rasm.c
	${MZX_SRC}/legacy_board.c
	${MZX_SRC}/legacy_robot.c
	${MZX_SRC}/legacy_world.c
	${MZX_SRC}/mzm.c
	${MZX_SRC}/render.c
	${MZX_SRC}/robot.c
	${MZX_SRC}/run_robot.c
	${MZX_SRC}/scrdisp.c
	${MZX_SRC}/settings.c
	${MZX_SRC}/sprite.c
	${MZX_SRC}/str.c
	${MZX_SRC}/util.c
	${MZX_SRC}/window.c
	${MZX_SRC}/world.c
	${MZX_SRC}/zip.c

	# Renderer
	${MZX_SRC}/render_layer.c
	${MZX_SRC}/render_soft.c

	# SDL2
	${MZX_SRC}/audio/audio_sdl.c
	${MZX_SRC}/event_sdl.c
	${MZX_SRC}/platform_sdl.c
	${MZX_SRC}/render_sdl.c

	# Audio
	${MZX_SRC}/audio/audio.c
	${MZX_SRC}/audio/audio_pcs.c
	${MZX_SRC}/audio/audio_wav.c
	${MZX_SRC}/audio/ext.c
	${MZX_SRC}/audio/sampled_stream.c
	${MZX_SRC}/audio/sfx.c

	# Vorbis
	${MZX_SRC}/audio/audio_vorbis.c

	# XMP
	${MZX_SRC}/audio/audio_xmp.c
	${XMP_SRC}/control.c
	${XMP_SRC}/dataio.c
	${XMP_SRC}/effects.c
	${XMP_SRC}/extras.c
	${XMP_SRC}/filter.c
	${XMP_SRC}/fnmatch.c
	${XMP_SRC}/format.c
	${XMP_SRC}/hio.c
	${XMP_SRC}/hmn_extras.c
	${XMP_SRC}/lfo.c
	${XMP_SRC}/load.c
	${XMP_SRC}/load_helpers.c
	${XMP_SRC}/med_extras.c
	${XMP_SRC}/md5.c
	${XMP_SRC}/memio.c
	${XMP_SRC}/mix_all.c
	${XMP_SRC}/mix_paula.c
	${XMP_SRC}/mixer.c
	${XMP_SRC}/period.c
	${XMP_SRC}/player.c
	${XMP_SRC}/read_event.c
	${XMP_SRC}/scan.c
	${XMP_SRC}/smix.c
	${XMP_SRC}/virtual.c
	${XMP_SRC}/loaders/669_load.c
	${XMP_SRC}/loaders/amf_load.c
	${XMP_SRC}/loaders/asif.c
	${XMP_SRC}/loaders/asylum_load.c
	${XMP_SRC}/loaders/common.c
	${XMP_SRC}/loaders/far_load.c
	${XMP_SRC}/loaders/flt_load.c
	${XMP_SRC}/loaders/gdm_load.c
	${XMP_SRC}/loaders/hmn_load.c
	${XMP_SRC}/loaders/iff.c
	${XMP_SRC}/loaders/it_load.c
	${XMP_SRC}/loaders/itsex.c
	${XMP_SRC}/loaders/med2_load.c
	${XMP_SRC}/loaders/med3_load.c
	${XMP_SRC}/loaders/med4_load.c
	${XMP_SRC}/loaders/mmd_common.c
	${XMP_SRC}/loaders/mmd1_load.c
	${XMP_SRC}/loaders/mmd3_load.c
	${XMP_SRC}/loaders/mod_load.c
	${XMP_SRC}/loaders/mtm_load.c
	${XMP_SRC}/loaders/okt_load.c
	${XMP_SRC}/loaders/sample.c
	${XMP_SRC}/loaders/st_load.c
	${XMP_SRC}/loaders/s3m_load.c
	${XMP_SRC}/loaders/stm_load.c
	${XMP_SRC}/loaders/ult_load.c
	${XMP_SRC}/loaders/voltable.c
	${XMP_SRC}/loaders/xm_load.c

	# main()
	../../src/main.c

	# Platform-specific
	linked_list.c
	vitaio.c
)

target_link_libraries(${BIN_NAME}
	SDL2
	vita2d
	vorbisfile
	vorbis
	ogg
	SceDisplay_stub
	SceCtrl_stub
	SceAudio_stub
	SceSysmodule_stub
	SceGxm_stub
	SceCommonDialog_stub
	SceTouch_stub
	SceHid_stub
	m
	z
)

vita_create_self(${BIN_NAME}.self ${BIN_NAME})
vita_create_vpk(${PKG_NAME} ${VITA_TITLEID} ${BIN_NAME}.self
	VERSION ${VITA_VERSION}
	NAME ${VITA_APP_NAME}
	FILE sce_sys/icon0.png sce_sys/icon0.png
	FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
	FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
	FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
