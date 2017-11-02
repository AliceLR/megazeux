call VersionSettings.bat
call DevkitProSettings.bat

:: Build and package
usr\bin\bash -l /dk-psp-build.sh %DEVKITPRO% %DEVKITPSP% %stable_branch%

pause
