@echo off

rem clean up afterwards ;p
del src\*.obj lib\gdm2s3m.lib bin\gdm2s3m.exe

rem build it!
cd src
cl /c /O2 /I . error.c gdm.c s3m.c utility.c gdm2s3m.c
link /LIB /OUT:..\lib\gdm2s3m.lib error.obj gdm.obj s3m.obj utility.obj
link /OUT:..\bin\gdm2s3m.exe error.obj gdm.obj s3m.obj utility.obj gdm2s3m.obj
cd ..