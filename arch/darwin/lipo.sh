#!/bin/sh

#
# Lipo all the binaries together, if not already done
#
for FILE in "$@"; do
	[ -f ${FILE} ] && rm ${FILE}

	list=
	for ARCH in arm64 arm64e i686 x86_64 x86_64h ppc ppc64; do
		if [ -f "${FILE}.${ARCH}" ]; then
			list="$list ${FILE}.${ARCH}"
		fi
	done

	if [ -z "$list" ]; then
		echo "No target binaries are present!"
		breakout 4
	fi

	echo "lipo -create $list -output ${FILE}"
	if [ -f "$list" ]; then
		cp "$list" "${FILE}"
	else
		lipo -create $list -output ${FILE}
	fi

	if [ ! -f ${FILE} ]; then
		echo "Failed to compile '${FILE}'.."
		breakout 5
	fi
done
