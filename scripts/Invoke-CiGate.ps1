#Requires -Version 5.1
<#
.SYNOPSIS
    TrueGaze local CI gate (R14 E4.2).

.DESCRIPTION
    Mirrors .github/workflows/build-test.yml locally:
      1. Configure the standalone preset (windows-release).
      2. Build it.
      3. Run ctest (KinematicsTests, ValidationTests, HcepBridgeClientMock).
      4. Verify charter manifest integrity (config/charter_manifest.json).
      5. Doc-consistency spot check (version strings across key docs).

    Exits non-zero on the first failing stage. Use -SkipConfigure to reuse an
    existing build tree.

.PARAMETER Preset
    CMake preset to use. Default: windows-release.

.PARAMETER SkipConfigure
    Skip the configure step (build tree must already exist).

.EXAMPLE
    pwsh ./scripts/Invoke-CiGate.ps1
#>
[CmdletBinding()]
param(
    [string]$Preset = 'windows-release',
    [switch]$SkipConfigure
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    Write-Host '=== TrueGaze CI gate ===' -ForegroundColor Cyan

    # ---- Stage 1: Configure ------------------------------------------------
    if (-not $SkipConfigure) {
        Write-Host "[1/5] Configure ($Preset)..." -ForegroundColor Yellow
        cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed (exit $LASTEXITCODE)." }
    }
    else {
        Write-Host '[1/5] Configure skipped (-SkipConfigure).' -ForegroundColor DarkGray
    }

    # ---- Stage 2: Build ----------------------------------------------------
    Write-Host "[2/5] Build ($Preset)..." -ForegroundColor Yellow
    cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }

    # ---- Stage 3: Test -----------------------------------------------------
    Write-Host "[3/5] Test ($Preset)..." -ForegroundColor Yellow
    ctest --preset $Preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed (exit $LASTEXITCODE)." }

    # ---- Stage 4: Charter manifest ----------------------------------------
    Write-Host '[4/5] Charter manifest verify...' -ForegroundColor Yellow
    $charterPath = Join-Path $repoRoot 'config/charter_manifest.json'
    if (-not (Test-Path $charterPath)) {
        throw "Charter manifest missing: $charterPath"
    }
    try {
        $null = Get-Content $charterPath -Raw | ConvertFrom-Json
        Write-Host '      charter_manifest.json parses OK.' -ForegroundColor Green
    }
    catch {
        throw "Charter manifest is not valid JSON: $($_.Exception.Message)"
    }

    # ---- Stage 5: Doc consistency -----------------------------------------
    Write-Host '[5/5] Doc consistency spot check...' -ForegroundColor Yellow
    $expectedVersion = '1.0.5'
    $checks = @(
        @{ File = 'vcpkg.json'; Pattern = '"version-string"\s*:\s*"' + [regex]::Escape($expectedVersion) + '"' },
        @{ File = 'README.md'; Pattern = [regex]::Escape($expectedVersion) },
        @{ File = 'CHANGELOG.md'; Pattern = [regex]::Escape($expectedVersion) }
    )
    $docFailures = 0
    foreach ($check in $checks) {
        $path = Join-Path $repoRoot $check.File
        if (-not (Test-Path $path)) {
            Write-Warning "      MISSING file: $($check.File)"
            $docFailures++
            continue
        }
        $content = Get-Content $path -Raw
        if ($content -notmatch $check.Pattern) {
            Write-Warning "      $($check.File) does not mention version $expectedVersion."
            $docFailures++
        }
        else {
            Write-Host "      $($check.File): version $expectedVersion present." -ForegroundColor Green
        }
    }
    if ($docFailures -gt 0) {
        throw "Doc consistency check failed with $docFailures issue(s)."
    }

    Write-Host '=== CI gate PASSED ===' -ForegroundColor Green
    exit 0
}
finally {
    Pop-Location
}
