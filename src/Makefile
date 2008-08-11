#
# core MegaZeux makefile
#
include ../Makefile.platform
include ../Makefile.in

OBJS      = main.o graphics.o window.o hexchar.o event.o         \
            error.o helpsys.o world.o board.o robot.o idput.o    \
            intake.o sfx.o scrdisp.o data.o game.o counter.o     \
            idarray.o delay.o game2.o expr.o sprite.o runrobo2.o \
            mzm.o decrypt.o audio.o edit.o edit_di.o block.o     \
            char_ed.o pal_ed.o param.o sfx_edit.o fill.o rasm.o  \
            robo_ed.o configure.o fsafeopen.o

MZX       = ../${TARGET}
            
all:	mzx

mzx:	${OBJS}
	${CPP} ${OBJS} ${LIBS} -o ${MZX}${BINEXT}
	${STRIP} ${MZX}${BINEXT}

clean:
	rm -f *.o *~ ${MZX}${BINEXT}
