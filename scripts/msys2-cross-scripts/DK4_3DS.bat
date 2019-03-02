call VersionSettings.bat

:: Build and package
usr\bin\bash -l /dk-3ds-build.sh %stable_branch%

pause
