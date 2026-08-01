; Octoglow Sender — Setup Wizard
; Built with NSIS (Nullsoft Scriptable Install System)

!include "MUI2.nsh"

; ---------- General ----------
Name "Octoglow Sender"
OutFile "Octoglow_Sender_Setup.exe"
InstallDir "$PROGRAMFILES64\Octoglow Sender"
InstallDirRegKey HKCU "Software\OctoglowSender" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

; ---------- Interface ----------
!define MUI_ICON "icons\app.ico"
!define MUI_UNICON "icons\app.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "icons\header_banner.bmp"
!define MUI_HEADERIMAGE_RIGHT
!define MUI_ABORTWARNING
!define MUI_WELCOMEFINISHPAGE_BITMAP "icons\wizard_side.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "icons\wizard_side.bmp"

; ---------- Pages ----------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\Octoglow Sender.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run Octoglow Sender now"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---------- Install ----------
Section "Octoglow Sender (required)" SEC01
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "Octoglow Sender.exe"
  File "icons\app.ico"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Start Menu shortcuts
  CreateDirectory "$SMPROGRAMS\Octoglow Sender"
  CreateShortCut "$SMPROGRAMS\Octoglow Sender\Octoglow Sender.lnk" "$INSTDIR\Octoglow Sender.exe" "" "$INSTDIR\app.ico"
  CreateShortCut "$SMPROGRAMS\Octoglow Sender\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  ; Registry info for Add/Remove Programs
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "DisplayName" "Octoglow Sender"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "DisplayIcon" "$INSTDIR\app.ico"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "Publisher" "Octoglow"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "DisplayVersion" "1.0.0"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender" "NoRepair" 1
SectionEnd

Section "Desktop Shortcut" SEC02
  CreateShortCut "$DESKTOP\Octoglow Sender.lnk" "$INSTDIR\Octoglow Sender.exe" "" "$INSTDIR\app.ico"
SectionEnd

; ---------- Uninstall ----------
Section "Uninstall"
  Delete "$INSTDIR\Octoglow Sender.exe"
  Delete "$INSTDIR\app.ico"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\Octoglow Sender\Octoglow Sender.lnk"
  Delete "$SMPROGRAMS\Octoglow Sender\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Octoglow Sender"
  Delete "$DESKTOP\Octoglow Sender.lnk"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OctoglowSender"
  DeleteRegKey /ifempty HKCU "Software\OctoglowSender"
SectionEnd
