<#
.SYNOPSIS
    Installs EVERY prerequisite needed to build and run TrueGaze.

.DESCRIPTION
    TrueGaze.cmd prereqs covers only SKSE64 and the Address Library (the two
    Nexus-only mod files). This script covers the whole machine instead: the
    build toolchain, vcpkg, the SDK submodule, and then those two game files.

    Prerequisites, in dependency order:

      1. Windows PowerShell      (ships with Windows - only checked)
      2. winget                  (the package installer this script drives)
      3. Git                     (vcpkg + the CommonLibSSE-NG submodule)
      4. CMake                   (build system; needs >= 3.23)
      5. 7-Zip                   (MANDATORY - Windows tar cannot read LZMA 7z,
                                  and both game archives are 7z)
      6. Visual Studio Build Tools 2022 + C++ workload
      7. Microsoft VC++ 2015-2022 x64 Redistributable
      8. vcpkg at x64-windows-static-md  (bootstrapped, VCPKG_ROOT persisted)
      9. extern/CommonLibSSE-NG submodule
     10. SKSE64 + Address Library       (Nexus-only; see the manual step below)

    ONE STEP CANNOT BE AUTOMATED. SKSE64 and the Address Library are distributed
    on Nexus Mods, which requires a logged-in account, and the Address Library's
    permissions forbid redistribution. Nothing may download them for you. This
    script detects them, and if they are absent it prints the exact URL and save
    path, then installs and verifies them on the next run.

.PARAMETER Verify
    Probe only. Install nothing. Reports what is present and what is missing.

.PARAMETER SkipGame
    Skip the SKSE64 / Address Library step (useful on a build-only machine).

.PARAMETER VcpkgPath
    Where vcpkg lives or should be cloned. Default: $env:VCPKG_ROOT, else D:\vcpkg.

.PARAMETER GamePath
    Skyrim install directory. Auto-detected when omitted.

.PARAMETER NoPersist
    Do not write VCPKG_ROOT to the user environment. Use when you want a
    session-only install (e.g. a CI runner).

.EXAMPLE
    .\Install-AllPrerequisites.ps1
    .\Install-AllPrerequisites.ps1 -Verify
    .\Install-AllPrerequisites.ps1 -SkipGame

.NOTES
    Part of TrueGaze(TM), an HCEP product by Kirk LaSalle.
#>

[CmdletBinding()]
param(
    [switch]$Verify,
    [switch]$SkipGame,
    [switch]$NoPersist,
    [string]$VcpkgPath,
    [string]$GamePath
)

# Do NOT use 'Stop' here: native tools (winget, cmake, git) write normal progress
# to stderr, and under 'Stop' that is promoted to a terminating error - the exact
# false-failure trap documented in this project's audit notes.
$ErrorActionPreference = 'Continue'

$script:Failures = 0
$script:Warnings = 0

function Write-Step { param([string]$t) Write-Host ''; Write-Host $t -ForegroundColor Cyan; Write-Host ('-' * 66) -ForegroundColor DarkGray }
function Write-Ok   { param([string]$t) Write-Host ("  OK    " + $t) -ForegroundColor Green }
function Write-Bad  { param([string]$t) Write-Host ("  FAIL  " + $t) -ForegroundColor Red; $script:Failures++ }
function Write-Warn { param([string]$t) Write-Host ("  WARN  " + $t) -ForegroundColor Yellow; $script:Warnings++ }
function Write-Info { param([string]$t) Write-Host ("  ..    " + $t) -ForegroundColor DarkGray }
function Write-Note { param([string]$t) Write-Host ("        " + $t) -ForegroundColor DarkCyan }

