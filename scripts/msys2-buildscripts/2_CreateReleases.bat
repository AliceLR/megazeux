SET stable=2.90
SET stable_branch=v2.90
SET unstable=master
SET unstable_branch=master
SET debytecode=debytecode
SET debytecode_branch=master

usr\bin\bash -l /mingw-build.sh x86 win32 "" %stable% %stable_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x86 win32 "" %unstable% %unstable_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x86 win32 "--enable-debytecode" %debytecode% %debytecode_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x64 win64 "" %stable% %stable_branch% MINGW64
usr\bin\bash -l /mingw-build.sh x64 win64 "" %unstable% %unstable_branch% MINGW64
usr\bin\bash -l /mingw-build.sh x64 win64 "--enable-debytecode" %debytecode% %debytecode_branch% MINGW64
usr\bin\bash -l /mingw-release.sh "%stable% %debytecode% %unstable%" "windows-x64 windows-x86" Stable Debytecode Unstable
