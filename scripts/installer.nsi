!include "MUI2.nsh"

Name "Academy"
OutFile "academy-windows-setup.exe"
InstallDir "$PROGRAMFILES64\Academy"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Portuguese"

Section "Academy"
  SetOutPath "$INSTDIR"
  File /r "dist\bin\*.*"
  CreateShortcut "$DESKTOP\Academy.lnk" "$INSTDIR\bin\qt_academy.exe"
SectionEnd