;FileZilla Server Setup script
;written by Tim Kosse (tim.kosse@filezilla-project.org)
;Based on
;NSIS Modern User Interface version 1.6
;Basic Example Script
;Written by Joost Verburg

Unicode true

;Set compressor before outputting anything
SetCompressor /SOLID LZMA

;--------------------------------
;Include Modern UI and functions

  !include "MUI.nsh"
  !include "WinVer.nsh"
  !include "x64.nsh"

;--------------------------------
;Product Info

  !define PRODUCT_NAME    "FileZilla Server"
  !define VERSION_MAJOR   "0"
  !define VERSION_MINOR   "9"
  !define VERSION_MICRO   "59"
  !define VERSION_NANO    "0"
  !define PRODUCT_VERSION "beta ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}"
  !define VERSION_FULL    "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}.${VERSION_NANO}"
  !define PUBLISHER       "FileZilla Project"
  !define WEBSITE_URL     "https://filezilla-project.org/"
  !define PRODUCT_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"

;--------------------------------
;Installer's VersionInfo

  VIProductVersion                   "${VERSION_FULL}"
  VIAddVersionKey "CompanyName"      "${PUBLISHER}"
  VIAddVersionKey "ProductName"      "${PRODUCT_NAME}" 
  VIAddVersionKey "ProductVersion"   "${PRODUCT_VERSION}"
  VIAddVersionKey "FileDescription"  "${PRODUCT_NAME}"
  VIAddVersionKey "FileVersion"      "${PRODUCT_VERSION}"
  VIAddVersionKey "LegalCopyright"   "${PUBLISHER}"
  VIAddVersionKey "OriginalFilename" "FileZilla_Server-${VERSION_MAJOR}_${VERSION_MINOR}_${VERSION_MICRO}.exe"

;StartOptions Page strings
LangString StartOptionsTitle ${LANG_ENGLISH} ": Server startup settings"

;--------------------------------
;Modern UI Configuration

  !define MUI_ABORTWARNING

  !define MUI_ICON "..\res\filezilla server.ico"
  !define MUI_UNICON "uninstall.ico"

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_LICENSE "..\..\license.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  Page custom StartOptions
  Page custom InterfaceOptions
  !insertmacro MUI_PAGE_INSTFILES

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;More

  ;General
  OutFile "../../FileZilla_Server-${VERSION_MAJOR}_${VERSION_MINOR}_${VERSION_MICRO}.exe"

  ;Installation types
  InstType "Standard"
  InstType "Full"
  InstType "Service only"
  InstType "Interface only"

  ;Descriptions
  LangString DESC_SecFileZillaServer ${LANG_ENGLISH} "Copy FileZilla Server to the application folder."
  LangString DESC_SecFileZillaServerInterface ${LANG_ENGLISH} "Copy the administration interface to the application folder."
  LangString DESC_SecSourceCode ${LANG_ENGLISH} "Copy the source code of FileZilla Server to the application folder"
  LangString DESC_SecStartMenu ${LANG_ENGLISH} "Create shortcuts to FileZilla Server in the Start Menu"
  LangString DESC_SecDesktopIcon ${LANG_ENGLISH} "Create an Icon on the desktop for quick access to the administration interface"

  ;Folder-selection page
  InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
  InstallDirRegKey HKLM "${PRODUCT_UNINSTALL}" "UninstallString"

  ShowInstDetails show

  RequestExecutionLevel admin

;--------------------------------
;Reserve Files

  ;Things that need to be extracted on first (keep these lines before any File command!)
  ;Only useful for BZIP2 compression

  ReserveFile "StartupOptions.ini"
  ReserveFile "InterfaceOptions.ini"
  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

;--------------------------------
;Installer Sections

Var GetInstalledSize.total

