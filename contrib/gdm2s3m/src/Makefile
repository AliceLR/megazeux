#
# src/ makefile
#

include ../../../Makefile.in
include ../../../Makefile.platform

# must be build with c99
CFLAGS  += -std=c99

# don't want to pull in SDL here
INCLUDES =

#
# objects to build
#

objs  = error.${o} gdm.${o} s3m.${o} utility.${o} gdm2s3m.${o}

ifeq (${BUILD},1)
libs:	${objs}
	${AR} crus libgdm2s3m.a ${objs}
else
libs:
	@echo "--> Building of GDM2S3M disabled."
endif

clean:
	rm -f ${objs} *~ libgdm2s3m.a
