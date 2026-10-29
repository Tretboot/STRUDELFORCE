@echo off
rem =========================================================================
rem  retroarch_test.bat - STRUDELFORCE Amiga-ADF unter RetroArch (PUAE/A500)
rem
rem  Startet die Steam-RetroArch-Installation mit dem PUAE-A500-Core,
rem  Kickstart 1.3 aus ./Kickstart und dem gebauten strudelforce.adf.
rem  Fehlt die ADF, wird erst `make amiga_adf` ausgefuehrt.
rem =========================================================================
setlocal
set "RA=C:\Program Files (x86)\Steam\steamapps\common\RetroArch\retroarch.exe"
set "CORE=C:\Program Files (x86)\Steam\steamapps\common\RetroArch\cores\puae_libretro.dll"
set "ROOT=%~dp0.."
set "ADF=%ROOT%\strudelforce.adf"

if not exist "%RA%" (
    echo [FEHLER] RetroArch nicht gefunden: %RA%
    echo Bitte den Pfad oben anpassen (oder EmuVR-Instanz).
    exit /b 1
)
if not exist "%CORE%" (
    echo [FEHLER] PUAE-Core nicht gefunden: %CORE%
    exit /b 1
)
if not exist "%ADF%" (
    echo [INFO] strudelforce.adf fehlt - wird gebaut (make amiga_adf)...
    cd /d "%ROOT%"
    make amiga_adf
    if not exist "%ADF%" (
        echo [FEHLER] ADF-Build fehlgeschlagen. Voraussetzungen:
        echo          m68k-amigaos-gcc + xdftool (pip install amitools)
        exit /b 1
    )
)

echo [OK] RetroArch (PUAE A500) + Kickstart + ADF starten...
"%RA%" --appendconfig "%ROOT%\tools\retroarch_amiga.cfg" -L "%CORE%" "%ADF%"
endlocal