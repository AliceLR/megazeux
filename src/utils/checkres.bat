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
:loop
if "%1"=="" goto end
echo Scanning %1...
checkres.exe %flags% %1
shift
pause
goto loop
:end