Section "-default files"
  SectionIn 1 2 3 4

  ; Stopping interface
  DetailPrint "Closing interface..."
  Push "FileZilla Server Main Window"
  Call CloseWindowByName

  ${If} ${FileExists} "$INSTDIR\FileZilla Server.exe"
    ExecWait '"$INSTDIR\FileZilla Server.exe" /stop'
    ExecWait '"$INSTDIR\FileZilla Server.exe" /compat /stop'
    Sleep 500
    Push "FileZilla Server Helper Window"
    call CloseWindowByName
  ${EndIf}
  Sleep 100

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; Put file there
  File "..\Release\libeay32.dll"
  File "..\Release\ssleay32.dll"
  File "..\..\readme.htm"
  File "..\..\legal.htm"
  File "..\..\license.txt"

  ; Write the uninstall keys for Windows
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "DisplayName"     "FileZilla Server"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "DisplayIcon"     "$INSTDIR\FileZilla server.exe"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "DisplayVersion"  "${PRODUCT_VERSION}"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "HelpLink"        "${WEBSITE_URL}"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "URLInfoAbout"    "${WEBSITE_URL}"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "URLUpdateInfo"   "${WEBSITE_URL}"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr   HKLM "${PRODUCT_UNINSTALL}" "Publisher"       "${PUBLISHER}"
  WriteRegDWORD HKLM "${PRODUCT_UNINSTALL}" "VersionMajor"    "${VERSION_MAJOR}"
  WriteRegDWORD HKLM "${PRODUCT_UNINSTALL}" "VersionMinor"    "${VERSION_MINOR}"
  WriteRegDWORD HKLM "${PRODUCT_UNINSTALL}" "NoModify"        "1"
  WriteRegDWORD HKLM "${PRODUCT_UNINSTALL}" "NoRepair"        "1"

  ; Enable mini dumps
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  !define DUMP_KEY "SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\FileZilla Server.exe"
  WriteRegStr   HKLM "${DUMP_KEY}" "DumpFolder" "$INSTDIR"
  WriteRegDWORD HKLM "${DUMP_KEY}" "DumpType"   "1"
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

  Call GetInstalledSize
  WriteRegDWORD HKLM "${PRODUCT_UNINSTALL}" "EstimatedSize"  "$GetInstalledSize.total" ; Create/Write the reg key with the dword value

  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

Section "FileZilla Server (Service)" SecFileZillaServer
  SectionIn 1 2 3
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  IfFileExists "$INSTDIR\FileZilla Server.exe" found

  File "..\Release\FileZilla Server.exe"

  DetailPrint "Stopping service..."
  ExecWait '"$INSTDIR\FileZilla Server.exe" /stop'
  ExecWait '"$INSTDIR\FileZilla Server.exe" /compat /stop'
  Sleep 500
  Push "FileZilla Server Helper Window"
  call CloseWindowByName
  DetailPrint "Uninstalling service..."
  ExecWait '"$INSTDIR\FileZilla Server.exe" /uninstall'
  Sleep 500
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_NAME}"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_NAME}"
  goto copy_main_done
 found:
  GetTempFileName $R1
  File /oname=$R1 "..\Release\FileZilla Server.exe"
  DetailPrint "Stopping service..."
  ExecWait '"$R1" /stop'
  ExecWait '"$R1" /compat /stop'
  Sleep 500
  Push "FileZilla Server Helper Window"
  call CloseWindowByName
  DetailPrint "Uninstalling service..."
  ExecWait '"$R1" /uninstall'
  Sleep 500
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_NAME}"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_NAME}"
  Delete "$INSTDIR\FileZilla Server.exe"
  Rename $R1 "$INSTDIR\FileZilla Server.exe"
 copy_main_done:

SectionEnd

Section "Administration interface" SecFileZillaServerInterface
  SectionIn 1 2 4

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  File "..\Release\FileZilla Server Interface.exe"

SectionEnd

