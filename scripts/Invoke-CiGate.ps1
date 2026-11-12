#Requires -Version 5.1
<#
.SYNOPSIS
    TrueGaze local CI gate (R14 E4.2).

.DESCRIPTION
    Mirrors .github/workflows/build-test.yml locally:
      1. Configure the standalone preset (windows-release).
      2. Build it (fails on MSVC warning C5102 - audit 2026-10-15 F-01).
      3. Run ctest (KinematicsTests, ValidationTests, GeometryTests, TraceTests,
         HcepBridgeClientMock).
      4. Verify charter manifest integrity (config/charter_manifest.json).
      5. Doc-consistency spot check (version strings across key docs; the
         expected version is read from project(VERSION) in CMakeLists.txt).
      6. Configurator parity (HTML DEFAULTS vs shipped INI).

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
        Write-Host "[1/6] Configure ($configurePreset)..." -ForegroundColor Yellow
        cmake --preset $configurePreset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed (exit $LASTEXITCODE)." }
    }
    else {
        Write-Host '[1/6] Configure skipped (-SkipConfigure).' -ForegroundColor DarkGray
    }

    # ---- Stage 2: Build ----------------------------------------------------
    Write-Host "[2/6] Build ($buildPreset)..." -ForegroundColor Yellow
    # MSBuild reports compiler warnings on stdout, so no 2>&1 is needed (and under
    # Windows PowerShell 5.1 with ErrorActionPreference=Stop it would turn any
    # native stderr line into a terminating error).
    cmake --build --preset $buildPreset | Tee-Object -Variable buildLog | Out-Host
    $buildExit = $LASTEXITCODE
    if ($buildExit -ne 0) { throw "Build failed (exit $buildExit)." }

    # Audit 2026-10-15 F-01: warning C5102 ("ignoring invalid command-line macro
    # definition") is how NDEBUG silently survived truegaze_force_asserts() and
    # compiled every test assert out. It must never be tolerated again.
    $c5102 = @($buildLog | Where-Object { "$_" -match 'warning C5102' })
    if ($c5102.Count -gt 0) {
        $c5102 | Select-Object -First 3 | ForEach-Object { Write-Warning "      $_" }
        throw "Build emitted warning C5102 ($($c5102.Count)x): a test target's assert() may be compiled out."
    }

    # ---- Stage 3: Test -----------------------------------------------------
    Write-Host "[3/6] Test ($testPreset)..." -ForegroundColor Yellow
    ctest --preset $testPreset --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed (exit $LASTEXITCODE)." }

    # ---- Stage 4: Charter manifest ----------------------------------------
    Write-Host '[4/6] Charter manifest verify...' -ForegroundColor Yellow
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
    # Audit 2026-10-15 F-07: the expected version comes from project(VERSION) in
    # CMakeLists.txt - the same source the plugin binary is built from - instead
    # of vcpkg.json with a stale hardcoded fallback. vcpkg.json is now CHECKED
    # against it like every other document.
    $expectedVersion = & (Join-Path $PSScriptRoot 'Get-TrueGazeVersion.ps1')
    $escapedVersion = [regex]::Escape($expectedVersion)
    $checks = @(
        @{ File = 'vcpkg.json'; Pattern = '"version-string"\s*:\s*"' + $escapedVersion + '"' },
        @{ File = 'README.md'; Pattern = $escapedVersion },
        @{ File = 'CHANGELOG.md'; Pattern = $escapedVersion },
        @{ File = 'PRD.md'; Pattern = '\*\*Version:\*\*\s*' + $escapedVersion },
        @{ File = 'ROADMAP.md'; Pattern = '\*\*Current Milestone:\*\*\s*v' + $escapedVersion }
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

    & node (Join-Path $repoRoot 'tests\ConfiguratorTests.js')
    if ($LASTEXITCODE -ne 0) { throw 'Configurator behavior/schema/command parity failed.' }
    Write-Host '=== CI gate PASSED ===' -ForegroundColor Green
    exit 0
}
finally {
    Pop-Location
}
