platform_enter_hook() {
	pushd arch/wii >/dev/null
		unzip -qq $HOME/megazeux/scripts/deps/wii.zip
	popd >/dev/null

	export DEVKITPRO=$HOME/bin/devkitpro
	export DEVKITPPC=$HOME/bin/devkitpro/devkitPPC
	export PATH=$DEVKITPPC/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform wii --prefix $DEVKITPPC \
	              --optimize-size --disable-utils --disable-libpng \
	              --enable-meter"
}

platform_exit_hook() {
	rm -rf arch/wii/{libfat,libogg,libvorbis}
}

platform_build_test_hook() {
	if [ ! -f mzxrun -o ! -f megazeux ]; then
		ERRNO=1
		return
	fi

	file mzxrun | egrep -q "ELF 32-bit.*MSB.*PowerPC.*statically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi

	file megazeux | egrep -q "ELF 32-bit.*MSB.*PowerPC.*statically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/wii/*wii.zip ] && ERRNO=2
}
