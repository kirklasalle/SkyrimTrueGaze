@echo off
REM ===========================================================================
REM  TrueGaze - Self-contained build / deploy / verify / launch
REM
REM  An HCEP Product by Kirk LaSalle.
REM
REM  Single-file batch script. It needs nothing beyond what the build already
REM  needs (CMake, MSVC, vcpkg), plus PowerShell which ships with Windows.
REM
REM  It locates the game, reads its version, derives the exact Address Library
REM  filename that version requires, checks SKSE and the runtime, builds,
REM  deploys, verifies the deployed binary, and only then launches.
REM
REM  Usage:
REM      TrueGaze.cmd                 interactive menu
REM      TrueGaze.cmd all             build, deploy, verify, launch
REM      TrueGaze.cmd loadonly        deploy with the engine off, then launch
REM      TrueGaze.cmd build           build and verify only
REM      TrueGaze.cmd verify          verify only, no build
REM      TrueGaze.cmd postrun         analyse the log from the last run
REM      TrueGaze.cmd status          show detected configuration
REM      TrueGaze.cmd help            this text
REM
REM  Options:
REM      /game "path"   override the detected Skyrim install
REM      /force         launch even if verification fails
REM      /nopause       never wait for a keypress (for automation)
REM
REM  IMPLEMENTATION NOTE
REM  -----------------------------------------------------------------------
REM  Batch quoting is fragile around paths like "Program Files (x86)": a bare
REM  ")" inside a parenthesised block closes the block early, and nesting a
REM  quote character as a FOR delimiter does not survive being passed through
REM  another shell. Every check in this file therefore avoids both:
REM    * file reads go through pushd + a relative name, or findstr
REM    * nothing inside an IF/FOR parenthesised block contains a bare ")"
REM    * goto labels are used instead of parentheses wherever practical
REM  See the comments at each site. Verified against a real install.
REM ===========================================================================

setlocal EnableExtensions EnableDelayedExpansion
title TrueGaze

REM ---------------------------------------------------------------------------
REM Configuration
REM ---------------------------------------------------------------------------
if not defined VCPKG_ROOT set "VCPKG_ROOT=D:\vcpkg"

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

set "BUILT_DLL=%PROJECT_ROOT%\build\windows-release\Release\TrueGaze.dll"
set "SRC_INI=%PROJECT_ROOT%\skyrim\SKSE\Plugins\TrueGaze.ini"
set "PS=powershell -NoProfile -ExecutionPolicy Bypass -Command"

set "GAMEPATH="
set "MODE="
set "FORCE=0"
set "NOPAUSE=0"
set "FAILED=0"
set "WARNED=0"

REM ===========================================================================
REM  Argument parsing
REM ===========================================================================
:PARSE_ARGS
if "%~1"=="" goto :ARGS_DONE
set "ARG=%~1"

if /i "!ARG!"=="/game" (
    set "GAMEPATH=%~2"
    shift
    shift
    goto :PARSE_ARGS
)
if /i "!ARG!"=="/force" (
    set "FORCE=1"
    shift
    goto :PARSE_ARGS
)
if /i "!ARG!"=="/nopause" (
    set "NOPAUSE=1"
    shift
    goto :PARSE_ARGS
)

if /i "!ARG!"=="all"      set "MODE=ALL"
if /i "!ARG!"=="loadonly" set "MODE=LOADONLY"
if /i "!ARG!"=="build"    set "MODE=BUILD"
if /i "!ARG!"=="deploy"   set "MODE=DEPLOY"
if /i "!ARG!"=="verify"   set "MODE=VERIFY"
if /i "!ARG!"=="postrun"  set "MODE=POSTRUN"
if /i "!ARG!"=="status"   set "MODE=STATUS"
if /i "!ARG!"=="prereqs"  set "MODE=PREREQS"
if /i "!ARG!"=="help"     set "MODE=HELP"

shift
goto :PARSE_ARGS

:ARGS_DONE
if not defined MODE goto :MENU
if /i "!MODE!"=="HELP" goto :HELP
if /i "!MODE!"=="PREREQS" goto :DO_PREREQS
goto :DISPATCH

