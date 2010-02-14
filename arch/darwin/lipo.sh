#!/bin/sh

#
# Lipo all the binaries together, if not already done
#
for FILE in megazeux mzxrun libcore.dylib \
            libeditor.dylib; do
	[ -f ${FILE} ] && continue

	if [ ! -f ${FILE}.i686 -a ! -f ${FILE}.ppc ]; then
		echo "Neither ${FILE}.i686 nor ${FILE}.ppc are present!"
		breakout 4
	fi

	if [ -f ${FILE}.i686 -a ! -f ${FILE}.ppc ]; then
		mv ${FILE}.i686 ${FILE}
	elif [ -f ${FILE}.ppc -a ! -f ${FILE}.i686 ]; then
		mv ${FILE}.ppc ${FILE}
	else
		lipo -create ${FILE}.i686 ${FILE}.ppc -output ${FILE}
		rm -f ${FILE}.i686 ${FILE}.ppc
	fi

	if [ ! -f ${FILE} ]; then
		echo "Failed to compile '${FILE}'.."
		breakout 5
	fi
done
