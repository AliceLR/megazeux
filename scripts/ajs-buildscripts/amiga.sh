platform_enter_hook() {
	export PATH=$HOME/bin/amiga/bin:/usr/local/amiga/SDK/local/clib2/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform amiga \
	              --prefix /usr/local/amiga --disable-libpng \
	              --disable-utils"
}

platform_exit_hook() {
	DUMMY=1
}

platform_build_test_hook() {
	if [ ! -f mzxrun.exe -o ! -f megazeux.exe ]; then
		ERRNO=1
		return
	fi

	file mzxrun.exe | egrep -q "ELF 32-bit.*MSB.*PowerPC.*dynamically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi

	file megazeux.exe | egrep -q "ELF 32-bit.*MSB.*PowerPC.*dynamically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/amiga/*amiga.lha ] && ERRNO=2
}
