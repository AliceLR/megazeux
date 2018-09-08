# $1 MINGW32 or MINGW64

case $1 in
	MINGW32|mingw32)
		echo "/mzx-build-workingdir/sdl2-mingw/i686-w64-mingw32/"
		;;

	MINGW64|mingw64)
		echo "/mzx-build-workingdir/sdl2-mingw/x86_64-w64-mingw32/"
		;;
esac
