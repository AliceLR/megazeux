call VersionSettings.bat
call DevkitProSettings.bat

:: Build and package
usr\bin\bash -l /dk-switch-build.sh %DEVKITPRO% %DEVKITA64% %stable_branch%

pause
