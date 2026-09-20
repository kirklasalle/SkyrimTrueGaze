@echo off
setlocal enabledelayedexpansion
title TrueGaze Pre-Flight and Launch
color 0B

:: ============================================================================
::  TrueGaze Pre-Flight Check and Launch
::  Author: Kirk LaSalle
::  Build:  f2c8e50
:: ============================================================================

set "SKYRIM=G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"
set "PROJECT=D:\Projects\SkyrimTrueGaze"
set "PLUGINS=G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins"
set PASS=0
set FAIL=0

echo.
echo  ============================================================
echo   TrueGaze Pre-Flight Check and Launch
echo   %date% %time%
echo  ============================================================
echo.

tasklist /fi "imagename eq SkyrimSE.exe" 2>nul | find /i "SkyrimSE.exe" >nul
if not errorlevel 1 (
    color 0E
    echo.
    echo  ============================================================
    echo   WARNING: SkyrimSE.exe is currently running!
    echo   Please exit Skyrim first so new files can be deployed.
    echo  ============================================================
    echo.
    echo  Press any key once Skyrim is closed to proceed with checks...
    pause >nul
    color 07
    echo.
)

:: ---- Check 1: Skyrim Installation ----
echo  [1/8] Skyrim Installation...
if exist "%SKYRIM%\SkyrimSE.exe" (
    echo        PASS - SkyrimSE.exe found
    set /a PASS+=1
) else (
    echo        FAIL - SkyrimSE.exe not found
    set /a FAIL+=1
)

:: ---- Check 2: SKSE Loader ----
echo  [2/8] SKSE Loader...
if exist "%SKYRIM%\skse64_loader.exe" (
    echo        PASS - skse64_loader.exe found
    set /a PASS+=1
) else (
    echo        FAIL - skse64_loader.exe not found
    set /a FAIL+=1
)

:: ---- Check 3: SKSE Runtime DLL ----
echo  [3/8] SKSE Runtime...
if exist "%SKYRIM%\skse64_1_7_104.dll" (
    echo        PASS - skse64_1_7_104.dll found
    set /a PASS+=1
) else (
    if exist "%SKYRIM%\skse64_1_6_1170.dll" (
        echo        PASS - skse64_1_6_1170.dll found
        set /a PASS+=1
    ) else (
        echo        FAIL - No matching SKSE runtime DLL
        set /a FAIL+=1
    )
)

:: ---- Check 4: Address Library ----
echo  [4/8] Address Library...
if exist "%PLUGINS%\versionlib-1-7-104-0.bin" (
    echo        PASS - versionlib-1-7-104-0.bin found
    set /a PASS+=1
) else (
    echo        FAIL - Address Library not found
    set /a FAIL+=1
)

:: ---- Check 5: Plugins Directory ----
echo  [5/8] Plugins Directory...
if exist "%PLUGINS%\" (
    echo        PASS - Plugins directory exists
    set /a PASS+=1
) else (
    mkdir "%PLUGINS%" 2>nul
    if exist "%PLUGINS%\" (
        echo        PASS - Created plugins directory
        set /a PASS+=1
    ) else (
        echo        FAIL - Cannot create plugins directory
        set /a FAIL+=1
    )
)

:: ---- Check 6: Deploy TrueGaze.dll ----
echo  [6/8] TrueGaze.dll...
set "BUILD_DLL=%PROJECT%\build\windows-release\Release\TrueGaze.dll"
set "REPO_DLL=%PROJECT%\skyrim\SKSE\Plugins\TrueGaze.dll"
set "DEST_DLL=%PLUGINS%\TrueGaze.dll"

if exist "%BUILD_DLL%" (
    echo        Source: build output
    copy /y "%BUILD_DLL%" "%DEST_DLL%" >nul 2>&1
) else (
    if exist "%REPO_DLL%" (
        echo        Source: repo copy
        copy /y "%REPO_DLL%" "%DEST_DLL%" >nul 2>&1
    )
)

if exist "%DEST_DLL%" (
    for %%F in ("%DEST_DLL%") do (
        set /a DLL_KB=%%~zF / 1024
    )
    echo        PASS - TrueGaze.dll deployed [!DLL_KB! KB]
    if !DLL_KB! LSS 100 (
        echo        WARN - DLL is very small, may be a skeleton build
    )
    set /a PASS+=1
) else (
    echo        FAIL - TrueGaze.dll not found anywhere
    set /a FAIL+=1
)

:: ---- Check 7: Deploy TrueGaze.ini ----
echo  [7/8] TrueGaze.ini...
set "REPO_INI=%PROJECT%\skyrim\SKSE\Plugins\TrueGaze.ini"
set "DEST_INI=%PLUGINS%\TrueGaze.ini"