function Test-Command {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-ToolVersion {
    # NOTE: the parameter must NOT be called $Args - that is a PowerShell
    # automatic variable, and shadowing it silently breaks argument splatting
    # (the tool then prints its usage text instead of its version).
    param([string]$Name, [string[]]$ArgList)
    try {
        $out = & $Name @ArgList 2>&1 | Select-Object -First 1
        return ($out | Out-String).Trim()
    } catch {
        return ''
    }
}

# ---------------------------------------------------------------------------
# Elevation check
# ---------------------------------------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

# ---------------------------------------------------------------------------
# winget helper
# ---------------------------------------------------------------------------
function Install-WingetPackage {
    param(
        [string]$Id,
        [string]$DisplayName,
        [string]$ExtraArgs = ''
    )

    if ($Verify) {
        Write-Warn "$DisplayName missing (would install via winget: $Id)"
        return $false
    }

    if (-not $isAdmin) {
        Write-Warn "$DisplayName missing - installing it needs administrator rights"
        Write-Note "Re-run this script in an elevated terminal, or install manually:"
        Write-Note "winget install --id $Id --exact"
        return $false
    }

    Write-Info "Installing $DisplayName ($Id)..."
    $cmd = "winget install --id $Id --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity $ExtraArgs"
    $out = Invoke-Expression $cmd 2>&1
    $rc = $LASTEXITCODE

    if ($rc -eq 0 -or $rc -eq -1978335189) {
        # -1978335189 = "no applicable update found / already installed"
        Write-Ok "$DisplayName installed"
        return $true
    }

    Write-Bad "$DisplayName install failed (winget exit $rc)"
    $out | Select-Object -Last 4 | ForEach-Object { Write-Note $_ }
    return $false
}

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
Write-Host ''
Write-Host '==============================================================' -ForegroundColor Cyan
Write-Host '  TrueGaze - Complete Prerequisite Installer'                 -ForegroundColor Cyan
Write-Host '  Build toolchain + vcpkg + SDK + game prerequisites'         -ForegroundColor Cyan
Write-Host '  An HCEP Product by Kirk LaSalle'                          -ForegroundColor Cyan
Write-Host '==============================================================' -ForegroundColor Cyan
if ($Verify) { Write-Host '  MODE: verify only (nothing will be installed)' -ForegroundColor Yellow }
if (-not $isAdmin) { Write-Host '  MODE: not elevated (machine-wide installs will be skipped)' -ForegroundColor Yellow }

# ===========================================================================
Write-Step '1. Windows PowerShell'
# ===========================================================================
Write-Ok "PowerShell $($PSVersionTable.PSVersion)"

# ===========================================================================
Write-Step '2. winget (package installer)'
# ===========================================================================
$wingetOk = Test-Command 'winget'
if ($wingetOk) {
    Write-Ok "winget         $(Get-ToolVersion 'winget' @('--version'))"
} else {
    Write-Bad 'winget not found.'
    Write-Note 'winget ships with "App Installer" from the Microsoft Store.'
    Write-Note 'Install it, or install each tool below manually, then re-run.'
}

# ===========================================================================
Write-Step '3. Build toolchain'
# ===========================================================================

# --- Git ---
if (Test-Command 'git') {
    Write-Ok "git            $(Get-ToolVersion 'git' @('-c','core.pager=cat','--version'))"
} elseif ($wingetOk) {
    Install-WingetPackage -Id 'Git.Git' -DisplayName 'Git' | Out-Null
} else {
    Write-Bad 'Git missing (https://git-scm.com/download/win)'
}

# --- CMake (>= 3.23 enforced by CMakeLists.txt) ---
$cmakeOk = Test-Command 'cmake'
if ($cmakeOk) {
    $raw = Get-ToolVersion 'cmake' @('--version')
    Write-Ok "cmake          $raw"    if ($raw -match '(\d+)\.(\d+)') {
        $maj = [int]$Matches[1]; $min = [int]$Matches[2]
        if ($maj -lt 3 -or ($maj -eq 3 -and $min -lt 23)) {
            Write-Warn "CMake $maj.$min is below the required 3.23 - upgrade it."
        }
    }
} elseif ($wingetOk) {
    Install-WingetPackage -Id 'Kitware.CMake' -DisplayName 'CMake' | Out-Null
} else {
    Write-Bad 'CMake missing (https://cmake.org/download/)'
}

# --- 7-Zip (hard requirement: both game archives are LZMA 7z) ---
$sevenZipCandidates = @(
    'C:\Program Files\7-Zip\7z.exe',
    'C:\Program Files (x86)\7-Zip\7z.exe'
)
foreach ($drive in @('C','D','E','F','G','H')) {
    if (Test-Path "$drive`:\") {
        $sevenZipCandidates += "$drive`:\Program Files\7-Zip\7z.exe"
        $sevenZipCandidates += "$drive`:\Program Files (x86)\7-Zip\7z.exe"
    }
}
$sevenZip = $sevenZipCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sevenZip) {
    $w = & where.exe 7z 2>$null | Select-Object -First 1
    if ($w -and (Test-Path $w.Trim())) { $sevenZip = $w.Trim() }
}

if ($sevenZip) {
    Write-Ok "7-Zip          $sevenZip"
} elseif ($wingetOk) {
    Install-WingetPackage -Id '7zip.7zip' -DisplayName '7-Zip' | Out-Null
    foreach ($c in $sevenZipCandidates) {
        if (Test-Path $c) { $sevenZip = $c; break }
    }
} else {
    Write-Bad '7-Zip missing - required to extract SKSE and the Address Library'
}

