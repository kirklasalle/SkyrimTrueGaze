@echo off
setlocal enabledelayedexpansion
title TrueGaze Pre-Flight and Launch
color 0B

:: ============================================================================
::  TrueGaze Pre-Flight Check and Launch
::  Author: Kirk LaSalle
::  Build:  f2c8e50
:: ============================================================================

set "PROJECT=%~dp0"
if "%PROJECT:~-1%"=="\" set "PROJECT=%PROJECT:~0,-1%"
set "SKYRIM="
for /f "usebackq delims=" %%G in (`powershell -NoProfile -Command "foreach ($k in 'HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition','HKLM:\SOFTWARE\Bethesda Softworks\Skyrim Special Edition') { if (Test-Path $k) { $p=(Get-ItemProperty $k -Name 'Installed Path' -EA 0).'Installed Path'; if ($p -and (Test-Path (Join-Path $p 'SkyrimSE.exe'))) { $p.TrimEnd('\'); break } } }" 2^>nul`) do if not defined SKYRIM set "SKYRIM=%%G"
if not defined SKYRIM if defined SKYRIM_PATH if exist "%SKYRIM_PATH%\SkyrimSE.exe" set "SKYRIM=%SKYRIM_PATH%"
if not defined SKYRIM if exist "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\SkyrimSE.exe" set "SKYRIM=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"
set "PLUGINS=%SKYRIM%\Data\SKSE\Plugins"
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
echo  [1/9] Skyrim Installation...
if exist "%SKYRIM%\SkyrimSE.exe" (
    echo        PASS - SkyrimSE.exe found
    set /a PASS+=1
) else (
    echo        FAIL - SkyrimSE.exe not found
    set /a FAIL+=1
)

:: ---- Check 2: SKSE Loader ----
echo  [2/9] SKSE Loader...
if exist "%SKYRIM%\skse64_loader.exe" (
    echo        PASS - skse64_loader.exe found
    set /a PASS+=1
) else (
    echo        FAIL - skse64_loader.exe not found
    set /a FAIL+=1
)

:: ---- Check 3: SKSE Runtime DLL ----
echo  [3/9] SKSE Runtime...
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
echo  [4/9] Address Library...
if exist "%PLUGINS%\versionlib-1-7-104-0.bin" (
    echo        PASS - versionlib-1-7-104-0.bin found
    set /a PASS+=1
) else if exist "%PLUGINS%\versionlib-1-6-1170-0.bin" (
    echo        PASS - versionlib-1-6-1170-0.bin found
    set /a PASS+=1
) else if exist "%PLUGINS%\versionlib-*.bin" (
    echo        PASS - Address Library bin found
    set /a PASS+=1
) else (
    echo        FAIL - Address Library not found
    set /a FAIL+=1
)

:: ---- Check 5: Plugins Directory ----
echo  [5/9] Plugins Directory...
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
echo  [6/9] TrueGaze.dll...
:: Prefer the optimized Windows Release plugin, then the repo copy
set "RELEASE_DLL=%PROJECT%\build\windows-release\Release\TrueGaze.dll"
set "DEBUG_DLL=%PROJECT%\build\windows-debug\Debug\TrueGaze.dll"
set "REPO_DLL=%PROJECT%\skyrim\SKSE\Plugins\TrueGaze.dll"
set "DEST_DLL=%PLUGINS%\TrueGaze.dll"

