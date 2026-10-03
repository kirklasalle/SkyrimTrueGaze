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
    [string]$Preset = 'standalone',
    [switch]$SkipConfigure
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    Write-Host '=== TrueGaze CI gate ===' -ForegroundColor Cyan

    $configurePreset = $Preset
    $buildPreset = switch ($Preset) {
        'windows-release' { 'release' }
        'windows-debug'   { 'debug' }
        default           { $Preset }
    }
    $testPreset = $buildPreset

    # ---- Stage 1: Configure ------------------------------------------------
    if (-not $SkipConfigure) {
        Write-Host "[1/5] Configure ($configurePreset)..." -ForegroundColor Yellow
        cmake --preset $configurePreset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed (exit $LASTEXITCODE)." }
    }
    else {
        Write-Host '[1/5] Configure skipped (-SkipConfigure).' -ForegroundColor DarkGray
    }

    # ---- Stage 2: Build ----------------------------------------------------
    Write-Host "[2/5] Build ($buildPreset)..." -ForegroundColor Yellow
    cmake --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }

    # ---- Stage 3: Test -----------------------------------------------------
    Write-Host "[3/5] Test ($testPreset)..." -ForegroundColor Yellow
    ctest --preset $testPreset --output-on-failure
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
    Write-Host '[5/6] Doc consistency spot check...' -ForegroundColor Yellow
    $vcpkgPath = Join-Path $repoRoot 'vcpkg.json'
    $expectedVersion = '1.0.7'
    if (Test-Path $vcpkgPath) {
        try {
            $vcpkgJson = Get-Content $vcpkgPath -Raw | ConvertFrom-Json
            if ($vcpkgJson.'version-string') {
                $expectedVersion = $vcpkgJson.'version-string'
            }
        } catch {}
    }
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

    # ---- Stage 6: Configurator parity (R15 C4.2) --------------------------
    # Parses the HTML DEFAULTS block and diffs the shipped INI values, so
    # F1-class default drift (HTML says 1.0, INI says 1.5) can never ship again.
    # Also verifies the root and skyrim/ HTML copies are byte-identical.
    Write-Host '[6/6] Configurator parity check...' -ForegroundColor Yellow
    $htmlPath = Join-Path $repoRoot 'TrueGazeConfig.html'
    $iniPath = Join-Path $repoRoot 'skyrim/SKSE/Plugins/TrueGaze.ini'
    if (-not (Test-Path $htmlPath) -or -not (Test-Path $iniPath)) {
        throw "Configurator parity: missing $htmlPath or $iniPath"
    }

    # Parse the INI into a section->key->value map.
    $ini = @{}
    $section = ''
    foreach ($line in (Get-Content $iniPath)) {
        $t = $line.Trim()
        if ($t -match '^\[(.+?)\]$') { $section = $Matches[1]; $ini[$section] = @{}; continue }
        if ($t -match '^([fbis]\w+)\s*=\s*(.+?)\s*$') { $ini[$section][$Matches[1]] = $Matches[2] }
    }

    # Parse the HTML DEFAULTS block: lines of the form  key: ['type', default, ...]
    $html = Get-Content $htmlPath -Raw
    $parityFailures = 0
    $sectionName = ''
    foreach ($line in ($html -split "`n")) {
        if ($line -match "^\s+'(\w+)':\s*\{") { $sectionName = $Matches[1]; continue }
        if ($line -match "^\s+(\w+):\s*\['(bool|float|int|choice|text)',\s*([^,]+),") {
            $key = $Matches[1]
            $htmlDefault = $Matches[3].Trim()
            $htmlDefault = $htmlDefault.Trim([char]39, [char]34)
            if ($ini.ContainsKey($sectionName) -and $ini[$sectionName].ContainsKey($key)) {
                $iniValue = "$($ini[$sectionName][$key])"
                # Normalise INI bools: the engine accepts 1/0 and true/false —
                # but ONLY for keys the HTML schema types as 'bool' (an int key
                # like iRayRenderMode legitimately holds a numeric 1).
                if ($Matches[2] -eq 'bool') {
                    if ($iniValue -eq '1') { $iniValue = 'true' }
                    if ($iniValue -eq '0') { $iniValue = 'false' }
                }
                # Normalise the pipe path: the INI holds '\\.\pipe\...' (doubled)
                # and the HTML JS literal holds it doubled again. Backslash count
                # is an encoding artefact — compare backslash-stripped forms.
                $iniNorm = $iniValue.Replace('\', '')
                $htmlNorm = $htmlDefault.Replace('\', '')
                if ($htmlNorm -notmatch '^(true|false)$') {
                    $iniF = 0.0; $htmlF = 0.0
                    if ([double]::TryParse($iniNorm, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$iniF) -and
                        [double]::TryParse($htmlNorm, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$htmlF)) {
                        if ([Math]::Abs($iniF - $htmlF) -gt 0.0005) {
                            Write-Warning "      PARITY: [$sectionName] $key HTML=$htmlNorm INI=$iniNorm"
                            $parityFailures++
                        }
                        continue
                    }
                }
                if ($iniNorm -ne $htmlNorm) {
                    Write-Warning "      PARITY: [$sectionName] $key HTML=$htmlNorm INI=$iniNorm"
                    $parityFailures++
                }
            }
        }
    }
    if ($parityFailures -gt 0) {
        throw "Configurator parity check failed with $parityFailures drift(s). Sync TrueGazeConfig.html DEFAULTS to the shipped INI."
    }
    Write-Host '      HTML DEFAULTS match the shipped INI.' -ForegroundColor Green

    # Copy sync: root and skyrim/ HTML must be identical.
    $skyrimHtml = Join-Path $repoRoot 'skyrim/TrueGazeConfig.html'
    if (Test-Path $skyrimHtml) {
        $h1 = (Get-FileHash $htmlPath -Algorithm SHA256).Hash
        $h2 = (Get-FileHash $skyrimHtml -Algorithm SHA256).Hash
        if ($h1 -ne $h2) {
            throw 'Configurator copy sync: root and skyrim/TrueGazeConfig.html differ. Copy the root file to skyrim/.'
        }
        Write-Host '      root and skyrim/ HTML copies identical.' -ForegroundColor Green
    }

    Write-Host '=== CI gate PASSED ===' -ForegroundColor Green
    exit 0
}
finally {
    Pop-Location
}
