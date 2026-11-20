; Kalkyl: installation per användare med NSIS 3 och Modern UI 2, på svenska.
; Målet installer (installer/CMakeLists.txt) skickar in VERSION, PUBLISHER, EXE, ASSETS,
; ICON och OUTFILE med -D. Versionen står aldrig här utan kommer ur project(VERSION).
; Allt skrivs i användarens profil och under HKCU, så ingen administratörsrätt behövs.
Target amd64-unicode
RequestExecutionLevel user
ManifestDPIAware true
XPStyle on
SetCompressor /SOLID lzma

; Stoppar bygget med ett tydligt fel om ett värde från CMake saknas.
!macro RequireDefine NAME
  !ifndef ${NAME}
    !error "${NAME} saknas. Bygg med: cmake --build --preset installer"
  !endif
!macroend
!insertmacro RequireDefine VERSION
!insertmacro RequireDefine PUBLISHER
!insertmacro RequireDefine EXE
!insertmacro RequireDefine ASSETS
!insertmacro RequireDefine ICON
!insertmacro RequireDefine OUTFILE

; Uninstall-nyckeln under HKCU och Kalkyls datamapp. Datamappen (system.db, arbete\)
; tillhör användaren: den får aldrig bli installationsmapp och rörs aldrig.
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalkyl"
!define DATA_DIR "$LOCALAPPDATA\Kalkyl"

Name "Kalkyl ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Kalkyl"
InstallDirRegKey HKCU "${UNINST_KEY}" "InstallLocation"
BrandingText "${PUBLISHER}"
AllowSkipFiles off

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"

; En tidigare installation enligt Uninstall-nyckeln, och om skrivbordsgenvägen fanns.
Var OldDir
Var OldVersion
Var HadDesktop

; Ikonen ur res/kalkyl.ico för setup och avinstallation, och en fråga innan man avbryter.
!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING

; Installationens sidor: välkomst, licens, mapp (med kontroll), installation och slut.
!define MUI_WELCOMEPAGE_TEXT "Guiden installerar Kalkyl ${VERSION} för dig. Ingen administratörsbehörighet behövs.$\r$\n$\r$\nStäng Kalkyl innan du fortsätter, om programmet är öppet. Dina projekt och inställningar påverkas inte.$\r$\n$\r$\n$_CLICK"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${__FILEDIR__}\licens.txt"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Kalkyl installeras i mappen nedan, bara för dig. Väljer du en annan mapp än en tidigare installation flyttas Kalkyl dit."
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\kalkyl.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Starta Kalkyl"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Skapa en genväg på skrivbordet"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut
!insertmacro MUI_PAGE_FINISH

; Avinstallationens sidor: bekräftelse med besked om att data ligger kvar, och förlopp.
!define MUI_UNCONFIRMPAGE_TEXT_TOP "Kalkyl tas bort från datorn. Projektfilerna (.kalkyl) och dina inställningar i ${DATA_DIR} ligger kvar."
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Swedish"

; setup.exe:s versionsinformation. Den fjärde siffran är alltid 0.
VIProductVersion "${VERSION}.0"
VIFileVersion "${VERSION}.0"
VIAddVersionKey /LANG=${LANG_SWEDISH} "ProductName" "Kalkyl"
VIAddVersionKey /LANG=${LANG_SWEDISH} "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=${LANG_SWEDISH} "FileDescription" "Installation av Kalkyl"
VIAddVersionKey /LANG=${LANG_SWEDISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_SWEDISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_SWEDISH} "LegalCopyright" "© SWECO Sverige AB. Internt program."
VIAddVersionKey /LANG=${LANG_SWEDISH} "Comments" "Luftledningsberäkningar"
VIAddVersionKey /LANG=${LANG_SWEDISH} "OriginalFilename" "Kalkyl-${VERSION}-setup.exe"

; Läser en tidigare installation ur Uninstall-nyckeln: mappen (för flytt) och versionen.
; Är den installerade versionen nyare frågar guiden först. Tyst läge svarar ja,
; så att IT kan gå tillbaka till en äldre version.
Function .onInit
  SetShellVarContext current
  ReadRegStr $OldDir HKCU "${UNINST_KEY}" "InstallLocation"
  ReadRegStr $OldVersion HKCU "${UNINST_KEY}" "DisplayVersion"
  ${If} $OldVersion != ""
    ${VersionCompare} "${VERSION}" "$OldVersion" $0
    ${If} $0 == 2
      MessageBox MB_YESNO|MB_ICONQUESTION "En nyare version av Kalkyl ($OldVersion) är redan installerad.$\r$\nVill du ersätta den med version ${VERSION}?" /SD IDYES IDYES keep
      Abort
      keep:
    ${EndIf}
  ${EndIf}
FunctionEnd

; Svarar i $0 med 1 om $INSTDIR är Kalkyls datamapp eller ligger i den, annars 0.
; StrCmp jämför utan hänsyn till skiftläge. Funktionen använder $1–$3.
Function IsDataDir
  StrCpy $0 0
  StrLen $1 "${DATA_DIR}"
  StrCpy $2 "$INSTDIR" $1
  ${If} $2 == "${DATA_DIR}"
    StrCpy $3 "$INSTDIR" 1 $1
    ${If} $3 == ""
    ${OrIf} $3 == "\"
      StrCpy $0 1
    ${EndIf}
  ${EndIf}
FunctionEnd

; Mappsidans kontroll: datamappen avvisas med ett besked, och sidan ligger kvar.
Function DirectoryLeave
  Call IsDataDir
  ${If} $0 == 1
    MessageBox MB_OK|MB_ICONEXCLAMATION "Kalkyl kan inte installeras i ${DATA_DIR}.$\r$\nDär ligger dina inställningar och arbetskopior. Välj en annan mapp."
    Abort
  ${EndIf}
