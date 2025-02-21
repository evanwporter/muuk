OutFile "MuukInstaller.exe"

InstallDir "$PROGRAMFILES\Muuk"

Name "Muuk Installer"

Section "Install"
SetOutPath "$INSTDIR"
File /r "C:\Users\evanw\muuk\build\release\bin\*"
CreateShortCut "$SMPROGRAMS\Muuk\Muuk.lnk" "$INSTDIR\muuk.exe"
CreateShortCut "$DESKTOP\Muuk.lnk" "$INSTDIR\muuk.exe"
WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
Delete "$INSTDIR\*.*"
RMDir "$INSTDIR"
Delete "$SMPROGRAMS\Muuk\Muuk.lnk"
Delete "$DESKTOP\Muuk.lnk"
SectionEnd