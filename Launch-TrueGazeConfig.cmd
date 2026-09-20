@echo off
rem ============================================================================
rem  Launch-TrueGazeConfig.cmd - one-click entry to the TrueGaze config page
rem  with direct file access (no picker, no drag, no scan).
rem ============================================================================
setlocal
set "SCRIPT=%~dp0scripts\Launch-TrueGazeConfig.ps1"
if not exist "%SCRIPT%" (
    echo Launch script missing: %SCRIPT%
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%"
if errorlevel 1 pause
endlocal & exit /b 0