Section "Source Code" SecSourceCode
SectionIn 2
  SetOutPath "$INSTDIR\source"
  File "..\*.cpp"
  File "..\*.h"
  File "..\FileZilla Server.sln"
  File "..\FileZilla Server.vcxproj"
  File "..\FileZilla Server.rc"
  File "..\Dependencies.props.example"
  SetOutPath "$INSTDIR\source\res"
  File "..\res\*.ico"
  SetOutPath "$INSTDIR\source\misc"
  File "..\misc\*.h"
  File "..\misc\*.cpp"
  SetOutPath "$INSTDIR\source\hash_algorithms"
  File "..\hash_algorithms\*.h"
  File "..\hash_algorithms\*.c"
  SetOutPath "$INSTDIR\source\interface"
  File "..\interface\*.cpp"
  File "..\interface\*.h"
  File "..\interface\FileZilla Server Interface.vcxproj"
  File "..\interface\FileZilla Server.rc"
  SetOutPath "$INSTDIR\source\interface\res"
  File "..\interface\res\*.bmp"
  File "..\interface\res\*.ico"
  File "..\interface\res\*.rc2"
  File "..\interface\res\manifest.xml"
  SetOutPath "$INSTDIR\source\interface\misc"
  File "..\interface\misc\*.h"
  File "..\interface\misc\*.cpp"
  SetOutPath "$INSTDIR\source\install"
  File "FileZilla Server.nsi"
  File "StartupOptions.ini"
  File "InterfaceOptions.ini"
  File "uninstall.ico"
  SetOutPath "$INSTDIR\source\tinyxml"
  File "..\tinyxml\*.h"
  File "..\tinyxml\*.cpp"
SectionEnd

; optional section
Section "Start Menu Shortcuts" SecStartMenu
SectionIn 1 2 3 4
  SetShellVarContext all

  CreateDirectory "$SMPROGRAMS\FileZilla Server"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

  SectionGetFlags 1 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 done_service_startmenu

  !insertmacro MUI_INSTALLOPTIONS_READ $R0 "StartupOptions.ini" "Field 2" "State"
  StrCmp $R0 "Do not install as service, start server automatically (not recommended)" shortcutcompat

  ;NT service shortcuts
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Start FileZilla Server.lnk" "$INSTDIR\FileZilla Server.exe" "/start" "$INSTDIR\FileZilla Server.exe" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Stop FileZilla Server.lnk" "$INSTDIR\FileZilla Server.exe" "/stop" "$INSTDIR\FileZilla Server.exe" 0
  goto done_service_startmenu
 shortcutcompat:
  ;Compat mode
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Start FileZilla Server.lnk" "$INSTDIR\FileZilla Server.exe" "/compat /start" "$INSTDIR\FileZilla Server.exe" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Stop FileZilla Server.lnk" "$INSTDIR\FileZilla Server.exe" "/compat /stop" "$INSTDIR\FileZilla Server.exe" 0

 done_service_startmenu:

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 done_interface_startmenu

  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\FileZilla Server Interface.lnk" "$INSTDIR\FileZilla Server Interface.exe" "" "$INSTDIR\FileZilla Server Interface.exe" 0

 done_interface_startmenu:

  SectionGetFlags 3 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 done_source_startmenu

  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\FileZilla Server Source Project.lnk" "$INSTDIR\source\FileZilla Server.sln" "" "$INSTDIR\source\FileZilla Server.sln" 0

 done_source_startmenu:

SectionEnd

Section "Desktop Icon" SecDesktopIcon
SectionIn 1 2 4

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 noDesktopIcon

  CreateShortCut "$DESKTOP\FileZilla Server Interface.lnk" "$INSTDIR\FileZilla Server Interface.exe" "" "$INSTDIR\FileZilla Server Interface.exe" 0

 noDesktopIcon:

SectionEnd