if exist "%DEST_INI%" (
    echo        PASS - TrueGaze.ini already present
    set /a PASS+=1
) else (
    if exist "%REPO_INI%" (
        copy /y "%REPO_INI%" "%DEST_INI%" >nul 2>&1
        echo        PASS - TrueGaze.ini deployed
        set /a PASS+=1
    ) else (
        echo        FAIL - TrueGaze.ini not found
        set /a FAIL+=1
    )
)

:: ---- Check 8: INI Enabled ----
echo  [8/8] INI Settings...
if exist "%DEST_INI%" (
    findstr /i "bEnableTrueGaze=true" "%DEST_INI%" >nul 2>&1
    if !errorlevel! equ 0 (
        echo        PASS - bEnableTrueGaze=true
    ) else (
        echo        WARN - bEnableTrueGaze may not be set to true
    )
    set /a PASS+=1
) else (
    echo        SKIP - No INI to check
    set /a PASS+=1
)

:: ---- Configuration is INI-only (vanilla UI) ----
:: TrueGaze has no ESP, no Papyrus script, no MCM menu and no SkyUI dependency.
:: Clean up stale files from old installs so nothing can linger and confuse the
:: game or the logs. Configuration lives in Data\SKSE\Plugins\TrueGaze.ini and is
:: edited through TrueGazeConfig.html.
if exist "%SKYRIM%\Data\TrueGaze.esp" del /q "%SKYRIM%\Data\TrueGaze.esp" >nul 2>&1
if exist "%SKYRIM%\Data\TrueGaze.esl" del /q "%SKYRIM%\Data\TrueGaze.esl" >nul 2>&1
if exist "%SKYRIM%\Data\TrueGaze_MCM.pex" del /q "%SKYRIM%\Data\TrueGaze_MCM.pex" >nul 2>&1
if exist "%SKYRIM%\Data\TrueGaze.pex" del /q "%SKYRIM%\Data\TrueGaze.pex" >nul 2>&1
if exist "%SKYRIM%\Data\MCM\Config\TrueGaze" rd /s /q "%SKYRIM%\Data\MCM\Config\TrueGaze" >nul 2>&1
if exist "%SKYRIM%\Data\Interface\MCM\Config\TrueGaze" rd /s /q "%SKYRIM%\Data\Interface\MCM\Config\TrueGaze" >nul 2>&1
if exist "%SKYRIM%\Data\Interface\Translations\TrueGaze_*.txt" del /q "%SKYRIM%\Data\Interface\Translations\TrueGaze_*.txt" >nul 2>&1

:: Resolve user Documents directory for SKSE log checking
set "DOCS="
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')" 2^>nul`) do if not defined DOCS set "DOCS=%%D"
if not defined DOCS set "DOCS=%USERPROFILE%\Documents"
set "SKSE_LOG=%DOCS%\My Games\Skyrim Special Edition\SKSE\TrueGaze.log"

:: ============================================================================
::  RESULTS
:: ============================================================================
echo.
echo  ============================================================
if !FAIL! GTR 0 (
    color 0C
    echo   PREFLIGHT FAILED: !PASS! passed, !FAIL! failed
    echo.
    echo   Fix the failures above and run this script again.
    echo  ============================================================
    echo.
    pause
    exit /b 1
)

color 0A
echo   ALL !PASS! CHECKS PASSED - READY TO LAUNCH
echo  ============================================================
echo.
echo  After launch, check the log at:
echo    "%SKSE_LOG%"
echo.
echo  To disable TrueGaze without uninstalling:
echo    Set bEnableTrueGaze=false in TrueGaze.ini
echo    Or rename TrueGaze.dll to TrueGaze.dll.bak
echo.
echo  ============================================================
echo   Launching Skyrim via SKSE in 5 seconds...
echo   Close this window to cancel.
echo  ============================================================
echo.

timeout /t 5 /nobreak

echo.
echo  Starting SKSE (working directory: %SKYRIM%)...
pushd "%SKYRIM%"
start "" /D "%SKYRIM%" "%SKYRIM%\skse64_loader.exe"
popd

echo  Activating SkyrimSE window into active foreground...
:: The Win32 foreground activator lives in its own script. Embedding C# inline
:: in a batch `powershell -Command` argument is unparseable by cmd.exe: each
:: source line leaks and runs as a batch command, and the '@ here-string never
:: terminates. Invoking a real .ps1 with -File avoids every quoting boundary.
set "ACTIVATOR=%PROJECT%\scripts\Activate-TrueGazeWindow.ps1"
if exist "!ACTIVATOR!" (
    start /b "" powershell -NoProfile -ExecutionPolicy Bypass -File "!ACTIVATOR!"
) else (
    echo        WARN - Window activator not found: !ACTIVATOR!
)

echo.
echo  ============================================================
echo   Skyrim is launching. After you reach the main menu:
echo.
echo   1. Load a save near NPCs
echo   2. Walk up to an NPC within 3 metres
echo   3. Watch their eyes
echo.
echo   When done testing, check:
echo     "%SKSE_LOG%"
echo  ============================================================
echo.
pause
