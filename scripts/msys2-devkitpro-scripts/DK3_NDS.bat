call VersionSettings.bat
call DevkitProSettings.bat

:: Build and package
usr\bin\bash -l /dk-nds-build.sh %DEVKITPRO% %DEVKITARM% %stable_branch%

pause
