platform_enter_hook() {
	export PATH=$HOME/bin/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform gp2x \
	              --prefix $HOME/bin/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux \
	              --optimize-size --disable-editor --disable-helpsys
	              --disable-utils --enable-mikmod --enable-meter"
}

platform_exit_hook() {
	DUMMY=1
}

platform_build_test_hook() {
	if [ ! -f mzxrun.gpe ]; then
		ERRNO=1
		return
	fi

	file mzxrun.gpe | egrep -q "ELF 32-bit.*LSB.*ARM.*statically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/gp2x/*gp2x.zip ] && ERRNO=2
}
