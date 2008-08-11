# MZX makefile

OBJS      = main.o graphics.o window.o hexchar.o event.o         \
            error.o helpsys.o world.o board.o robot.o idput.o    \
            intake.o sfx.o scrdisp.o data.o game.o counter.o     \
            idarray.o delay.o game2.o expr.o sprite.o runrobo2.o \
            mzm.o decrypt.o audio.o edit.o edit_di.o block.o     \
            char_ed.o pal_ed.o param.o sfx_edit.o fill.o rasm.o  \
            robo_ed.o configure.o

PREFIX    = /usr

BIN       = mzx280c.exe

CC        = gcc
CPP       = g++
STRIP     = strip
CFLAGS    = -mconsole -O2 -funsigned-char -ffast-math
INCLUDES  = -I$(PREFIX)/include -I$(PREFIX)/include/SDL

LIBS      = -L$(PREFIX)/lib -lmingw32 -lSDLmain -lSDL -lmodplug -lgdm2s3m

.SUFFIXES: .cpp

%.o: %.cpp
	${CPP} ${CFLAGS} ${INCLUDES} -c $<

all: mzx

mzx:	${OBJS}
	${CPP} ${OBJS} ${LIBS} -o ${BIN}
	${STRIP} --strip-all ${BIN}

clean:
	rm -f *.o ${BIN}