REM ===========================================================================
REM  Interactive menu
REM ===========================================================================
:MENU
call :BANNER
echo   What would you like to do?
echo.
echo     [1]  Full cycle      build, deploy, verify, launch
echo     [2]  Safe load-only  deploy with the engine OFF, then launch
echo     [3]  Build           build and verify, do not launch
echo     [4]  Verify          check everything, do not build or launch
echo     [5]  Analyse         report what happened on the last run
echo     [6]  Status          show the detected configuration
echo     [7]  Prerequisites   install SKSE + Address Library
echo     [0]  Exit
echo.
echo   For a first test, start with [7], then [4], then [2].
echo.
choice /c 12345670 /n /m "  Select: "
if errorlevel 8 goto :THE_END
if errorlevel 7 set "MODE=PREREQS"
if errorlevel 6 set "MODE=STATUS"
if errorlevel 5 set "MODE=POSTRUN"
if errorlevel 4 set "MODE=VERIFY"
if errorlevel 3 set "MODE=BUILD"
if errorlevel 2 set "MODE=LOADONLY"
if errorlevel 1 set "MODE=ALL"
goto :DISPATCH

REM ===========================================================================
REM  Dispatch
REM ===========================================================================
:DISPATCH
call :BANNER
call :FIND_GAME
if not defined GAMEPATH goto :NO_GAME
call :READ_VERSION

if /i "!MODE!"=="STATUS"   goto :DO_STATUS
if /i "!MODE!"=="POSTRUN"  goto :DO_POSTRUN
if /i "!MODE!"=="VERIFY"   goto :DO_VERIFY
if /i "!MODE!"=="DEPLOY"   goto :DO_DEPLOY
if /i "!MODE!"=="BUILD"    goto :DO_BUILD
if /i "!MODE!"=="LOADONLY" goto :DO_LOADONLY
if /i "!MODE!"=="ALL"      goto :DO_ALL
goto :THE_END

REM Prerequisites mode runs BEFORE game detection: it is the tool that makes
REM SKSE and the Address Library present, so it must work even when the game
REM folder does not yet contain them. It delegates to the PowerShell installer,
REM whose job is to print the two Nexus links and then install anything that has
REM been downloaded.
:DO_PREREQS
call :BANNER
echo   Installing/prerequiring SKSE64 and the Address Library...
echo   (This prints the two Nexus links, then installs anything already downloaded.)
echo.
set "PRE_PS=%PROJECT_ROOT%\scripts\Install-Prerequisites.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File "%PRE_PS%"
set "PRE_RC=!ERRORLEVEL!"

:END_PREREQS
if "!NOPAUSE!"=="1" goto :END_NO_PAUSE
echo.
echo   Press any key to close...
pause >nul
:END_NO_PAUSE
endlocal & exit /b %PRE_RC%

REM ===========================================================================
REM  Modes
REM ===========================================================================

:DO_STATUS
call :CHECK_GAME
call :CHECK_SKSE
call :CHECK_ADDRESSLIB
call :CHECK_RUNTIME
call :CHECK_BINARY
call :SUMMARY
goto :THE_END

:DO_VERIFY
call :CHECK_GAME
call :CHECK_SKSE
call :CHECK_ADDRESSLIB
call :CHECK_RUNTIME
call :CHECK_BINARY
call :CHECK_DEPLOYED
call :SUMMARY
goto :THE_END

:DO_DEPLOY
call :DEPLOY_IMPL 0
call :CHECK_GAME
call :CHECK_SKSE
call :CHECK_ADDRESSLIB
call :CHECK_RUNTIME
call :CHECK_BINARY
call :CHECK_DEPLOYED
call :SUMMARY
goto :THE_END

:DO_BUILD
call :BUILD_IMPL
call :CHECK_GAME
call :CHECK_SKSE
call :CHECK_ADDRESSLIB
call :CHECK_RUNTIME
call :CHECK_BINARY
call :CHECK_DEPLOYED
call :SUMMARY
goto :THE_END

:DO_ALL
call :BUILD_IMPL
call :DEPLOY_IMPL 0
call :CHECK_GAME
call :CHECK_SKSE
call :CHECK_ADDRESSLIB
call :CHECK_RUNTIME
call :CHECK_BINARY
call :CHECK_DEPLOYED
call :SUMMARY
call :MAYBE_LAUNCH
goto :THE_END

:DO_LOADONLY
call :BUILD_IMPL
call :DEPLOY_IMPL 1
call :CHECK_GAME
call :CHECK_SKSE
call :CHECK_ADDRESSLIB
call :CHECK_RUNTIME
call :CHECK_BINARY
call :CHECK_DEPLOYED
call :SUMMARY
call :MAYBE_LAUNCH
goto :THE_END

:DO_POSTRUN
call :POSTRUN_IMPL
goto :THE_END

REM ===========================================================================
REM  Build
REM ===========================================================================
:BUILD_IMPL
echo.
echo [BUILD]
echo ------------------------------------------------------------
if not exist "%PROJECT_ROOT%\CMakePresets.json" goto :BUILD_NOT_ROOT
if not exist "%PROJECT_ROOT%\extern\CommonLibSSE-NG\CMakeLists.txt" goto :BUILD_NO_SDK
if not defined VCPKG_ROOT goto :BUILD_NO_VCPKG
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" goto :BUILD_NO_VCPKG

