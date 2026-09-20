@echo off
REM ===========================================================================
REM  TrueGaze - Install ALL prerequisites
REM
REM  Batch entry point. All real work is done by
REM  scripts\Install-AllPrerequisites.ps1 - cmd.exe cannot parse PowerShell
REM  reliably (a multi-line quoted -Command argument leaks every source line
REM  into cmd.exe as a command; see the project's audit notes).
REM
REM  This wrapper only:
REM    * finds a PowerShell that works,
REM    * forwards its arguments,
REM    * pauses when double-clicked so the result is readable.
REM
REM  Usage:
REM    Install-AllPrerequisites.bat                install everything
REM    Install-AllPrerequisites.bat -Verify        report only, install nothing
REM    Install-AllPrerequisites.bat -SkipGame      build toolchain only
REM    Install-AllPrerequisites.bat -NoPersist     do not set VCPKG_ROOT
REM    Install-AllPrerequisites.bat -GamePath "G:\...\Skyrim Special Edition"
REM
REM  An HCEP Product by Kirk LaSalle.
REM ===========================================================================

setlocal EnableExtensions
set "PROJECT_ROOT=%~dp0"
set "PS_SCRIPT=%PROJECT_ROOT%scripts\Install-AllPrerequisites.ps1"

title TrueGaze - Install All Prerequisites

REM ---------------------------------------------------------------------------
REM  Split our own switches from the arguments PowerShell understands.
REM
REM  -NoPause belongs to this wrapper only; forwarding it to the PowerShell
REM  script would fail with "cannot be found that matches parameter name".
REM  Every other argument is passed through untouched, re-quoted so a value
REM  containing spaces (e.g. a "Program Files (x86)" game path) survives.
REM ---------------------------------------------------------------------------
set "FORWARD="
set "HAD_ARGS="
set "NOPAUSE="

REM  IMPORTANT: this loop must NOT use a parenthesised IF/ELSE block.
REM  A game path contains "(x86)", and a bare ")" inside such a block closes
REM  it early, truncating the argument at "(x86" and aborting the script.
REM  Label-goto is the only safe structure here.
:PARSE_ARGS
if "%~1"=="" goto :ARGS_DONE
set "HAD_ARGS=1"
if /i "%~1"=="-NoPause" goto :PARSE_NOPAUSE
set "FORWARD=%FORWARD% "%~1""
shift
goto :PARSE_ARGS

:PARSE_NOPAUSE
set "NOPAUSE=1"
shift
goto :PARSE_ARGS

:ARGS_DONE

REM ---------------------------------------------------------------------------
REM  Banner
REM ---------------------------------------------------------------------------
echo.
echo ==============================================================
echo   TrueGaze - Complete Prerequisite Installer
echo   Build toolchain + vcpkg + SDK + game prerequisites
echo   An HCEP Product by Kirk LaSalle
echo ==============================================================
echo.

REM ---------------------------------------------------------------------------
REM  The script must exist. Check before doing anything else.
REM ---------------------------------------------------------------------------
if not exist "%PS_SCRIPT%" goto :NO_SCRIPT

REM ---------------------------------------------------------------------------
REM  Find PowerShell.
REM
REM  Prefer powershell.exe (Windows PowerShell 5.1, always present). Fall back
REM  to pwsh.exe (PowerShell 7) if 5.1 is somehow unavailable. Both can run the
REM  script - it uses no version-specific syntax for the 5.1 path.
REM ---------------------------------------------------------------------------
set "PS_EXE="
where powershell.exe >nul 2>&1 && set "PS_EXE=powershell.exe"
if not defined PS_EXE where pwsh.exe >nul 2>&1 && set "PS_EXE=pwsh.exe"

if not defined PS_EXE goto :NO_POWERSHELL

echo   Using                  %PS_EXE%
echo   Script                 %PS_SCRIPT%
REM  Label-goto again, for the same ")" reason as the parse loop: FORWARD can
REM  hold a path containing "(x86)".
if not defined FORWARD goto :SHOW_MODE_ALL
echo   Arguments              %FORWARD%
goto :AFTER_MODE
:SHOW_MODE_ALL
echo   Mode                   install ^(all^)
:AFTER_MODE
echo.

REM ---------------------------------------------------------------------------
REM  Elevation hint (the PowerShell script re-checks and reports precisely).
REM ---------------------------------------------------------------------------
net session >nul 2>&1
if errorlevel 1 (
    echo   Note: this terminal is NOT elevated. Machine-wide installs
    echo   ^(Visual Studio Build Tools, VC++ Redistributable^) will be
    echo   skipped. Close this and re-run as administrator if they are
    echo   reported missing.
    echo.
)

REM ---------------------------------------------------------------------------
REM  Run the installer.
REM
REM  -NoProfile keeps a user profile from altering the environment mid-run.
REM  -ExecutionPolicy Bypass is scoped to this process only.
REM
REM  Exit codes: 0 = everything installed, 1 = something still missing.
REM ---------------------------------------------------------------------------
%PS_EXE% -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %FORWARD%
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
    echo   RESULT: success.
) else (
    echo   RESULT: incomplete ^(exit %RC%^). Address the FAIL lines above.
    echo           The most common cause is a missing Nexus download - the
    echo           script prints the exact URL and save path for each.
)

REM ---------------------------------------------------------------------------
REM  Pause only when double-clicked (no arguments, and -NoPause not given), so
REM  automation never hangs.
REM ---------------------------------------------------------------------------
if not defined NOPAUSE if not defined HAD_ARGS (
    echo.
    echo   Press any key to close...
    pause >nul
)

endlocal & exit /b %RC%

REM ===========================================================================
REM  Error paths
REM ===========================================================================

:NO_SCRIPT
echo   FAIL  Installer script not found:
echo         %PS_SCRIPT%
echo.
echo   Run this batch file from the repository root, or restore the
echo   scripts\ folder from source control.
set "RC=1"
goto :END_PAUSE

:NO_POWERSHELL
echo   FAIL  No PowerShell found.
echo.
echo   Windows PowerShell 5.1 ships with Windows. If it is missing, the
echo   installation is unusual - repair it, or install PowerShell 7.
set "RC=1"
goto :END_PAUSE

:END_PAUSE
if not defined NOPAUSE if not defined HAD_ARGS (
    echo.
    echo   Press any key to close...
    pause >nul
)
endlocal & exit /b %RC%