if exist "%RELEASE_DLL%" (
    echo        Source: current Release build output
    copy /y "%RELEASE_DLL%" "%DEST_DLL%" >nul 2>&1
) else if exist "%DEBUG_DLL%" (
    echo        Source: current Debug build output
    copy /y "%DEBUG_DLL%" "%DEST_DLL%" >nul 2>&1
) else if exist "%REPO_DLL%" (
    echo        Source: repo copy
    copy /y "%REPO_DLL%" "%DEST_DLL%" >nul 2>&1
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
echo  [7/9] TrueGaze.ini...
set "REPO_INI=%PROJECT%\skyrim\SKSE\Plugins\TrueGaze.ini"
set "DEST_INI=%PLUGINS%\TrueGaze.ini"

if not exist "%REPO_INI%" goto :INI_SOURCE_MISSING
if exist "%DEST_INI%" goto :INI_PRESERVED
copy /y "%REPO_INI%" "%DEST_INI%" >nul 2>&1
if errorlevel 1 goto :INI_COPY_FAILED
echo        PASS - TrueGaze.ini deployed (first install)
set /a PASS+=1
goto :INI_DONE

:INI_PRESERVED
echo        PASS - Existing TrueGaze.ini preserved
set /a PASS+=1
goto :INI_DONE

:INI_SOURCE_MISSING
if exist "%DEST_INI%" goto :INI_PRESERVED
echo        FAIL - TrueGaze.ini not found
set /a FAIL+=1
goto :INI_DONE

:INI_COPY_FAILED
echo        FAIL - Could not deploy TrueGaze.ini
set /a FAIL+=1

:INI_DONE

:: ---- Check 8: Deploy mesh and texture assets ----
echo  [8/9] TrueGaze Assets...
set "SRC_MESHES=%PROJECT%\skyrim\meshes\TrueGaze"
set "SRC_TEXTURES=%PROJECT%\skyrim\textures\TrueGaze"
set "DEST_MESHES=%SKYRIM%\Data\meshes\TrueGaze"
set "DEST_TEXTURES=%SKYRIM%\Data\textures\TrueGaze"

if not exist "%DEST_MESHES%" mkdir "%DEST_MESHES%" >nul 2>&1
if not exist "%DEST_TEXTURES%" mkdir "%DEST_TEXTURES%" >nul 2>&1

set ASSET_OK=1
echo        Primary beam uses Skyrim's native BSA meshes\marker_arrow.nif (SEC-01 clean)
if exist "%SRC_MESHES%\GazeBeam.nif" (
    copy /y "%SRC_MESHES%\GazeBeam.nif" "%DEST_MESHES%\GazeBeam.nif" >nul 2>&1
    echo        Deployed GazeBeam.nif
) else (
    echo        WARN - GazeBeam.nif not found in repo
    set ASSET_OK=0
)
if exist "%SRC_MESHES%\GazeRegionPanel.nif" (
    copy /y "%SRC_MESHES%\GazeRegionPanel.nif" "%DEST_MESHES%\GazeRegionPanel.nif" >nul 2>&1
    echo        Deployed GazeRegionPanel.nif [development panel]
) else (
    echo        FAIL - GazeRegionPanel.nif not found in repo
    set ASSET_OK=0
)
if exist "%SRC_TEXTURES%\GazeBeamGlow.dds" (
    copy /y "%SRC_TEXTURES%\GazeBeamGlow.dds" "%DEST_TEXTURES%\GazeBeamGlow.dds" >nul 2>&1
    echo        Deployed GazeBeamGlow.dds
) else (
    echo        FAIL - GazeBeamGlow.dds not found in repo
    set ASSET_OK=0
)
if exist "%SRC_TEXTURES%\GazeRegionPanel.dds" (
    copy /y "%SRC_TEXTURES%\GazeRegionPanel.dds" "%DEST_TEXTURES%\GazeRegionPanel.dds" >nul 2>&1
    echo        Deployed GazeRegionPanel.dds
) else (
    echo        FAIL - GazeRegionPanel.dds not found in repo
    set ASSET_OK=0
)


if not exist "%SKYRIM%\Data\Interface" mkdir "%SKYRIM%\Data\Interface" >nul 2>&1
if not exist "%PROJECT%\skyrim\Interface\TrueGazeIncoming.swf" (
    echo        FAIL - Incoming UI missing; rebuild the current Release target.
    set /a FAIL+=1
) else (
    copy /y "%PROJECT%\skyrim\Interface\TrueGazeIncoming.swf" "%SKYRIM%\Data\Interface\TrueGazeIncoming.swf" >nul 2>&1
    if !errorlevel! neq 0 (
        echo        FAIL - Cannot deploy incoming UI.
        set /a FAIL+=1
    ) else (
        echo        Deployed TrueGazeIncoming.swf
    )
)
if exist "%PROJECT%\skyrim\Interface\TrueGazeReticle.swf" (
    copy /y "%PROJECT%\skyrim\Interface\TrueGazeReticle.swf" "%SKYRIM%\Data\Interface\TrueGazeReticle.swf" >nul 2>&1
    echo        Deployed TrueGazeReticle.swf
)
if exist "%PROJECT%\skyrim\TrueGaze_ExpandedPlay.esp" (
    copy /y "%PROJECT%\skyrim\TrueGaze_ExpandedPlay.esp" "%SKYRIM%\Data\TrueGaze_ExpandedPlay.esp" >nul 2>&1
    echo        Deployed TrueGaze_ExpandedPlay.esp
)
if exist "%PROJECT%\skyrim\SEQ\TrueGaze_ExpandedPlay.seq" (
    if not exist "%SKYRIM%\Data\SEQ" mkdir "%SKYRIM%\Data\SEQ" >nul 2>&1
    copy /y "%PROJECT%\skyrim\SEQ\TrueGaze_ExpandedPlay.seq" "%SKYRIM%\Data\SEQ\TrueGaze_ExpandedPlay.seq" >nul 2>&1
    echo        Deployed TrueGaze_ExpandedPlay.seq
)
if exist "%PROJECT%\skyrim\Sound\Voice\TrueGaze_ExpandedPlay.esp" (
    xcopy /s /e /y /q "%PROJECT%\skyrim\Sound\Voice\TrueGaze_ExpandedPlay.esp" "%SKYRIM%\Data\Sound\Voice\TrueGaze_ExpandedPlay.esp\" >nul 2>&1
    echo        Deployed clean-room voice timing WAVs
)

if !ASSET_OK! equ 1 (
    echo        PASS - TrueGaze assets deployed
    set /a PASS+=1
) else (
    echo        WARN - Some assets missing; fallback rendering will be used
    set /a PASS+=1
)

:: ---- Check 9: INI Enabled ----
echo  [9/9] INI Settings...
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
