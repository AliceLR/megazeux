#
# core MegaZeux makefile
#
include ../Makefile.in
include ../Makefile.platform

# MZX requires some additional flags
CXXFLAGS += -Wno-char-subscripts
CXXFLAGS += -ffast-math -funsigned-char -DMZX_VERSION=\"${VERSTRING}\"
CFLAGS   += -std=gnu99 ${CXXFLAGS}

# standard MZX includes
INCLUDES += -I${PREFIX}/include -I../${MODPLUG} -I../${GDM2S3M} \
            -I../${MODPLUG}/libmodplug

OBJS      = main.${o} graphics.${o} window.${o} hexchar.${o} event.${o} \
            error.${o} helpsys.${o} world.${o} board.${o} robot.${o} \
            idput.${o} intake.${o} sfx.${o} scrdisp.${o} data.${o} game.${o} \
            counter.${o} idarray.${o} delay.${o} game2.${o} expr.${o} \
            sprite.${o} runrobo2.${o} mzm.${o} decrypt.${o} audio.${o} \
            edit.${o} edit_di.${o} block.${o} char_ed.${o} pal_ed.${o} \
            param.${o} sfx_edit.${o} fill.${o} rasm.${o} robo_ed.${o} \
            configure.${o} fsafeopen.${o} macro.${o}

MZX       = ../${TARGET}

ifdef DEBUG

subdir:
	cd utils && ${MAKE} utils DEBUG=1 && cd ../../..

subdir-clean:
	cd utils && ${MAKE} clean DEBUG=1 && cd ../../..

mzx:	subdir ${OBJS}
	${CXX} ${OBJS} ${LIBS} -o ${MZX}.dbg${BINEXT}
	@echo "DEBUG BUILD AVAILABLE AT ${MZX}.dbg${BINEXT}"

else

subdir:
	cd utils && ${MAKE} utils && cd ../../..

subdir-clean:
	cd utils && ${MAKE} clean && cd ../../..

mzx:	subdir ${OBJS}
	${CXX} ${OBJS} ${LIBS} -o ${MZX}${BINEXT}
	${STRIP} ${MZX}${BINEXT} 

endif

clean: subdir-clean
	rm -f ${OBJS} *~ ${MZX}${BINEXT} ${MZX}.dbg${BINEXT} 
