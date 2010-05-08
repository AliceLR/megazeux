platform_enter_hook() {
	export PATH=$HOME/bin/mingw64/bin:$HOME/bin/mingw64/local/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform mingw64 \
	              --prefix $HOME/bin/mingw64"
}

platform_exit_hook() {
	DUMMY=1
}

platform_build_test_hook() {
	if [ ! -f mzxrun.exe -o ! -f megazeux.exe ]; then
		ERRNO=1
		return
	fi

	file mzxrun.exe | egrep -q "PE32\+.*MS.Windows"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi

	file megazeux.exe | egrep -q "PE32\+.*MS.Windows"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/windows-x64/*-x64.zip ] && ERRNO=2
}