# --- Visual Studio + C++ workload ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsOk = $false
if (Test-Path $vswhere) {
    $vsName = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName 2>$null
    $vsVer  = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion 2>$null
    if ($vsName) {
        Write-Ok ("Visual Studio  {0} ({1})" -f $vsName, $vsVer)
        $vsOk = $true
    } else {
        $anyVs = & $vswhere -latest -property displayName 2>$null
        if ($anyVs) {
            Write-Warn "Visual Studio found ('$anyVs') but WITHOUT the C++ workload"
            Write-Note 'Add it via the Visual Studio Installer:'
            Write-Note '  Desktop development with C++  (Microsoft.VisualStudio.Workload.VCTools)'
        } else {
            Write-Warn 'Visual Studio not found'
        }
    }
} else {
    Write-Warn 'Visual Studio not detected (vsWhere missing)'
}

if (-not $vsOk) {
    if ($wingetOk -and -not $Verify) {
        Install-WingetPackage -Id 'Microsoft.VisualStudio.2022.BuildTools' `
            -DisplayName 'Visual Studio 2022 Build Tools (C++ workload)' `
            -ExtraArgs '--override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"' | Out-Null
    } else {
        Write-Note 'Install "Visual Studio 2022 Build Tools" with the'
        Write-Note '"Desktop development with C++" workload.'
    }
}

# --- VC++ Redistributable ---
$vcKey = 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'
if (Test-Path $vcKey) {
    $vc = Get-ItemProperty $vcKey
    Write-Ok "VC++ Redist    $($vc.Version)"
} elseif ($wingetOk) {
    Install-WingetPackage -Id 'Microsoft.VCRedist.2015+.x64' -DisplayName 'VC++ 2015-2022 x64 Redistributable' | Out-Null
} else {
    Write-Warn 'VC++ x64 Redistributable not registered (game may fail to start)'
}

# ===========================================================================
Write-Step '4. vcpkg (C++ dependency manager)'
# ===========================================================================
if (-not $VcpkgPath) {
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
        $VcpkgPath = $env:VCPKG_ROOT
    } else {
        # Prefer an existing checkout over cloning a second copy.
        $existing = @('D:\vcpkg', 'C:\vcpkg', 'C:\dev\vcpkg') |
            Where-Object { Test-Path (Join-Path $_ 'vcpkg.exe') } |
            Select-Object -First 1
        $VcpkgPath = if ($existing) { $existing } else { 'D:\vcpkg' }
    }
}

$vcpkgExe = Join-Path $VcpkgPath 'vcpkg.exe'
$vcpkgToolchain = Join-Path $VcpkgPath 'scripts\buildsystems\vcpkg.cmake'

if ((Test-Path $vcpkgExe) -and (Test-Path $vcpkgToolchain)) {
    Write-Ok "vcpkg          $VcpkgPath"
} elseif ($Verify) {
    Write-Warn "vcpkg missing at $VcpkgPath"
} elseif (Test-Command 'git') {
    if (Test-Path (Join-Path $VcpkgPath '.git')) {
        Write-Info "vcpkg checkout exists at $VcpkgPath but is not bootstrapped; bootstrapping..."
    } else {
        Write-Info "Cloning vcpkg into $VcpkgPath ..."
        & git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgPath 2>&1 |
            ForEach-Object { Write-Note $_ }
    }

    $bootstrap = Join-Path $VcpkgPath 'bootstrap-vcpkg.bat'
    if (Test-Path $bootstrap) {
        Write-Info 'Bootstrapping vcpkg (this can take a minute)...'
        $p = Start-Process -FilePath 'cmd.exe' `
            -ArgumentList "/c `"$bootstrap`" -disableMetrics" `
            -WorkingDirectory $VcpkgPath -NoNewWindow -Wait -PassThru
        if ($p.ExitCode -eq 0 -and (Test-Path $vcpkgExe)) {
            Write-Ok "vcpkg bootstrapped at $VcpkgPath"
        } else {
            Write-Bad "vcpkg bootstrap failed (exit $($p.ExitCode))"
        }
    } else {
        Write-Bad "bootstrap-vcpkg.bat not found in $VcpkgPath"
    }
} else {
    Write-Bad 'Cannot obtain vcpkg: git is missing.'
}