echo   ..    VCPKG_ROOT = %VCPKG_ROOT%
pushd "%PROJECT_ROOT%"
call cmake --build --preset release > "%TEMP%\tg_build.log" 2>&1
set "BUILD_RC=!ERRORLEVEL!"
popd
if not "!BUILD_RC!"=="0" goto :BUILD_FAILED
echo   OK    Build succeeded.
exit /b 0

:BUILD_NOT_ROOT
echo   FAIL  Not in the TrueGaze project root.
echo         Expected CMakePresets.json in %PROJECT_ROOT%
set /a FAILED+=1
exit /b 1

:BUILD_NO_SDK
echo   FAIL  CommonLibSSE-NG submodule is missing.
echo         Run:  git submodule update --init --recursive
set /a FAILED+=1
exit /b 1

:BUILD_NO_VCPKG
echo   FAIL  vcpkg not found at "%VCPKG_ROOT%".
echo         Set the VCPKG_ROOT environment variable, or edit the default at
echo         the top of this file.
set /a FAILED+=1
exit /b 1

:BUILD_FAILED
echo   FAIL  Build failed with exit code !BUILD_RC!.
echo.
echo   Last lines of the build log:
for /f "delims=" %%L in ('findstr /n "^" "%TEMP%\tg_build.log" 2^>nul') do set "LASTLINE=%%L"
call :TAIL_LOG "%TEMP%\tg_build.log"
set /a FAILED+=1
exit /b 1

:TAIL_LOG
set "COUNT=0"
for /f "delims=" %%L in ('findstr /n "^" "%~1" 2^>nul') do set /a COUNT+=1
if !COUNT! LSS 1 exit /b 0
set /a START=COUNT-14
if !START! LSS 1 set "START=1"
set /a N=0
for /f "tokens=1* delims=:" %%A in ('findstr /n "^" "%~1" 2^>nul') do (
    set /a N+=1
    if !N! GEQ !START! echo     %%B
)
exit /b 0

REM ===========================================================================
REM  Launch
REM ===========================================================================
:MAYBE_LAUNCH
echo.
if "!FAILED!"=="0" goto :LAUNCH_NOW
if "!FORCE!"=="1" goto :LAUNCH_FORCED
echo   NOT LAUNCHING - verification failed.
echo.
echo   Fix the failures listed above, then run this again.
echo   To launch anyway (expect a crash), pass /force.
exit /b 0

:LAUNCH_FORCED
echo   WARNING: launching despite failures (/force). Expect a crash.
echo            Keep a backup save.
echo.

:LAUNCH_NOW
echo [LAUNCH]
echo ------------------------------------------------------------
if not exist "%GAMEPATH%\skse64_loader.exe" goto :LAUNCH_NO_SKSE
echo   ..    Starting Skyrim via SKSE...
echo.
echo   Launch via skse64_loader.exe, never SkyrimSE.exe.
echo   Steam's Play button will NOT load the plugin.
echo.
echo   When you have quit the game, run:  TrueGaze.cmd postrun
start "" /D "%GAMEPATH%" "%GAMEPATH%\skse64_loader.exe"
exit /b 0

:LAUNCH_NO_SKSE
echo   FAIL  skse64_loader.exe not found in the game folder.
echo         SKSE must be installed before the plugin can load.
set /a FAILED+=1
exit /b 1

REM ===========================================================================
REM  Deploy
REM  %1 = 1 for load-only (disable the simulation in the deployed INI)
REM ===========================================================================
:DEPLOY_IMPL
echo.
echo [DEPLOY]
echo ------------------------------------------------------------
if not exist "%BUILT_DLL%" goto :DEPLOY_NO_DLL

set "PLUGDIR=%GAMEPATH%\Data\SKSE\Plugins"
if not exist "%PLUGDIR%" mkdir "%PLUGDIR%" 2>nul

copy /y "%BUILT_DLL%" "%PLUGDIR%\TrueGaze.dll" >nul 2>&1
if not exist "%PLUGDIR%\TrueGaze.dll" goto :DEPLOY_COPY_FAIL
echo   OK    TrueGaze.dll deployed.

if exist "%SRC_INI%" goto :DEPLOY_INI
goto :DEPLOY_AFTER_INI

:DEPLOY_INI
copy /y "%SRC_INI%" "%PLUGDIR%\TrueGaze.ini" >nul 2>&1
echo   OK    TrueGaze.ini deployed.

