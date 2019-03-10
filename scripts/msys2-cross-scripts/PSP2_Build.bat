call VersionSettings.bat

:: Build and package
usr\bin\bash -l /psp-build.sh %stable_branch%

pause
