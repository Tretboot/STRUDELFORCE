@echo off
rem ============================================================================
rem STRUDELFORCE Windows MSVC Build Script (unified, post-Merge)
rem
rem 7 Targets:
rem   [1/7] strudelforce_game.exe    - Terminal-Spiel (Smith-Kern core_sim.c)
rem   [2/7] strudelforce_v0.1.exe    - HAUPT-VERSION: 2.5D Voxel GUI + Jukebox + Explorer (Smith-Kern)
rem   [3/7] test_audio_mix.exe       - TFMX+MOD+Mixer Harness (v3 Audio-Stack)
rem   [4/7] strudelforce_test.exe    - Phase-1 43-Check (sf-Kern core_sim_sf.c!)
rem   [5/7] strudelforce_headless.exe- Audio-Parser-Selftest
rem   [6/7] win_audio_selftest.exe   - Win-Audio-Transport v2 (Ring/Thread-Selftest)
rem   [7/7] test_embedded_music.exe  - Selftest der In-EXE-Tracks (embedded_music.h)
rem
rem WICHTIG: core_sim.c und core_sim_sf.c definieren beide core_init/core_tick
rem          -> niemals in EINEM Binary linken (siehe Makefile-Kommentar).
rem ============================================================================

setlocal enabledelayedexpansion

echo === STRUDELFORCE MSVC Build (unified) ===

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

where cl >nul 2>&1
if errorlevel 1 (
    echo FEHLER: cl.exe nicht im Pfad gefunden. Bitte Visual Studio Command Prompt oeffnen.
    exit /b 1
)

set CFLAGS=/nologo /W3 /O2 /Isrc /D_CRT_SECURE_NO_WARNINGS

echo.
echo [1/7] Kompiliere strudelforce_game.exe (Terminal + Live Audio, Smith-Kern)...
cl %CFLAGS% src\game_main.c src\core_sim.c src\game_audio.c src\tfmx_player.c src\amiga_intro.c src\livecode_ui.c src\win_audio.c user32.lib gdi32.lib winmm.lib /Fe:strudelforce_game.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von strudelforce_game.exe
    exit /b 1
)

echo.
echo [2/7] Kompiliere strudelforce_v0.1.exe (HAUPT-VERSION: 2.5D Voxel GUI, Jukebox, Explorer, Smith-Kern)...
cl %CFLAGS% src\gui_main.c src\core_sim.c src\game_audio.c src\tfmx_player.c src\mod_player.c src\livecode_ui.c src\win_audio.c src\soundgodizer.c user32.lib gdi32.lib winmm.lib /Fe:strudelforce_v0.1.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von strudelforce_v0.1.exe
    exit /b 1
)

echo.
echo [3/7] Kompiliere test_audio_mix.exe (TFMX + MOD + SfMixer Harness, v3)...
cl %CFLAGS% src\test_audio_mix.c src\tfmx_player.c src\mod_player.c src\sf_mixer.c src\sid_header.c src\game_audio.c src\core_sim.c /Fe:test_audio_mix.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von test_audio_mix.exe
    exit /b 1
)

echo.
echo [4/7] Kompiliere strudelforce_test.exe (Phase-1 43-Check, sf-Kern core_sim_sf.c)...
rem /Isrc/compat: MSVC-SDK ohne native dirent.h/unistd.h -> Win32-Shims
cl %CFLAGS% /Isrc/compat src\test_phase1.c src\core_sim_sf.c src\cyber_paula.c src\voxel_render.c src\livecode_ui_sf.c src\biomes.c src\amiga_hw.c /Fe:strudelforce_test.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von strudelforce_test.exe
    exit /b 1
)

echo.
echo [5/7] Kompiliere strudelforce_headless.exe (Audio-Parser-Selftest)...
cl %CFLAGS% src\test_core_audio.c /Fe:strudelforce_headless.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von strudelforce_headless.exe
    exit /b 1
)

echo.
echo [6/7] Kompiliere win_audio_selftest.exe (Win-Audio-Transport v2 Ring/Thread)...
cl %CFLAGS% src\test_win_audio.c src\win_audio.c user32.lib gdi32.lib winmm.lib /Fe:win_audio_selftest.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von win_audio_selftest.exe
    exit /b 1
)

echo.
echo [7/7] Kompiliere test_embedded_music.exe (Selftest der In-EXE-Tracks)...
cl %CFLAGS% src\test_embedded_music.c src\tfmx_player.c src\mod_player.c /Fe:test_embedded_music.exe
if errorlevel 1 (
    echo FEHLER beim Kompilieren von test_embedded_music.exe
    exit /b 1
)

echo.
echo ============================================================================
echo ERFOLG: Alle 7 STRUDELFORCE Executables wurden erfolgreich gebaut!
echo   - strudelforce_game.exe      : 50Hz ANSI/ASCII Terminal-Spiel (Smith-Kern)
echo   - strudelforce_v0.1.exe      : HAUPT-VERSION zum Spielen (2.5D GUI, Jukebox, Embedded-Intro)
echo   - test_audio_mix.exe         : TFMX+MOD-Mixer Harness (--tfmx/--mod/--wav)
echo   - strudelforce_test.exe      : Phase-1 Integrationstest (sf-Kern)
echo   - strudelforce_headless.exe  : Audio-Parser-Selftest
echo   - win_audio_selftest.exe     : Audio-Transport v2 Selftest (Ring/Thread)
echo   - test_embedded_music.exe    : Selftest der In-EXE-Musik (Intro-Tracks)
echo ============================================================================
