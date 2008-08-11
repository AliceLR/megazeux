#
# Slave MegaZeux makefile
#

include Makefile.platform

subdir:
	cd contrib/libmodplug/src && make && cd ../../..
	cd contrib/gdm2s3m/src    && make && cd ../../..
	cd src                    && make && cd ..

clean:
	cd contrib/libmodplug/src && make clean && cd ../../..
	cd contrib/gdm2s3m/src    && make clean && cd ../../..
	cd src                    && make clean && cd ..

distclean: clean
	rm -f src/config.h
	cp -f arch/Makefile.dist Makefile.platform
