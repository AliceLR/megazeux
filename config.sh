#!/bin/sh

if [ "$1" = "" ]; then
	if [ "`uname -o`" = "GNU/Linux" ]; then
		echo "Assuming Linux operating system.."
		cp -f arch/Makefile.linux Makefile.platform
		ARCH=linux
	else
		echo "Assuming Windows operating system.."
		cp -f arch/Makefile.win32 Makefile.platform
		ARCH=win32
	fi
else
	cp -f arch/Makefile.$1 Makefile.platform
	ARCH=$1
fi

if [ ! -f Makefile.platform ]; then
	echo "Invalid platform selection (see arch/)"
	exit 1
fi

if [ "$2" = "" ]; then
	echo "Assuming /usr prefix.."
	PREFIX=/usr
else
	PREFIX=$2
fi

echo                    >> Makefile.platform
echo "# install prefix" >> Makefile.platform
echo "PREFIX=$PREFIX"   >> Makefile.platform

if [ "$ARCH" = "win32" -o "$ARCH" = "macos" ]; then
	echo "#define MZX_DEFAULT_CHR \"mzx_default.chr\"" > src/config.h
	echo "#define MZX_BLANK_CHR \"mzx_blank.chr\"" >> src/config.h
	echo "#define MZX_SMZX_CHR \"mzx_smzx.chr\"" >> src/config.h
	echo "#define MZX_ASCII_CHR \"mzx_ascii.chr\"" >> src/config.h
	echo "#define SMZX_PAL \"smzx.pal\"" >> src/config.h
	echo "#define MZX_HELP_FIL \"mzx_help.fil\"" >> src/config.h
	echo "#define CONFIG_TXT \"config.txt\"" >> src/config.h
fi

if [ "$ARCH" = "linux" ]; then
	echo "#define MZX_DEFAULT_CHR \"$PREFIX/share/megazeux/mzx_default.chr\"" > src/config.h
	echo "#define MZX_BLANK_CHR \"$PREFIX/share/megazeux/mzx_blank.chr\"" >> src/config.h
	echo "#define MZX_SMZX_CHR \"$PREFIX/share/megazeux/mzx_smzx.chr\"" >> src/config.h
	echo "#define MZX_ASCII_CHR \"$PREFIX/share/megazeux/mzx_ascii.chr\"" >> src/config.h
	echo "#define SMZX_PAL \"$PREFIX/share/megazeux/smzx.pal\"" >> src/config.h
	echo "#define MZX_HELP_FIL \"$PREFIX/share/megazeux/mzx_help.fil\"" >> src/config.h
	echo "#define CONFIG_TXT \"/etc/megazeux-config\"" >> src/config.h
fi

echo "All done!"
