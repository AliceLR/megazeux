setlocal enableextensions enabledelayedexpansion
cd %~dp0

git describe --exact-match --tags>vertemp.txt
if not "%ERRORLEVEL%" == "0" (
	echo #define VERSION "git">version.h
) else (
	for /f "delims=" %%a in (vertemp.txt) do set VERSION=%%a
	echo #define VERSION "!VERSION!">version.h
)

del vertemp.txt
