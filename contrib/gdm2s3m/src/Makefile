#
# src/ makefile
#

include ../../../Makefile.in
include ../../../Makefile.platform

# must be build with c99
CFLAGS += -std=c99

#
# objects to build
#

objs  = error.${o} gdm.${o} s3m.${o} utility.${o} gdm2s3m.${o}

libs:	${objs}
	${AR} crus libgdm2s3m.a ${objs}

clean: dclean
	rm -f *.o *~ libgdm2s3m.a

dclean:
	rm -f *.dbg.o libgdm2s3m.a

