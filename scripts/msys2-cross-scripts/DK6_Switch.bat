call VersionSettings.bat

:: Build and package
usr\bin\bash -l /dk-switch-build.sh %stable_branch%

pause
