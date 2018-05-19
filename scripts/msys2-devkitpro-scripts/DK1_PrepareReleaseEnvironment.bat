:: Set up extra dependencies required on top of the regular msys environment.

:: These scripts expect the following:
:: - 1_PrepareReleaseEnvironment.bat has been run
:: - DevkitPro is installed
:: - DevkitProSettings.bat is correctly configured for %DEVKITPRO%

usr\bin\bash -l /dk-install.sh
usr\bin\bash -l /dk-init-environment.sh

pause