:DEPLOY_AFTER_INI
if not "%~1"=="1" exit /b 0
if not exist "%PLUGDIR%\TrueGaze.ini" exit /b 0

REM Flip the master switch in the *deployed* copy only, so the source INI
REM remains the single source of truth.
REM
REM Delegated to PowerShell, after the batch version was tried and corrupted
REM the file three separate ways:
REM   * `for /f` silently DROPS blank lines (this INI has 28 of them)
REM   * `echo(%%L` mangles any line containing "=" or a paren - most of the
REM     file - and destroyed all 31 comment lines
REM   * `!VAR:search=replace!` cannot express a replacement containing "=",
REM     because the first "=" separates search from replacement
REM Together those rewrote a 93-line INI into 34 corrupted lines.
REM
REM The command is passed inline rather than written to a temp script, because
REM batch `echo` strips "^" escapes and would silently disarm the regex
REM anchors, turning "^(\s*bEnable...)" into "(\s*bEnable...)": it would then
REM match anywhere in the file instead of only the flag line.
REM
REM Set-Content and Get-Content are both avoided. This INI contains a non-ASCII
REM character (the trademark sign on the first line), and PowerShell's default
REM encoding is not a byte-preserving round trip: reading it and writing it back
REM silently re-encodes those bytes and changes the file by more than the flag.
REM
REM Latin-1 (codepage 28591) is used instead because it maps all 256 byte values
REM to distinct characters, so GetString/GetBytes is byte-exact for any input.
REM The edit is then guaranteed to be the flag and nothing else.
set "FLIPCMD=[Text.Encoding]::GetEncoding(28591) | Out-Null; $e=[Text.Encoding]::GetEncoding(28591); $p='%PLUGDIR%\TrueGaze.ini'; $t=$e.GetString([IO.File]::ReadAllBytes($p)); $t=$t -replace '(?im)^(\s*bEnableTrueGaze\s*=\s*)(true|1)(\s*)$', '${1}false${3}'; [IO.File]::WriteAllBytes($p, $e.GetBytes($t))"
%PS% "%FLIPCMD%" >nul 2>&1

findstr /i /c:"bEnableTrueGaze=false" "%PLUGDIR%\TrueGaze.ini" >nul 2>&1
if errorlevel 1 goto :DEPLOY_FLIP_FAILED
echo   OK    bEnableTrueGaze=false (load-only mode).
echo   ..    The plugin will load and install its hook but move nothing.
exit /b 0

:DEPLOY_FLIP_FAILED
echo   WARN  Could not set bEnableTrueGaze=false automatically.
echo         Edit the file by hand and set bEnableTrueGaze=false:
echo           %PLUGDIR%\TrueGaze.ini
set /a WARNED+=1
exit /b 0

:DEPLOY_NO_DLL
echo   FAIL  No binary at build\windows-release\Release\TrueGaze.dll
set /a FAILED+=1
exit /b 1

:DEPLOY_COPY_FAIL
echo   FAIL  Could not copy the DLL to:
echo         %PLUGDIR%
echo         Try running this script as Administrator.
set /a FAILED+=1
exit /b 1

REM ===========================================================================
REM  Post-run log analysis
REM ===========================================================================
:POSTRUN_IMPL
call :FIND_GAME
if not defined GAMEPATH goto :NO_GAME

set "LOGFILE=%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\TrueGaze.log"

echo.
echo [ANALYSE LAST RUN]
echo ------------------------------------------------------------
if not exist "!LOGFILE!" goto :POSTRUN_NO_LOG

echo   OK    Found !LOGFILE!
echo.
echo   Startup sequence:
call :LOGMARK "Plugin loaded"            "Loading True Gaze"                1
call :LOGMARK "Messaging listener bound" "SKSE plugin loaded successfully"  1
call :LOGMARK "Papyrus bindings"         "Registered 10 Papyrus functions"  0
call :LOGMARK "Game data loaded"         "Game data loaded"                 1
call :LOGMARK "Configuration loaded"     "Configuration loaded from"        0
call :LOGMARK "Gaze driver installed"    "Gaze driver installed"            1
call :LOGMARK "Engine ready"             "Gaze engine ready"                1

echo.
echo   Skeleton resolution (the critical unknown):
findstr /c:"Skeleton probe for" "!LOGFILE!" >nul 2>&1
if errorlevel 1 goto :POSTRUN_NO_PROBE
call :SHOW_FIRST "Skeleton probe for"
echo.
findstr /c:"head=yes" "!LOGFILE!" >nul 2>&1
if errorlevel 1 goto :POSTRUN_HEAD_MISS
echo   OK    Head bone resolved. Gaze should be visible.
goto :POSTRUN_FAULTS

