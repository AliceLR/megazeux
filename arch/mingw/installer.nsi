; MegaZeux NSIS script

!include "MUI2.nsh"

!define srcdir "dist/mzx282"
!define setup "mzx282-x86.exe"
!define exec "mzx282.exe"

!define prodname "MegaZeux"
!define icon "contrib/icons/quantump.ico"

!define regkey "Software\${prodname}"
!define uninstkey "Software\Microsoft\Windows\CurrentVersion\Uninstall\${prodname}"

!define startmenu "$SMPROGRAMS\${prodname}"
!define uninstaller "uninstall.exe"

!define website "https://www.digitalmzx.com/"

;--------------------------------

XPStyle on

ShowInstDetails hide
ShowUninstDetails hide

Name "${prodname}"
Caption "${prodname}"
OutFile "${setup}"

SetDateSave on
CRCCheck on
SilentInstall normal

SetCompress Auto
SetCompressor /SOLID lzma
SetCompressorDictSize 32
SetDatablockOptimize On

InstallDir "$PROGRAMFILES\${prodname}"
InstallDirRegKey HKLM "${regkey}" ""

RequestExecutionLevel admin

!define MUI_ABORTWARNING

!define MUI_ICON ${icon}
!define MUI_UNICON ${icon}

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;--------------------------------

; Installer

Section

  WriteRegStr HKLM "${regkey}" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "${uninstkey}" "DisplayName" "${prodname} (remove only)"
  WriteRegStr HKLM "${uninstkey}" "UninstallString" '"$INSTDIR\${uninstaller}"'

  SetOutPath $INSTDIR

  ; package all files, recursively, preserving attributes
  ; assume files are in the correct places
  File /r "${srcdir}/*"

  WriteUninstaller "${uninstaller}"

SectionEnd

; Start Menu

Section

  CreateDirectory "${startmenu}"
  SetOutPath $INSTDIR ; for working directory
  CreateShortCut "${startmenu}\${prodname}.lnk" "$INSTDIR\${exec}" "" "$INSTDIR\${exec}"

  WriteINIStr "${startmenu}\web site.url" "InternetShortcut" "URL" ${website}

SectionEnd

; Uninstaller

UninstallText "This will uninstall ${prodname}."

Section "Uninstall"

  DeleteRegKey HKLM "${uninstkey}"
  DeleteRegKey HKLM "${regkey}"

  Delete "${startmenu}\*.*"
  Delete "${startmenu}"

  RMDir /r "$INSTDIR"

SectionEnd