# Persist VCPKG_ROOT. CMakePresets.json resolves the toolchain as
# $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake, so an unset variable is a
# hard configure failure - this is the single most common fresh-machine blocker.
if ((Test-Path $vcpkgToolchain) -and -not $NoPersist) {
    if ($env:VCPKG_ROOT -ne $VcpkgPath) {
        if ($Verify) {
            Write-Warn "VCPKG_ROOT is '$env:VCPKG_ROOT' but vcpkg is at '$VcpkgPath' (would be set)"
        } else {
            try {
                [Environment]::SetEnvironmentVariable('VCPKG_ROOT', $VcpkgPath, 'User')
                $env:VCPKG_ROOT = $VcpkgPath
                Write-Ok "VCPKG_ROOT set to $VcpkgPath (user environment)"
                Write-Note 'Open a NEW terminal for it to take effect everywhere.'
            } catch {
                Write-Bad "Could not persist VCPKG_ROOT: $($_.Exception.Message)"
            }
        }
    } else {
        Write-Ok "VCPKG_ROOT  = $env:VCPKG_ROOT"
    }

    # The plugin is built static-MD; make sure the triplet exists.
    $triplet = Join-Path $VcpkgPath 'triplets\x64-windows-static-md.cmake'
    if (Test-Path $triplet) {
        Write-Ok 'triplet x64-windows-static-md present'
    } else {
        Write-Warn "triplet x64-windows-static-md not found at $triplet"
    }
} elseif (Test-Path $vcpkgToolchain) {
    $env:VCPKG_ROOT = $VcpkgPath
    Write-Info "VCPKG_ROOT set for this session only (session-only install requested)"
}

# ===========================================================================
Write-Step '5. CommonLibSSE-NG submodule'
# ===========================================================================
$projectRoot = Split-Path $PSScriptRoot -Parent
$sdkDir = Join-Path $projectRoot 'extern\CommonLibSSE-NG'
$sdkCmake = Join-Path $sdkDir 'CMakeLists.txt'

if (Test-Path $sdkCmake) {
    Write-Ok 'extern/CommonLibSSE-NG present'
} elseif ($Verify) {
    Write-Warn 'extern/CommonLibSSE-NG missing (SDK not vendored)'
} elseif (Test-Command 'git') {
    if (Test-Path (Join-Path $projectRoot '.git')) {
        Write-Info 'Initialising the CommonLibSSE-NG submodule...'
        Push-Location $projectRoot
        try {
            & git submodule update --init --recursive 2>&1 | ForEach-Object { Write-Note $_ }
        } finally {
            Pop-Location
        }
        if (Test-Path $sdkCmake) {
            Write-Ok 'Submodule initialised'
        } else {
            Write-Bad 'Submodule init did not produce extern/CommonLibSSE-NG/CMakeLists.txt'
        }
    } else {
        Write-Bad 'Not a git checkout - cannot initialise the submodule automatically'
    }
} else {
    Write-Bad 'Git missing - cannot initialise the SDK submodule'
}

