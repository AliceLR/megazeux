call VersionSettings.bat
call DevkitProSettings.bat

:: Build and package
usr\bin\bash -l /dk-wii-build.sh %DEVKITPRO% %DEVKITPPC% %stable_branch%

pause
