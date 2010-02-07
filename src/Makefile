#
# core MegaZeux makefile
#
include ../Makefile.in
include ../Makefile.platform

# MZX requires some additional flags
CFLAGS += -W -Wno-unused-parameter
CFLAGS += -ffast-math -funsigned-char -DMZX_VERSION=\"${VERSTRING}\"
CXXFLAGS = ${CFLAGS}

# standard MZX includes
INCLUDES += -I${PREFIX}/include
MZX       = ../${TARGET}

# always built in
OBJS      = main.${o} graphics.${o} window.${o} hexchar.${o} event.${o} \
            error.${o} helpsys.${o} world.${o} board.${o} robot.${o} \
            idput.${o} intake.${o} sfx.${o} scrdisp.${o} data.${o} game.${o} \
            counter.${o} idarray.${o} delay.${o} game2.${o} expr.${o} \
            sprite.${o} runrobo2.${o} mzm.${o} decrypt.${o} audio.${o} \
            edit.${o} edit_di.${o} block.${o} char_ed.${o} pal_ed.${o} \
            param.${o} sfx_edit.${o} fill.${o} rasm.${o} robo_ed.${o} \
            configure.${o} fsafeopen.${o} macro.${o}

# only built in if we picked modplug
ifeq (${BUILD_MODPLUG},1)
INCLUDES += -I../${GDM2S3M} -I../${MODPLUG} -I../${MODPLUG}/libmodplug
LIBS += ../${GDM2S3M}/libgdm2s3m.a ../${MODPLUG}/libmodplug.a -lstdc++
OBJS += audio_modplug.${o}
endif

# only built in if we picked mikmod
ifeq (${BUILD_MIKMOD},1)
LIBS += -L${PREFIX} -lmikmod
OBJS += audio_mikmod.${o}
endif

# only built in on Win32/MinGW32
ifdef ICON
OBJS += ../contrib/icons/quantump.${o}
endif

ifdef DEBUG

subdir:
	cd utils && ${MAKE} utils DEBUG=1 && cd ../../..

subdir-clean:
	cd utils && ${MAKE} clean DEBUG=1 && cd ../../..

mzx:	subdir ${OBJS}
ifdef V
	${CC} ${OBJS} ${LIBS} -o ${MZX}.dbg${BINEXT}
else
	@echo "  LINK    " ${MZX}.dbg${BINEXT}
	@${CC} ${OBJS} ${LIBS} -o ${MZX}.dbg${BINEXT}
endif
	@echo "DEBUG BUILD AVAILABLE AT ${MZX}.dbg${BINEXT}"

else

subdir:
	cd utils && ${MAKE} utils && cd ../../..

subdir-clean:
	cd utils && ${MAKE} clean && cd ../../..

mzx:	subdir ${OBJS}
ifdef V
	${CC} ${OBJS} ${LIBS} -o ${MZX}${BINEXT}
else
	@echo "  LINK    " ${MZX}${BINEXT}
	@${CC} ${OBJS} ${LIBS} -o ${MZX}${BINEXT}
endif
ifdef V
	${STRIP} ${MZX}${BINEXT}
else
	@echo "  STRIP   " ${MZX}${BINEXT}
	@${STRIP} ${MZX}${BINEXT}
endif
endif

clean: subdir-clean
	rm -f ${OBJS} *~ ${MZX}${BINEXT} ${MZX}.dbg${BINEXT}