Section "-PostInst"

  ; Write the installation path into the registry
  WriteRegStr HKCU "SOFTWARE\${PRODUCT_NAME}" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "SOFTWARE\${PRODUCT_NAME}" "Install_Dir" "$INSTDIR"

  SectionGetFlags 1 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 NoSetAdminPort

  ;Set Adminport
  !insertmacro MUI_INSTALLOPTIONS_READ $R0 "StartupOptions.ini" "Field 4" "State"
  ExecWait '"$INSTDIR\FileZilla Server.exe" /adminport $R0'

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 NoSetAdminPort

  ExecWait '"$INSTDIR\FileZilla Server Interface.exe" /adminport $R0'
 NoSetAdminPort:

  SectionGetFlags 1 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 done

  !insertmacro MUI_INSTALLOPTIONS_READ $R0 "StartupOptions.ini" "Field 2" "State"
  StrCmp $R0 "Do not install as service, start server automatically (not recommended)" Install_Standard_Auto
  DetailPrint "Installing Service..."
  StrCmp $R0 "Install as service, started manually" Install_AsService_Manual

  ExecWait '"$INSTDIR\FileZilla Server.exe" /install auto'
  goto done
 Install_AsService_Manual:
  ExecWait '"$INSTDIR\FileZilla Server.exe" /install'
  goto done
 Install_Standard_Auto:
  DetailPrint "Put FileZilla Server into registry..."
  ClearErrors
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server" '"$INSTDIR\FileZilla Server.exe" /compat /start'
  IfErrors Install_Standard_Auto_CU
  goto done
 Install_Standard_Auto_CU:
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server" '"$INSTDIR\FileZilla Server.exe" /compat /start'
 done:

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 interface_done

  ;Write interface startup settings
  !insertmacro MUI_INSTALLOPTIONS_READ $R0 $3 "Field 2" "State"
  StrCmp $R0 "Start manually" interface_done
  DetailPrint "Put FileZilla Server Interface into registry..."
  StrCmp $R0 "Start if user logs on, apply only to current user" interface_cu
  ClearErrors
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server Interface" '"$INSTDIR\FileZilla Server Interface.exe"'
  IfErrors interface_cu
  goto interface_done
 interface_cu:
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server Interface" '"$INSTDIR\FileZilla Server Interface.exe"'
 interface_done:

SectionEnd

;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecFileZillaServer} $(DESC_SecFileZillaServer)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecFileZillaServerInterface} $(DESC_SecFileZillaServerInterface)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSourceCode} $(DESC_SecSourceCode)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} $(DESC_SecStartMenu)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktopIcon} $(DESC_SecDesktopIcon)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Installer Functions

Function .onInit

  ${Unless} ${AtLeastWinVista}
    MessageBox MB_YESNO|MB_ICONSTOP "Unsupported operating system.$\nFileZilla Server requires at least Windows Vista and will not work on your system.$\nDo you really want to continue with the installation?" /SD IDNO IDYES installonoldwindows
    Abort
installonoldwindows:
  ${EndUnless}

  ;Extract InstallOptions INI Files
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "StartupOptions.ini"

  ;Extract InstallOptions INI Files
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "InterfaceOptions.ini"
  strcpy $3 "InterfaceOptions.ini"

FunctionEnd

LangString TEXT_IO_TITLE ${LANG_ENGLISH} "Startup settings"
LangString TEXT_IO_SUBTITLE ${LANG_ENGLISH} "Select startup behaviour for FileZilla Server"

Function StartOptions

  IfSilent DoneServerStartOptions

  SectionGetFlags 1 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 DoneServerStartOptions

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 1 ChangeNextTextToNext
  !insertmacro MUI_INSTALLOPTIONS_WRITE "StartupOptions.ini" "Settings" "NextButtonText" "&Install"
  goto DoneChangeNextText
 ChangeNextTextToNext:
  !insertmacro MUI_INSTALLOPTIONS_WRITE "StartupOptions.ini" "Settings" "NextButtonText" "&Next"
 DoneChangeNextText:


  !insertmacro MUI_HEADER_TEXT "$(TEXT_IO_TITLE)" "$(TEXT_IO_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "StartupOptions.ini"

 DoneServerStartOptions:

FunctionEnd

