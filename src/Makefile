#
# core MegaZeux makefile
#
include ../Makefile.in
include ../Makefile.platform

# MZX requires some additional flags
CXXFLAGS += -ffast-math -funsigned-char -DMZX_VERSION=\"${VERSTRING}\"

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

subdir:	mzx

ifdef DEBUG
mzx:	${OBJS}
	${CXX} ${OBJS} ${LIBS} -o ${MZX}.dbg${BINEXT}
	@echo "DEBUG BUILD AVAILABLE AT ${MZX}.dbg${BINEXT}"
else
mzx:	${OBJS}
	${CXX} ${OBJS} ${LIBS} -o ${MZX}${BINEXT}
	${STRIP} ${MZX}${BINEXT}
endif

clean: dclean
	rm -f *.o *~ ${MZX}${BINEXT}

dclean:
	rm -f *.dbg.o ${MZX}.dbg${BINEXT}
