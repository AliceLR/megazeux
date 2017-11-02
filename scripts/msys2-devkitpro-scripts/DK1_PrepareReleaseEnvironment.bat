:: Set up extra dependencies required on top of the regular msys environment.

:: These scripts expect the following:
:: - 1_PrepareReleaseEnvironment.bat has been run
:: - DevkitPro is installed
:: - DevkitProSettings.bat is correctly configured for %DEVKITPRO%

call DevkitProSettings.bat

usr\bin\bash -l /dk-init-environment.sh

:: Set up dependencies for target platforms.
usr\bin\bash -l /dk-nds-dependencies.sh %DEVKITPRO% %DEVKITARM%
usr\bin\bash -l /dk-3ds-dependencies.sh %DEVKITPRO% %DEVKITARM%
usr\bin\bash -l /dk-psp-dependencies.sh %DEVKITPRO% %DEVKITPSP%

pause