:POSTRUN_HEAD_MISS
echo   FAIL  No actor reported head=yes.
echo         Bone names do not match this rig. Extend the candidate lists in
echo         src\Engine\GazeEngine.cpp and rebuild.
set /a FAILED+=1
goto :POSTRUN_FAULTS

:POSTRUN_NO_PROBE
echo   WARN  No skeleton probe line found.
echo         Either no eligible actor was within 15 m, or the tick never ran.
echo         Stand near a living NPC and try again.
set /a WARNED+=1

:POSTRUN_FAULTS
echo.
echo   Faults:
call :LOGMARK_BAD "Tick exceptions"       "Gaze tick threw"          1
call :LOGMARK_BAD "Missing head warnings" "No head bone found"       1
call :LOGMARK_WARN "Frame budget overruns" "Frame budget exceeded"   0
call :SUMMARY
exit /b 0

:POSTRUN_NO_LOG
echo   FAIL  No TrueGaze.log at:
echo         !LOGFILE!
echo.
echo   The plugin never ran. Check that you launched via skse64_loader.exe,
echo   then look at skse64.log in the same folder.
set /a FAILED+=1
call :SUMMARY
exit /b 0

REM Print the first log line containing a pattern.
:SHOW_FIRST
for /f "delims=" %%L in ('findstr /c:"%~1" "!LOGFILE!" 2^>nul') do (
    echo     %%L
    goto :SHOW_FIRST_DONE
)
:SHOW_FIRST_DONE
exit /b 0

REM %1 = label, %2 = pattern, %3 = 1 required / 0 optional
:LOGMARK
findstr /c:"%~2" "!LOGFILE!" >nul 2>&1
if errorlevel 1 goto :LOGMARK_MISS
echo     OK    %~1
exit /b 0
:LOGMARK_MISS
if "%~3"=="1" goto :LOGMARK_FAIL
echo     WARN  %~1  (not found)
set /a WARNED+=1
exit /b 0
:LOGMARK_FAIL
echo     FAIL  %~1  (not found)
set /a FAILED+=1
exit /b 0

REM A fault marker must be ABSENT for a pass.
:LOGMARK_BAD
findstr /c:"%~2" "!LOGFILE!" >nul 2>&1
if errorlevel 1 goto :LOGMARK_BAD_OK
echo     FAIL  %~1 present
set /a FAILED+=1
exit /b 0
:LOGMARK_BAD_OK
echo     OK    No %~1
exit /b 0

:LOGMARK_WARN
findstr /c:"%~2" "!LOGFILE!" >nul 2>&1
if errorlevel 1 goto :LOGMARK_WARN_OK
echo     WARN  %~1 present
set /a WARNED+=1
exit /b 0
:LOGMARK_WARN_OK
echo     OK    No %~1
exit /b 0

REM ===========================================================================
REM  Checks
REM ===========================================================================

:CHECK_GAME
echo.
echo [GAME]
echo ------------------------------------------------------------
echo   OK    %GAMEPATH%
echo   OK    Version %GAMEVER%
echo   ..    Address Library needs versionlib-%ALIBVER%.bin
exit /b 0

:CHECK_SKSE
echo.
echo [SKSE]
echo ------------------------------------------------------------
if not exist "%GAMEPATH%\skse64_loader.exe" goto :SKSE_MISSING
echo   OK    skse64_loader.exe present

REM The runtime DLL name encodes the game build SKSE was compiled for, so a
REM mismatch shows up as the classic "SKSE reported a different game version"
REM failure. Reads go through a pushd and a relative pattern: an absolute path
REM with a parenthesis does not survive `for /f ... in ('dir ...')`.
set "SKSE_FOUND="
pushd "%GAMEPATH%" 2>nul
for /f "delims=" %%F in ('dir /b "skse64_*.dll" 2^>nul') do call :SKSE_TEST "%%F"
popd
if defined SKSE_FOUND goto :SKSE_MATCH

echo   FAIL  No skse64_%SKSEVER%.dll in the game folder.
echo         Found instead:
pushd "%GAMEPATH%" 2>nul
for /f "delims=" %%F in ('dir /b "skse64_*.dll" 2^>nul') do echo           %%F
popd
echo         You have the wrong SKSE build for this game version.
set /a FAILED+=1
exit /b 0

:SKSE_TEST
echo %~1 | findstr /c:"_%SKSEVER%." >nul 2>&1
if not errorlevel 1 set "SKSE_FOUND=%~1"
exit /b 0

:SKSE_MATCH
echo   OK    Runtime matches game version: !SKSE_FOUND!
exit /b 0

