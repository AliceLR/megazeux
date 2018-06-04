:: Run as administrator.
:: %1 - portlibs folder.

pushd %1

:: 3DS
mklink /d armv6k 3ds

:: Wii
mklink /d wii ppc

:: Switch
mklink /d cortex-a57 switch

popd
