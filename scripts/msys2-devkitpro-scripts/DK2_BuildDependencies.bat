:: Build platform-specific MegaZeux dependencies and install them
:: to %DEVKITPRO%\portlibs or likewise. This need not be done if
:: your devkitPro installation already has all of the required
:: dependencies.

call DevkitProSettings.bat

::usr\bin\bash -l /dk-portlibs-repo.sh

usr\bin\bash -l /dk-nds-dependencies.sh %DEVKITPRO% %DEVKITARM%
usr\bin\bash -l /dk-3ds-dependencies.sh %DEVKITPRO% %DEVKITARM%
usr\bin\bash -l /dk-wii-dependencies.sh %DEVKITPRO% %DEVKITPPC%
usr\bin\bash -l /dk-psp-dependencies.sh %DEVKITPRO% %DEVKITPSP%
usr\bin\bash -l /dk-switch-dependencies.sh %DEVKITPRO% %DEVKITA64%

pause