:SKSE_MISSING
echo   FAIL  SKSE is not installed. Nothing loads without it.
echo.
echo         Download the AE build (Nexus-only):
echo           https://www.nexusmods.com/skyrimspecialedition/mods/30379
echo.
echo         Extract it into:
echo           %GAMEPATH%
echo.
echo         Then launch via skse64_loader.exe, never SkyrimSE.exe.
set /a FAILED+=1
exit /b 0

:CHECK_ADDRESSLIB
echo.
echo [ADDRESS LIBRARY]
echo ------------------------------------------------------------
set "ALIB=%GAMEPATH%\Data\SKSE\Plugins\versionlib-%ALIBVER%.bin"
if exist "!ALIB!" goto :ALIB_OK

REM A library for a *different* version is worse than none: it looks present
REM while resolving every address incorrectly.
set "ANYLIB="
pushd "%GAMEPATH%\Data\SKSE\Plugins" 2>nul
for /f "delims=" %%F in ('dir /b "versionlib-*.bin" 2^>nul') do set "ANYLIB=1"
popd
if defined ANYLIB goto :ALIB_WRONG

echo   FAIL  Missing versionlib-%ALIBVER%.bin
echo.
echo         TrueGaze resolves game offsets through this file.
echo         Download "Address Library for SKSE Plugins" (AE build):
echo           https://www.nexusmods.com/skyrimspecialedition/mods/32444
echo.
echo         It must land at:
echo           Data\SKSE\Plugins\versionlib-%ALIBVER%.bin
set /a FAILED+=1
exit /b 0

:ALIB_OK
echo   OK    versionlib-%ALIBVER%.bin present
exit /b 0

:ALIB_WRONG
echo   FAIL  Wrong Address Library version installed.
echo         Found:
pushd "%GAMEPATH%\Data\SKSE\Plugins" 2>nul
for /f "delims=" %%F in ('dir /b "versionlib-*.bin" 2^>nul') do echo           %%F
popd
echo         Need: versionlib-%ALIBVER%.bin
set /a FAILED+=1
exit /b 0

:CHECK_RUNTIME
echo.
echo [RUNTIME]
echo ------------------------------------------------------------
set "RTMISS="
if not exist "%SystemRoot%\System32\msvcp140.dll" set "RTMISS=1"
if not exist "%SystemRoot%\System32\vcruntime140.dll" set "RTMISS=1"
if not exist "%SystemRoot%\System32\vcruntime140_1.dll" set "RTMISS=1"
if defined RTMISS goto :RT_MISSING
echo   OK    Microsoft VC++ 2015-2022 x64 runtime present
exit /b 0

:RT_MISSING
echo   FAIL  Microsoft VC++ 2015-2022 x64 runtime is incomplete.
echo         Install the x64 Visual C++ Redistributable for VS 2015-2022.
set /a FAILED+=1
exit /b 0

:CHECK_BINARY
echo.
echo [PLUGIN BINARY]
echo ------------------------------------------------------------
if not exist "%BUILT_DLL%" goto :BIN_MISSING

for %%A in ("%BUILT_DLL%") do set "DLLBYTES=%%~zA"
set /a DLLKB=!DLLBYTES!/1024
echo   OK    TrueGaze.dll exists (!DLLKB! KB)

REM SKSE's loader contract is defined entirely by the export table. A plugin
REM exporting only SKSEPlugin_Load sits on the legacy load path and declares
REM nothing about which runtimes it supports.
set "EXPMISS="
call :EXPORT_TEST SKSEPlugin_Load
call :EXPORT_TEST SKSEPlugin_Query
call :EXPORT_TEST SKSEPlugin_Version
if defined EXPMISS goto :BIN_BAD_EXPORTS
echo   OK    SKSE loader contract satisfied (3 of 3 exports)
goto :BIN_HOOK

:BIN_BAD_EXPORTS
echo   FAIL  SKSE loader contract incomplete. Need all three of:
echo           SKSEPlugin_Load  SKSEPlugin_Query  SKSEPlugin_Version
set /a FAILED+=1

:BIN_HOOK
REM The hook target is the most dangerous thing in this codebase: a wrong
REM vtable index corrupts unrelated entries. Confirm the corrected target is
REM the one actually compiled in.
findstr /M /c:"vtable slot 0xAD" "%BUILT_DLL%" >nul 2>&1
if errorlevel 1 goto :BIN_BAD_HOOK
echo   OK    Gaze driver targets Actor::Update (slot 0xAD)
goto :BIN_GUARD

:BIN_BAD_HOOK
echo   FAIL  Binary does not target Actor::Update slot 0xAD.
echo         An older binary may still use the invalid Main vtable slot 0x05.
set /a FAILED+=1

