@echo off
rem =========================================================================
rem  start_winuae.bat - Startet STRUDELFORCE direkt in WinUAE (Amiga 500)
rem =========================================================================
setlocal
set "WINUAE=C:\Program Files\WinUAE\winuae64.exe"
set "ROOT=%~dp0.."
set "CONFIG=%ROOT%\tools\winuae_a500.uae"

if not exist "%WINUAE%" (
    echo [FEHLER] WinUAE nicht gefunden unter: %WINUAE%
    pause
    exit /b 1
)

echo [OK] Starte STRUDELFORCE in WinUAE (Amiga 500 PAL 50Hz, Kickstart 1.3)...
"%WINUAE%" -f "%CONFIG%"
endlocal