# ===========================================================================
if (-not $SkipGame) {
    Write-Step '6. Game prerequisites (SKSE64 + Address Library)'
    # ===========================================================================

    # Locate the game so the delegate script does not have to repeat the search.
    if (-not $GamePath) {
        $candidates = @()
        try {
            $reg = Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition' `
                -Name 'Installed Path' -ErrorAction Stop
            if ($reg.'Installed Path') { $candidates += $reg.'Installed Path' }
        } catch { }

        foreach ($drive in @('C','D','E','F','G','H')) {
            if (-not (Test-Path "$drive`:\")) { continue }
            $candidates += "$drive`:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"
            $candidates += "$drive`:\Steam\steamapps\common\Skyrim Special Edition"
            $candidates += "$drive`:\SteamLibrary\steamapps\common\Skyrim Special Edition"
        }
        $GamePath = $candidates | Where-Object { $_ -and (Test-Path (Join-Path $_ 'SkyrimSE.exe')) } |
            Select-Object -First 1
        # The registry value carries a trailing backslash; trim it so the printed
        # path and every Join-Path below are tidy.
        if ($GamePath) { $GamePath = $GamePath.TrimEnd('\', '/') }
    }

    if (-not $GamePath -or -not (Test-Path (Join-Path $GamePath 'SkyrimSE.exe'))) {
        Write-Warn 'Skyrim Special Edition not found; skipping the game step.'
        Write-Note 'Install Skyrim SE/AE, then re-run with -GamePath "<path>".'
    } else {
        Write-Ok "Game           $GamePath"
        $exeVersion = (Get-Item (Join-Path $GamePath 'SkyrimSE.exe')).VersionInfo.FileVersion
        Write-Ok "Game version   $exeVersion"

        $loader = Join-Path $GamePath 'skse64_loader.exe'
        $skseVer = (($exeVersion -split '\.')[0..2]) -join '_'
        $skseDll = Join-Path $GamePath "skse64_${skseVer}.dll"
        $alibName = "versionlib-$(($exeVersion -split '\.')[0..3] -join '-').bin"
        $alib = Join-Path $GamePath "Data\SKSE\Plugins\$alibName"

        if ((Test-Path $loader) -and (Test-Path $skseDll)) {
            Write-Ok 'SKSE64         installed and version-matched'
        } else {
            Write-Warn "SKSE64 missing (need skse64_$skseVer.dll)"

            if (-not $Verify) {
                $delegate = Join-Path $PSScriptRoot 'Install-Prerequisites.ps1'
                if (Test-Path $delegate) {
                    Write-Info 'Handing off to Install-Prerequisites.ps1 (extracts anything in downloads\)...'
                    $dArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $delegate,
                               '-GamePath', $GamePath, '-AutoInstall')
                    if ($sevenZip) { $dArgs += @('-SevenZip', $sevenZip) }
                    & powershell @dArgs 2>&1 | ForEach-Object { Write-Note $_ }
                } else {
                    Write-Bad 'Install-Prerequisites.ps1 not found'
                }
            }

            # Re-check after the handoff.
            if (-not ((Test-Path $loader) -and (Test-Path $skseDll))) {
                Write-Note ''
                Write-Note '--- ONE MANUAL STEP REQUIRED (Nexus requires a login) ---'
                Write-Note "SKSE64 (AE build for game $exeVersion)"
                Write-Note '  https://www.nexusmods.com/skyrimspecialedition/mods/30379'
                Write-Note "  Save the .7z into: $(Join-Path $projectRoot 'downloads')"
            }
        }

        if (Test-Path $alib) {
            Write-Ok "Address Library $alibName present"
        } else {
            Write-Warn "Address Library missing (need $alibName)"
            if (-not $Verify) {
                Write-Note ''
                Write-Note '--- ONE MANUAL STEP REQUIRED (permissions forbid redistribution) ---'
                Write-Note 'Address Library for SKSE Plugins ("All in one", AE)'
                Write-Note '  https://www.nexusmods.com/skyrimspecialedition/mods/32444'
                Write-Note "  Save the .7z into: $(Join-Path $projectRoot 'downloads')"
            }
        }

        if (-not $Verify -and (Test-Path (Join-Path $projectRoot 'scripts\Test-TrueGazeHealth.ps1'))) {
            Write-Info 'Running the TrueGaze health check...'
            & powershell -NoProfile -ExecutionPolicy Bypass -File `
                (Join-Path $projectRoot 'scripts\Test-TrueGazeHealth.ps1') -GamePath $GamePath 2>&1 |
                Select-Object -Last 12 | ForEach-Object { Write-Note $_ }
        }
    }
}

# ===========================================================================
Write-Step 'Summary'
# ===========================================================================
Write-Host ''
if ($Verify) {
    Write-Host "  Verify complete: $script:Failures missing, $script:Warnings warning(s)." -ForegroundColor White
    Write-Host '  Re-run without -Verify to install what is missing.' -ForegroundColor White
} elseif ($script:Failures -eq 0) {
    Write-Host '  All automated prerequisites are in place.' -ForegroundColor Green
    Write-Host ''
    Write-Host '  Next:' -ForegroundColor White
    Write-Host '    TrueGaze.cmd build      build the plugin'   -ForegroundColor White
    Write-Host '    TrueGaze.cmd verify     confirm prerequisites' -ForegroundColor White
    Write-Host '    TrueGaze.cmd all        build + deploy + verify' -ForegroundColor White
    Write-Host '    TrueGazeConfig.html     configure and launch' -ForegroundColor White
} else {
    Write-Host "  $script:Failures item(s) still need attention, $script:Warnings warning(s)." -ForegroundColor Yellow
    Write-Host '  Address each FAIL above, then re-run this script.' -ForegroundColor White
}

if (-not $isAdmin -and -not $Verify) {
    Write-Host ''
    Write-Host '  Note: not running as administrator. Any WARN above that mentions' -ForegroundColor DarkYellow
    Write-Host '  administrator rights can be resolved by re-running this script' -ForegroundColor DarkYellow
    Write-Host '  from an elevated ("Run as administrator") terminal.' -ForegroundColor DarkYellow
}

if ($env:VCPKG_ROOT) {
    Write-Host ''
    Write-Host "  VCPKG_ROOT = $env:VCPKG_ROOT" -ForegroundColor DarkGray
    Write-Host '  Open a NEW terminal before building so the variable is inherited.' -ForegroundColor DarkGray
}

Write-Host ''
if ($Verify) { exit 0 }
exit $(if ($script:Failures -gt 0) { 1 } else { 0 })