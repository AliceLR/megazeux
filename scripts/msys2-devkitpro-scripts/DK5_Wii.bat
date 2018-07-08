call VersionSettings.bat

:: Build and package
usr\bin\bash -l /dk-wii-build.sh %stable_branch%

pause