Function InterfaceOptions

  IfSilent DoneInterfaceStartOptions

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 DoneInterfaceStartOptions

  !insertmacro MUI_HEADER_TEXT "$(TEXT_IO_TITLE)" "$(TEXT_IO_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "InterfaceOptions.ini"

 DoneInterfaceStartOptions:

FunctionEnd

Function .onInstSuccess

  SectionGetFlags 1 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 startserverend

 !insertmacro MUI_INSTALLOPTIONS_READ $R0 "StartupOptions.ini" "Field 5" "State"

  strcmp $R0 "0" startserverend

  !insertmacro MUI_INSTALLOPTIONS_READ $R0 "StartupOptions.ini" "Field 2" "State"
  StrCmp $R0 "Do not install as service, start server automatically (not recommended)" startservercompat

  Exec '"$INSTDIR\FileZilla Server.exe" /start'
  goto startserverend
 startservercompat:
  Exec '"$INSTDIR\FileZilla Server.exe" /compat /start'
 startserverend:

  SectionGetFlags 2 $R0
  IntOp $R0 $R0 & 1
  IntCmp $R0 0 NoStartInterface

  !insertmacro MUI_INSTALLOPTIONS_READ $R0 $3 "Field 3" "State"
  strcmp $R0 "0" NoStartInterface
  Exec '"$INSTDIR\FileZilla Server Interface.exe"'
 NoStartInterface:
FunctionEnd

Function CloseWindowByName
  Exch $R1
 closewindow_start:
  FindWindow $R0 $R1
  strcmp $R0 0 closewindow_end
  SendMessage $R0 ${WM_CLOSE} 0 0
  Sleep 500
  goto closewindow_start
 closewindow_end:
  Pop $R1
FunctionEnd

Var prevSel
Function .onSelChange

  SectionGetFlags 1 $R0
  SectionGetFlags 2 $R1

  IntOp $R2 $R0 & 1
  IntOp $R2 $R2 | $prevSel
  IntCmp $R2 1 +2
    IntOp $R1 $R1 | 1

  IntOp $R2 $R1 & 1
  IntCmp $R2 1 +2
    IntOp $R0 $R0 | 1

  SectionSetFlags 1 $R0
  SectionSetFlags 2 $R1

  IntOp $prevSel $R1 & 1

FunctionEnd

Function GetInstalledSize
  Push $0
  Push $1
  StrCpy $GetInstalledSize.total 0
  ${ForEach} $1 0 256 + 1
    ${if} ${SectionIsSelected} $1
      SectionGetSize $1 $0
      IntOp $GetInstalledSize.total $GetInstalledSize.total + $0
    ${Endif}
  ${Next}
  Pop $1
  Pop $0
  IntFmt $GetInstalledSize.total "0x%08X" $GetInstalledSize.total
  Push $GetInstalledSize.total
FunctionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  SetShellvarContext all

  ; Stopping and uninstalling service
  DetailPrint "Stopping service..."
  ExecWait '"$INSTDIR\FileZilla Server.exe" /stop'
  Sleep 500
  Push "FileZilla Server Helper Window"
  Call un.CloseWindowByName
  DetailPrint "Uninstalling service..."
  ExecWait '"$INSTDIR\FileZilla Server.exe" /uninstall'
  Sleep 500

  ; Stopping interface
  DetailPrint "Closing interface..."
  Push "FileZilla Server Main Window"
  Call un.CloseWindowByName

  ; remove registry keys
  DeleteRegValue HKCU "Software\${PRODUCT_NAME}" "Install_Dir"
  DeleteRegValue HKLM "Software\${PRODUCT_NAME}" "Install_Dir"
  DeleteRegKey /ifempty HKCU "Software\${PRODUCT_NAME}"
  DeleteRegKey /ifempty HKLM "Software\${PRODUCT_NAME}"
  MessageBox MB_YESNO "Delete settings?" /SD IDNO IDNO NoSettingsDelete
  Delete "$INSTDIR\FileZilla Server.xml"
  Delete "$INSTDIR\FileZilla Server Interface.xml"
 NoSettingsDelete:
  DeleteRegKey HKLM "${PRODUCT_UNINSTALL}"
  ; remove files
  Delete "$INSTDIR\FileZilla Server.exe"
  Delete "$INSTDIR\FileZilla Server Interface.exe"
  Delete "$INSTDIR\ssleay32.dll"
  Delete "$INSTDIR\libeay32.dll"
  Delete $INSTDIR\license.txt
  Delete $INSTDIR\readme.htm
  Delete $INSTDIR\legal.htm
  Delete $INSTDIR\source\*.cpp
  Delete $INSTDIR\source\*.h
  Delete "$INSTDIR\source\FileZilla Server.sln"
  Delete "$INSTDIR\source\FileZilla Server.vcxproj"
  Delete "$INSTDIR\source\FileZilla Server.rc"
  Delete "$INSTDIR\source\Dependencies.props.example"
  Delete $INSTDIR\source\res\*.ico
  Delete $INSTDIR\source\res\*.bmp
  Delete $INSTDIR\source\res\*.rc2
  Delete $INSTDIR\source\misc\*.h
  Delete $INSTDIR\source\misc\*.cpp
  Delete $INSTDIR\source\hash_algorithms\*.h
  Delete $INSTDIR\source\hash_algorithms\*.c
  Delete $INSTDIR\source\interface\*.cpp
  Delete $INSTDIR\source\interface\*.h
  Delete "$INSTDIR\source\interface\FileZilla Server Interface.vcxproj"
  Delete "$INSTDIR\source\interface\FileZilla Server.rc"
  Delete $INSTDIR\source\interface\res\*.ico
  Delete $INSTDIR\source\interface\res\*.bmp
  Delete $INSTDIR\source\interface\res\*.rc2
  Delete $INSTDIR\source\interface\res\manifest.xml
  Delete $INSTDIR\source\interface\misc\*.h
  Delete $INSTDIR\source\interface\misc\*.cpp
  Delete $INSTDIR\source\install\uninstall.ico
  Delete "$INSTDIR\source\install\FileZilla Server.nsi"
  Delete "$INSTDIR\source\install\StartupOptions.ini"
  Delete "$INSTDIR\source\install\InterfaceOptions.ini"
  Delete "$INSTDIR\source\tinyxml\*.h"
  Delete "$INSTDIR\source\tinyxml\*.cpp"

  ; MUST REMOVE UNINSTALLER, too
  Delete $INSTDIR\uninstall.exe

  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.*"
  Delete "$DESKTOP\FileZilla Server Interface.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"

  ; remove directories used.

  RMDir "$INSTDIR\source\res"
  RMDir "$INSTDIR\source\misc"
  RMDir "$INSTDIR\source\hash_algorithms"
  RMDir "$INSTDIR\source\interface\res"
  RMDir "$INSTDIR\source\interface\misc"
  RMDir "$INSTDIR\source\interface"
  RMDir "$INSTDIR\source\install"
  RMDir "$INSTDIR\source\includes"
  RMDir "$INSTDIR\source\tinyxml"
  RMDir "$INSTDIR\source"
  RMDir "$INSTDIR"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server Interface"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FileZilla Server Interface"

  RMDir "$INSTDIR"

  ; Remove dump key
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  DeleteRegValue HKLM "${DUMP_KEY}" "DumpFolder"
  DeleteRegValue HKLM "${DUMP_KEY}" "DumpType"
  DeleteRegKey /ifempty HKLM "${DUMP_KEY}"
  ${If} ${RunningX64}
    SetRegView lastused
  ${EndIf}

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.CloseWindowByName
  Exch $R1
 unclosewindow_start:
  FindWindow $R0 $R1
  strcmp $R0 0 unclosewindow_end
  SendMessage $R0 ${WM_CLOSE} 0 0
  Sleep 500
  goto unclosewindow_start
  Pop $R1
 unclosewindow_end:
FunctionEnd