FunctionEnd

; Väntar tills kalkyl.exe i mappen överst på stacken inte körs. Provet öppnar exe:n för
; skrivning, vilket Windows nekar så länge programmet körs. Användaren stänger Kalkyl och
; försöker igen. Avbryt, eller tyst läge, avbryter allt med kod 2 innan något ändrats.
!macro CheckNotRunningFunc PREFIX
Function ${PREFIX}CheckNotRunning
  Exch $R0
  Push $R1
  retry:
  ${If} ${FileExists} "$R0\kalkyl.exe"
    ClearErrors
    FileOpen $R1 "$R0\kalkyl.exe" a
    ${If} ${Errors}
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Kalkyl körs. Stäng alla Kalkyl-fönster och klicka på Försök igen." /SD IDCANCEL IDRETRY retry
      Abort
    ${EndIf}
    FileClose $R1
  ${EndIf}
  Pop $R1
  Pop $R0
FunctionEnd
!macroend
!insertmacro CheckNotRunningFunc ""
!insertmacro CheckNotRunningFunc "un."

; Genvägen på skrivbordet: slutsidans kryssruta, och vid uppgradering om den fanns förut.
Function CreateDesktopShortcut
  CreateShortcut "$DESKTOP\Kalkyl.lnk" "$INSTDIR\kalkyl.exe" "" "$INSTDIR\kalkyl.exe" 0 SW_SHOWNORMAL "" "Luftledningsberäkningar"
FunctionEnd

; Installerar programmet. Först kontrolleras mappen och att Kalkyl inte körs. En tidigare
; installation i en annan mapp tas bort med sin egen avinstallerare (flytt). Filtypen
; registreras av kalkyl.exe själv, så att registervärdena bara finns i association.cpp.
Section "Kalkyl" SecKalkyl
  Call IsDataDir
  ${If} $0 == 1
    MessageBox MB_OK|MB_ICONSTOP "Kalkyl kan inte installeras i ${DATA_DIR}." /SD IDOK
    Abort
  ${EndIf}
  Push $INSTDIR
  Call CheckNotRunning

  StrCpy $HadDesktop 0
  ${If} ${FileExists} "$DESKTOP\Kalkyl.lnk"
    StrCpy $HadDesktop 1
  ${EndIf}

  ; Flytt: den gamla installationen avinstalleras tyst och på plats (_?=), sedan städas den.
  ${If} $OldDir != ""
  ${AndIf} $OldDir != $INSTDIR
  ${AndIf} ${FileExists} "$OldDir\avinstallera.exe"
    Push $OldDir
    Call CheckNotRunning
    DetailPrint "Tar bort den tidigare installationen i $OldDir"
    ExecWait '"$OldDir\avinstallera.exe" /S _?=$OldDir'
    Delete "$OldDir\avinstallera.exe"
    RMDir "$OldDir"
  ${EndIf}

  SetOutPath "$INSTDIR"
  File "/oname=kalkyl.exe" "${EXE}"
  File "/oname=assets.bin" "${ASSETS}"
  WriteUninstaller "$INSTDIR\avinstallera.exe"

  CreateShortcut "$SMPROGRAMS\Kalkyl.lnk" "$INSTDIR\kalkyl.exe" "" "$INSTDIR\kalkyl.exe" 0 SW_SHOWNORMAL "" "Luftledningsberäkningar"
  ${If} $HadDesktop == 1
    Call CreateDesktopShortcut
  ${EndIf}

  ClearErrors
  ExecWait '"$INSTDIR\kalkyl.exe" --registrera' $0
  ${If} ${Errors}
  ${OrIf} $0 != 0
    DetailPrint "Filtypen .kalkyl kunde inte registreras ($0). Kalkyl registrerar den själv vid start."
  ${EndIf}
SectionEnd

; Raden i Appar och funktioner skrivs sist, när allt annat lyckats. Storleken (kB) är
; huvudsektionens filer. NoModify och NoRepair döljer Ändra och Reparera.
Section "-Appar och funktioner"
  SectionGetSize ${SecKalkyl} $0
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayName"          "Kalkyl"
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayVersion"       "${VERSION}"
  WriteRegStr   HKCU "${UNINST_KEY}" "Publisher"            "${PUBLISHER}"
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayIcon"          "$INSTDIR\kalkyl.exe,0"
  WriteRegStr   HKCU "${UNINST_KEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKCU "${UNINST_KEY}" "UninstallString"      '"$INSTDIR\avinstallera.exe"'
  WriteRegStr   HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\avinstallera.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify"             1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair"             1
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize"        $0
SectionEnd

; Avinstallationen arbetar i användarens profil, som installationen.
Function un.onInit
  SetShellVarContext current
FunctionEnd

; Tar bort programmet. Filtypen avregistreras först, av kalkyl.exe själv, som lämnar en
; annan befintlig kopias registrering orörd. Sedan tas bara de egna filerna bort, och mappen
; bara om den är tom (aldrig RMDir /r). Användarens datamapp nämns inte och rörs aldrig.
Section "Uninstall"
  Push $INSTDIR
  Call un.CheckNotRunning
  ${If} ${FileExists} "$INSTDIR\kalkyl.exe"
    ExecWait '"$INSTDIR\kalkyl.exe" --avregistrera'
  ${EndIf}
  Delete "$INSTDIR\kalkyl.exe"
  Delete "$INSTDIR\assets.bin"
  Delete "$INSTDIR\avinstallera.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Kalkyl.lnk"
  Delete "$DESKTOP\Kalkyl.lnk"
  DeleteRegKey HKCU "${UNINST_KEY}"
SectionEnd
