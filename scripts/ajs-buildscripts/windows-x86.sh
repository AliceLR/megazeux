platform_enter_hook() {
	export PATH=$HOME/bin/mingw32/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform mingw32 \
	              --prefix $HOME/bin/mingw32"
}

platform_exit_hook() {
	DUMMY=1
}

platform_build_test_hook() {
	if [ ! -f mzxrun.exe -o ! -f megazeux.exe ]; then
		ERRNO=1
		return
	fi

	file mzxrun.exe | egrep -q "PE32.*Intel.80386.*MS.Windows"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi

	file megazeux.exe | egrep -q "PE32.*Intel.80386.*MS.Windows"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/windows-x86/*-x86.zip ] && ERRNO=2
}
