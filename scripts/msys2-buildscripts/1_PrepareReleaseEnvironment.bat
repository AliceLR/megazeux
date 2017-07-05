usr\bin\bash -l /mingw-init-environment.sh "https://github.com/AliceLR/megazeux.git"

:: MSYS2 pacman doesn't have libxmp as of this time and we want to statically
:: link it. So, make x86 and x64 builds of it.

usr\bin\bash -l /mingw-init-libxmp.sh x86 MINGW32
usr\bin\bash -l /mingw-init-libxmp.sh x64 MINGW64

pause
