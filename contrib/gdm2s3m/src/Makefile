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

objs  = error.o gdm.o s3m.o utility.o gdm2s3m.o

subdir: libs

libs:	${objs}
	ar crus libgdm2s3m.a ${objs}

clean:
	rm -f ${objs} libgdm2s3m.a
