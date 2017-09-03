@REM (C) 2007 Josh Matthews (mrlachatte@gmail.com)
@echo off
set flags=
:args
if "%1"=="-a" (
  set flags=%flags% -a
  shift
)
if "%1"=="-q" (
  set flags=%flags% -q
  shift
)
set ftemp=%1
if "%ftemp:~0,1%"=="-" goto args

echo checkres.bat is intended to be used with file dragging in Windows Explorer.
echo For more options, use checkres.exe from the command line directly.
echo.
if "%1"=="" goto nofile

:: %~dp0 is the directory this batch file is in. This allows a
:: .mzx/.zip to be dragged onto the batch file from another folder.
set exec=%~dp0\checkres.exe

:loop
if "%1"=="" goto end
echo Scanning %1...
%exec% %flags% %1
shift
pause
echo.
goto loop

:nofile
echo No input files provided.
echo.
pause
:end