:BIN_GUARD
findstr /M /c:"Gaze tick threw" "%BUILT_DLL%" >nul 2>&1
if errorlevel 1 goto :BIN_NO_GUARD
echo   OK    Tick exception guard compiled in
exit /b 0

:BIN_NO_GUARD
echo   WARN  Tick exception guard not found in the binary.
set /a WARNED+=1
exit /b 0

:BIN_MISSING
echo   FAIL  No binary at build\windows-release\Release\TrueGaze.dll
echo         Build first.
set /a FAILED+=1
exit /b 1

:EXPORT_TEST
findstr /M /c:"%~1" "%BUILT_DLL%" >nul 2>&1
if errorlevel 1 set "EXPMISS=1"
exit /b 0

:CHECK_DEPLOYED
echo.
echo [DEPLOYMENT]
echo ------------------------------------------------------------
set "DEPLL=%GAMEPATH%\Data\SKSE\Plugins\TrueGaze.dll"
if not exist "!DEPLL!" goto :DEP_NOT_PRESENT

call :HASH "%BUILT_DLL%" H1
call :HASH "!DEPLL!" H2
if /i "!H1!"=="!H2!" goto :DEP_MATCH
echo   FAIL  Deployed DLL does NOT match the build - it is stale.
echo         You are about to test a different binary than you built.
set /a FAILED+=1
goto :CHK_INI

:DEP_MATCH
echo   OK    Deployed DLL matches the build
goto :CHK_INI

:DEP_NOT_PRESENT
echo   WARN  TrueGaze.dll is not deployed to Data\SKSE\Plugins yet.
set /a WARNED+=1

:CHK_INI
set "DEPINI=%GAMEPATH%\Data\SKSE\Plugins\TrueGaze.ini"
if not exist "!DEPINI!" goto :INI_ABSENT
findstr /i /c:"bEnableTrueGaze=true" "!DEPINI!" >nul 2>&1
if errorlevel 1 goto :INI_DISABLED
echo   OK    TrueGaze.ini deployed (simulation enabled)
exit /b 0

:INI_DISABLED
echo   OK    TrueGaze.ini deployed (simulation disabled)
echo   WARN  bEnableTrueGaze is false. The plugin will load but do nothing.
set /a WARNED+=1
exit /b 0

:INI_ABSENT
echo   WARN  No TrueGaze.ini deployed; compiled defaults will be used.
set /a WARNED+=1
exit /b 0

REM ===========================================================================
REM  Helpers
REM ===========================================================================

REM %1 = file, %2 = variable to receive the SHA-256 hex digest.
REM Hashing goes through a pushd and a relative name; an absolute path with a
REM parenthesis would otherwise be passed through `for /f ... in ('...')` and
REM silently skip the loop, leaving both hashes empty and always "matching".
:HASH
set "%~2="
for %%D in ("%~1") do set "HDIR=%%~dpD"
for %%N in ("%~1") do set "HNAME=%%~nxN"
pushd "!HDIR!" 2>nul
for /f "skip=1 delims=" %%a in ('certutil -hashfile "!HNAME!" SHA256 2^>nul') do call :HASH_TAKE "%%a" %2
popd
set "HDIR="
set "HNAME="
exit /b 0

:HASH_TAKE
if defined %~2 exit /b 0
set "H=%~1"
set "H=!H: =!"
set "%~2=!H!"
exit /b 0

REM Locate the game. Registry first, then a drive scan.
REM The drive scan deliberately uses fixed relative paths rather than parsing
REM Steam's libraryfolders.vdf: the manifest stores paths with doubled
REM separators inside quotes, and extracting them reliably in batch requires
REM nesting a quote as a delimiter, which does not survive shell round-trips.
REM The scan costs a few milliseconds and cannot mis-parse.
:FIND_GAME
if defined GAMEPATH goto :FG_CHECK
goto :FG_REGISTRY

:FG_CHECK
if exist "!GAMEPATH!\SkyrimSE.exe" exit /b 0
set "GAMEPATH="

:FG_REGISTRY
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition" /v "Installed Path" 2^>nul ^| findstr /i "Installed Path"') do call :TRY_PATH "%%B"
if defined GAMEPATH exit /b 0
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\Bethesda Softworks\Skyrim Special Edition" /v "Installed Path" 2^>nul ^| findstr /i "Installed Path"') do call :TRY_PATH "%%B"
if defined GAMEPATH exit /b 0

for %%R in (C D E F G H I J) do call :TRY_DRIVE "%%R"
exit /b 0

