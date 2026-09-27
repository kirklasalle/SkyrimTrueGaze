@echo off
rem ============================================================================
rem  Launch-TrueGazeConfig.cmd - one-click entry to the TrueGaze config page
rem  with direct file access (no picker, no drag, no scan).
rem ============================================================================
setlocal
if exist "%~dp0scripts\Launch-TrueGazeConfig.ps1" (
    set "SCRIPT=%~dp0scripts\Launch-TrueGazeConfig.ps1"
) else if exist "%~dp0tools\TrueGazeConfig\Launch-TrueGazeConfig.ps1" (
    set "SCRIPT=%~dp0tools\TrueGazeConfig\Launch-TrueGazeConfig.ps1"
) else (
    echo TrueGaze Launch script missing!
    echo Looked for:
    echo   %~dp0scripts\Launch-TrueGazeConfig.ps1
    echo   %~dp0tools\TrueGazeConfig\Launch-TrueGazeConfig.ps1
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%"
if errorlevel 1 pause
endlocal & exit /b 0