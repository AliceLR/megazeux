call VersionSettings.bat

:: Build and package
usr\bin\bash -l /dk-nds-build.sh %stable_branch%

pause
