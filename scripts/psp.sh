platform_enter_hook() {
	export DEVKITPRO=$HOME/bin/devkitpro
	export DEVKITPSP=$HOME/bin/devkitpro/devkitPSP
	export PATH=$DEVKITPSP/bin:$DEVKITPSP/psp/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform psp --prefix $DEVKITPSP/psp \
	              --optimize-size --disable-editor --disable-helpsys \
	              --disable-utils --disable-libpng --enable-meter"
}

platform_exit_hook() {
	DUMMY=1
}

platform_build_test_hook() {
	if [ ! -f mzxrun ]; then
		ERRNO=1
		return
	fi

	file mzxrun | egrep -q "ELF 32-bit.*LSB.*MIPS.*statically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/psp/*psp.zip ] && ERRNO=2
}
