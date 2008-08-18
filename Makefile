#
# Slave MegaZeux makefile
#

include Makefile.platform

subdir:
	cd contrib/libmodplug/src && ${MAKE} libs && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} libs && cd ../../..
	cd src                    && ${MAKE} mzx && cd ..

subdir-debug:
	cd contrib/libmodplug/src && ${MAKE} libs DEBUG=1 && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} libs DEBUG=1 && cd ../../..
	cd src                    && ${MAKE} mzx DEBUG=1 && cd ..

clean:
	cd contrib/libmodplug/src && ${MAKE} clean && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} clean && cd ../../..
	cd src                    && ${MAKE} clean && cd ..

dclean:
	cd contrib/libmodplug/src && ${MAKE} clean DEBUG=1 && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} clean DEBUG=1 && cd ../../..
	cd src                    && ${MAKE} clean DEBUG=1 && cd ..

distclean: clean dclean
	rm -f src/config.h
	cp -f arch/Makefile.dist Makefile.platform
