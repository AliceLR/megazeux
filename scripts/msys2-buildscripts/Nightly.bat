call VersionSettings.bat

:: This script is a work-in-progress variant of 2_CreateReleases.bat intended
:: to be run automatically to produce master and debytecode updater releases.

usr\bin\bash -l /mingw-nightly-check.sh || EXIT /b 0

usr\bin\rm -rf /mzx-build-workingdir/releases

:: From 2_CreateReleases.bat
usr\bin\bash -l /mingw-build.sh x86 win32 "" %unstable% %unstable_branch% MINGW32
usr\bin\bash -l /mingw-build.sh x86 win32 "--enable-debytecode" %debytecode% %debytecode_branch% MINGW32

usr\bin\bash -l /mingw-build.sh x64 win64 "" %unstable% %unstable_branch% MINGW64
usr\bin\bash -l /mingw-build.sh x64 win64 "--enable-debytecode" %debytecode% %debytecode_branch% MINGW64

:: use @echo to skip the pause.
@echo | call 3_PackageReleases.bat

:: use @echo to skip the pause.
@echo | call 4_UploadReleases.bat
