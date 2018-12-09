platform_enter_hook() {
	BUILD_FLAGS="dist"
	CONFIG_FLAGS="$CONFIG_FLAGS --platform darwin \
	              --prefix blah --disable-utils --disable-x11"
}

platform_exit_hook() {
        DUMMY=1
}

platform_build_test_hook() {
        if [ ! -f megazeux -o ! -f mzxrun ]; then
                ERRNO=1
                return
        fi

        file megazeux | egrep -q "Mach-O universal binary"
        if [ "$?" != "0" ]; then
                ERRNO=1
                return
        fi

        file mzxrun | egrep -q "Mach-O universal binary"
        if [ "$?" != "0" ]; then
                ERRNO=1
                return
        fi
}

platform_archive_test_hook() {
        [ ! -f build/dist/darwin/*.dmg ] && ERRNO=2
}
