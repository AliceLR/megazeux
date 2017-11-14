call VersionSettings.bat
call DevkitProSettings.bat

:: Build and package
usr\bin\bash -l /dk-3ds-build.sh %DEVKITPRO% %DEVKITARM% %stable_branch%

pause
