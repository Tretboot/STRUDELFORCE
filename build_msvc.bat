@echo off
rem ============================================================================
rem STRUDELFORCE Windows MSVC Build Script (Consolidated Monolithic Architecture)
rem
rem 3 Kern-Ziele:
rem   [1/3] strudelforce_game.exe - 50Hz ANSI/ASCII Terminal-Spiel & Headless Demo
rem   [2/3] strudelforce_v0.1.exe - HAUPT-VERSION: 2.5D Voxel GUI, Jukebox, Explorer
rem   [3/3] test_suite.exe        - Konsolidierte Headless Integrationstest-Suite
rem ============================================================================

setlocal enabledelayedexpansion

echo === STRUDELFORCE MSVC Build (Consolidated Architecture) ===

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

where cl >nul 2>&1
if errorlevel 1 (
    echo FEHLER: cl.exe nicht im Pfad gefunden.
    exit /b 1
)

set CFLAGS=/nologo /W3 /O2 /Isrc /D_CRT_SECURE_NO_WARNINGS
set CORE_SRCS=src\game_logic.c src\soundengine_core.c src\engine_core.c
set LIBS=user32.lib gdi32.lib winmm.lib

echo.
echo [1/3] Kompiliere strudelforce_game.exe (Terminal + Audio)...
cl %CFLAGS% src\game_main.c %CORE_SRCS% %LIBS% /Fe:strudelforce_game.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von strudelforce_game.exe
    exit /b 1
)

echo.
echo [2/3] Kompiliere strudelforce_v0.1.exe (HAUPT-VERSION: 2.5D Voxel GUI)...
cl %CFLAGS% src\gui_main.c %CORE_SRCS% %LIBS% /Fe:strudelforce_v0.1.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von strudelforce_v0.1.exe
    exit /b 1
)

echo.
echo [3/3] Kompiliere test_suite.exe (Konsolidierte Test-Suite)...
cl %CFLAGS% src\test_suite.c %CORE_SRCS% %LIBS% /Fe:test_suite.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von test_suite.exe
    exit /b 1
)

echo.
echo ============================================================================
echo ERFOLG: Alle STRUDELFORCE Executables wurden erfolgreich gebaut!
echo   - strudelforce_game.exe : 50Hz ANSI/ASCII Terminal-Spiel
echo   - strudelforce_v0.1.exe : HAUPT-VERSION zum Spielen (2.5D GUI)
echo   - test_suite.exe        : Konsolidierte Testsuite (alle Subsysteme)
echo ============================================================================
