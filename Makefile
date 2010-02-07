#
# Slave MegaZeux makefile
#

include Makefile.platform

subdir:
	cd contrib/libmodplug/src && ${MAKE} libs \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} libs \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd src                    && ${MAKE} mzx \
					BUILD_MODPLUG=${BUILD_MODPLUG} \
					BUILD_MIKMOD=${BUILD_MIKMOD} \
					&& cd ..

subdir-debug:
	cd contrib/libmodplug/src && ${MAKE} libs DEBUG=1 \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} libs DEBUG=1 \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd src                    && ${MAKE} mzx DEBUG=1 \
					BUILD_MODPLUG=${BUILD_MODPLUG} \
					BUILD_MIKMOD=${BUILD_MIKMOD} \
					&& cd ..

clean:
	cd contrib/libmodplug/src && ${MAKE} clean \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} clean \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd src                    && ${MAKE} clean \
					BUILD_MODPLUG=${BUILD_MODPLUG} \
					BUILD_MIKMOD=${BUILD_MIKMOD} \
					&& cd ..

dclean:
	cd contrib/libmodplug/src && ${MAKE} clean DEBUG=1 \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd contrib/gdm2s3m/src    && ${MAKE} clean DEBUG=1 \
					BUILD=${BUILD_MODPLUG} && cd ../../..
	cd src                    && ${MAKE} clean DEBUG=1 \
					BUILD_MODPLUG=${BUILD_MODPLUG} \
					BUILD_MIKMOD=${BUILD_MIKMOD} \
					&& cd ..

distclean: clean dclean
	rm -f src/config.h
	cp -f arch/Makefile.dist Makefile.platform
