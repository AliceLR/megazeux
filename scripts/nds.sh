platform_enter_hook() {
	pushd arch/nds >/dev/null
		unzip -qq $HOME/megazeux/scripts/deps/nds.zip
	popd >/dev/null

	export DEVKITPRO=$HOME/bin/devkitpro
	export DEVKITARM=$HOME/bin/devkitpro/devkitARM
	export PATH=$DEVKITARM/bin:$PATH

	BUILD_FLAGS="debuglink"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform nds --prefix $DEVKITARM \
	              --optimize-size --disable-editor --disable-helpsys \
	              --disable-utils --disable-libpng --enable-meter"
}

platform_exit_hook() {
	rm -rf arch/nds/ndsLibfat arch/nds/ndsDebug
	rm -rf arch/nds/ndsScreens/build arch/nds/ndsScreens/lib
}

platform_build_test_hook() {
	if [ ! -f mzxrun ]; then
		ERRNO=1
		return
	fi

	file mzxrun | egrep -q "ELF 32-bit.*LSB.*ARM.*statically"
	if [ "$?" != "0" ]; then
		ERRNO=1
		return
	fi
}

platform_archive_test_hook() {
	[ ! -f build/dist/nds/*nds.zip ] && ERRNO=2
}