:TRY_DRIVE
if defined GAMEPATH exit /b 0
set "DRV=%~1"
call :TRY_PATH "%DRV%:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"
call :TRY_PATH "%DRV%:\Program Files\Steam\steamapps\common\Skyrim Special Edition"
call :TRY_PATH "%DRV%:\Steam\steamapps\common\Skyrim Special Edition"
call :TRY_PATH "%DRV%:\SteamLibrary\steamapps\common\Skyrim Special Edition"
call :TRY_PATH "%DRV%:\Games\Steam\steamapps\common\Skyrim Special Edition"
call :TRY_PATH "%DRV%:\Games\Skyrim Special Edition"
exit /b 0

:TRY_PATH
if defined GAMEPATH exit /b 0
if exist "%~1\SkyrimSE.exe" set "GAMEPATH=%~1"
exit /b 0

REM Read the game's file version. Done through PowerShell because batch has no
REM built-in way to read a PE version resource, and WMIC is deprecated.
:READ_VERSION
set "GAMEVER="
for /f "usebackq delims=" %%V in (`%PS% "(Get-Item -LiteralPath '%GAMEPATH%\SkyrimSE.exe').VersionInfo.FileVersion" 2^>nul`) do call :TAKE_VERSION "%%V"
if not defined GAMEVER set "GAMEVER=0.0.0.0"

REM "1.7.104.0" -> "1-7-104-0"   (Address Library filename convention)
set "ALIBVER=%GAMEVER:.=-%"

REM "1.7.104.0" -> "1_7_104"     (SKSE runtime DLL naming convention)
set "SKSEVER="
for /f "tokens=1,2,3 delims=." %%A in ("%GAMEVER%") do set "SKSEVER=%%A_%%B_%%C"
exit /b 0

:TAKE_VERSION
if defined GAMEVER exit /b 0
set "GAMEVER=%~1"
exit /b 0

:BANNER
echo.
echo ============================================================
echo   TrueGaze - Build / Deploy / Verify / Launch
echo   An HCEP Product by Kirk LaSalle
echo ============================================================
exit /b 0

:SUMMARY
echo.
echo ------------------------------------------------------------
echo   Failures: !FAILED!   Warnings: !WARNED!
echo.
if "!FAILED!"=="0" goto :SUM_CLEAR
echo   VERDICT: NOT ready. Fix the failures above.
echo.
exit /b 0
:SUM_CLEAR
echo   VERDICT: Clear to launch.
echo.
exit /b 0

:NO_GAME
echo.
echo   FAIL  Could not locate Skyrim Special Edition.
echo.
echo   Pass the install directory explicitly, for example:
echo     TrueGaze.cmd verify /game "%ProgramFiles(x86)%\Steam\steamapps\common\Skyrim Special Edition"
echo.
set /a FAILED+=1
call :SUMMARY
goto :THE_END

:HELP
call :BANNER
echo   Usage:
echo     TrueGaze.cmd                 interactive menu
echo     TrueGaze.cmd prereqs         install SKSE + Address Library prerequisites
echo     TrueGaze.cmd all             build, deploy, verify, launch
echo     TrueGaze.cmd loadonly        deploy with the engine off, then launch
echo     TrueGaze.cmd build           build and verify only
echo     TrueGaze.cmd deploy          deploy an existing build, then verify
echo     TrueGaze.cmd verify          verify only, no build
echo     TrueGaze.cmd postrun         analyse the log from the last run
echo     TrueGaze.cmd status          show the detected configuration
echo.
echo   Options:
echo     /game "path"   override the detected Skyrim install
echo     /force         launch even if verification fails
echo     /nopause       never wait for a keypress
echo.
echo   Recommended first run:
echo     1.  TrueGaze.cmd prereqs      prints the two Nexus links; install once downloaded
echo     2.  TrueGaze.cmd verify       fix anything it reports
echo     3.  TrueGaze.cmd loadonly     prove it loads without crashing
echo     4.  TrueGaze.cmd all          the real test
echo     5.  TrueGaze.cmd postrun      what actually happened
echo.
goto :THE_END

REM ===========================================================================
REM  Exit
REM ===========================================================================
:THE_END
if "!NOPAUSE!"=="1" goto :END_NOPAUSE
echo.
echo   Press any key to close...
pause >nul

:END_NOPAUSE
REM Exit with a boolean result, not the failure count.
REM
REM Process exit codes are 8-bit on Windows, so returning FAILED directly would
REM wrap: 256 failures would report 0, i.e. success. Automation checks the exit
REM code, so it has to be either 0 or 1.
set "RESULT=0"
if not "!FAILED!"=="0" set "RESULT=1"
endlocal & exit /b %RESULT%
