:: Build platform-specific MegaZeux dependencies and install them
:: to %DEVKITPRO%\portlibs or likewise. This need not be done if
:: your devkitPro installation already has all of the required
:: dependencies.

usr\bin\bash -l /dk-nds-dependencies.sh
usr\bin\bash -l /dk-3ds-dependencies.sh
usr\bin\bash -l /dk-wii-dependencies.sh
usr\bin\bash -l /dk-switch-dependencies.sh

pause
