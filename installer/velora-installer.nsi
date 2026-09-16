!define PRODUCT_NAME "Velora Studio"
!define PRODUCT_PUBLISHER "Velora"
!define PRODUCT_WEB_SITE "https://velora.tv"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\VeloraStudio.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

!ifndef SOURCEDIR
  !define SOURCEDIR "..\..\dist"
!endif
!ifndef OUTDIR
  !define OUTDIR "..\..\dist-installer"
!endif

SetCompressor /SOLID lzma

!include "MUI2.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\64bit\VeloraStudio.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Velora Studio"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

Name "${PRODUCT_NAME}"
OutFile "${OUTDIR}\Velora-Studio-Setup-x64.exe"
InstallDir "$PROGRAMFILES64\Velora Studio"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File /r "${SOURCEDIR}\*.*"

  ; Shortcuts
  SetOutPath "$INSTDIR\bin\64bit"
  CreateDirectory "$SMPROGRAMS\Velora Studio"
  CreateShortcut "$SMPROGRAMS\Velora Studio\Velora Studio.lnk" "$INSTDIR\bin\64bit\VeloraStudio.exe" "" "$INSTDIR\bin\64bit\VeloraStudio.exe" 0
  CreateShortcut "$DESKTOP\Velora Studio.lnk" "$INSTDIR\bin\64bit\VeloraStudio.exe" "" "$INSTDIR\bin\64bit\VeloraStudio.exe" 0
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\bin\64bit\VeloraStudio.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\64bit\VeloraStudio.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "1.0.0"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
SectionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$DESKTOP\Velora Studio.lnk"
  Delete "$SMPROGRAMS\Velora Studio\Velora Studio.lnk"
  RMDir "$SMPROGRAMS\Velora Studio"

  RMDir /r "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
