#
# txt2hlp Makefile
#
include ../../Makefile.in
include ../../Makefile.platform

# txt2help requires some additional flags, but no SDL
CFLAGS   += -std=c99 -ffast-math -funsigned-char
INCLUDES =

TXT2HLPOBJS = txt2hlp.${o}
TXT2HLP     = txt2hlp

ifdef DEBUG
utils:	${TXT2HLPOBJS}
ifdef V
	${CC} ${TXT2HLPOBJS} -o ${TXT2HLP}.dbg${BINEXT}
else
	@echo "  LINK    " ${TXT2HLP}.dbg${BINEXT}
	@${CC} ${TXT2HLPOBJS} -o ${TXT2HLP}.dbg${BINEXT}
endif
else
utils:	${TXT2HLPOBJS}
ifdef V
	${CC} ${TXT2HLPOBJS} -o ${TXT2HLP}${BINEXT}
else
	@echo "  LINK    " ${TXT2HLP}${BINEXT}
	@${CC} ${TXT2HLPOBJS} -o ${TXT2HLP}${BINEXT}
endif
ifdef V
	${STRIP} ${TXT2HLP}${BINEXT}
else
	@echo "  STRIP   " ${TXT2HLP}${BINEXT}
	@${STRIP} ${TXT2HLP}${BINEXT}
endif
endif

clean:
	rm -f ${TXT2HLPOBJS} *~ ${TXT2HLP}${BINEXT} ${TXT2HLP}.dbg${BINEXT}
