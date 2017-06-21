call VersionSettings.bat

usr\bin\bash -l /mingw-build.sh x86 win32 "" %stable% %stable_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x86 win32 "" %unstable% %unstable_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x86 win32 "--enable-debytecode" %debytecode% %debytecode_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x64 win64 "" %stable% %stable_branch% MINGW64
usr\bin\bash -l /mingw-build.sh x64 win64 "" %unstable% %unstable_branch% MINGW64
usr\bin\bash -l /mingw-build.sh x64 win64 "--enable-debytecode" %debytecode% %debytecode_branch% MINGW64

pause
