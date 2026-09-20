<#
.SYNOPSIS
    Pre-flight and post-run health verification for the TrueGaze SKSE plugin.
.DESCRIPTION
    Answers one question honestly: will this plugin load and run, and did it?

    Two modes:

      PRE-FLIGHT  (default)  Static checks against the binary and the game install.
                             Run before launching. Catches version mismatches, missing
                             prerequisites, stale binaries and broken loader contracts
                             while they are still cheap to fix.

      POST-RUN    (-PostRun) Parses TrueGaze.log for the exact strings the engine
                             emits, and reports what actually happened on the last
                             run - including whether the bone names resolved.

    Dot-source this file to reuse the functions; run it directly for a report.

.PARAMETER GamePath
    Skyrim Special Edition install directory. Auto-detected when omitted.

.PARAMETER PluginPath
    TrueGaze.dll to inspect. Defaults to the freshly built Release binary.

.PARAMETER PostRun
    Parse the game log instead of doing static checks.

.PARAMETER LogPath
    Override the TrueGaze.log location.

.EXAMPLE
    .\Test-TrueGazeHealth.ps1
    .\Test-TrueGazeHealth.ps1 -PostRun
    .\Test-TrueGazeHealth.ps1 -GamePath "G:\Steam\steamapps\common\Skyrim Special Edition"

.NOTES
    Part of TrueGaze(TM), an HCEP product by Kirk LaSalle.
#>

[CmdletBinding()]
param(
    [string]$GamePath,
    [string]$PluginPath,
    [switch]$PostRun,
    [string]$LogPath,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Result bookkeeping.
#
# Every check records one of four outcomes rather than throwing, so a single
# failure never hides the checks that follow it. The summary at the end is what
# decides the exit code.
# ---------------------------------------------------------------------------
$script:TGResults = @()

function Add-TGResult {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][ValidateSet('Pass', 'Fail', 'Warn', 'Skip')][string]$Status,
        [string]$Detail = '',
        [string]$Fix = ''
    )

    $script:TGResults += [pscustomobject]@{
        Name   = $Name
        Status = $Status
        Detail = $Detail
        Fix    = $Fix
    }

    if (-not $Quiet) {
        $colour = switch ($Status) {
            'Pass' { 'Green' }
            'Fail' { 'Red' }
            'Warn' { 'Yellow' }
            'Skip' { 'DarkGray' }
        }
        $tag = switch ($Status) {
            'Pass' { '  OK  ' }
            'Fail' { ' FAIL ' }
            'Warn' { ' WARN ' }
            'Skip' { ' SKIP ' }
        }
        Write-Host ("  [" + $tag + "] " + $Name) -ForegroundColor $colour
        if ($Detail) { Write-Host ("           " + $Detail) -ForegroundColor DarkGray }
        if ($Fix -and $Status -in @('Fail', 'Warn')) {
            Write-Host ("           -> " + $Fix) -ForegroundColor DarkCyan
        }
    }
}

# ---------------------------------------------------------------------------
# Locate the game.
#
# Registry first (authoritative), then the usual Steam locations, then the
# Steam library manifest so a second drive works. Nothing is assumed; an
# install that cannot be found is reported, not guessed at.
# ---------------------------------------------------------------------------
function Get-TGGameInfo {
    param([string]$Explicit)

    $candidates = New-Object System.Collections.Generic.List[string]

    if ($Explicit) { $candidates.Add($Explicit) }

    # Bethesda's install-location key. Present for both Steam and GOG copies.
    foreach ($hive in @('HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition',
            'HKLM:\SOFTWARE\Bethesda Softworks\Skyrim Special Edition')) {
        try {
            $key = Get-ItemProperty -Path $hive -Name 'Installed Path' -ErrorAction Stop
            if ($key.'Installed Path') { $candidates.Add($key.'Installed Path') }
        }
        catch { }
    }

    # Steam: parse libraryfolders.vdf so installs on other drives are found.
    foreach ($steamRoot in @('C:\Program Files (x86)\Steam', 'C:\Program Files\Steam',
            'G:\Program Files (x86)\Steam', 'D:\Steam', 'E:\Steam')) {
        # Probe the drive before touching it. Test-Path against a path on a
        # nonexistent drive throws rather than returning false, which would abort
        # detection on any machine without that letter.
        $drive = Split-Path $steamRoot -Qualifier
        if ($drive -and -not (Test-Path ($drive + '\'))) { continue }

        $vdf = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            try {
                foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
                    $lib = $m.Groups[1].Value -replace '\\\\', '\'
                    if (Test-Path ($lib + '\')) {
                        $candidates.Add((Join-Path $lib 'steamapps\common\Skyrim Special Edition'))
                    }
                }
            }
            catch { }
        }
        $candidates.Add((Join-Path $steamRoot 'steamapps\common\Skyrim Special Edition'))
    }

    foreach ($c in $candidates) {
        if (-not $c) { continue }
        $qv = Split-Path $c -Qualifier
        if ($qv -and -not (Test-Path ($qv + '\'))) { continue }
        $exe = Join-Path $c 'SkyrimSE.exe'
        if (Test-Path $exe) {
            $fv = (Get-Item $exe).VersionInfo.FileVersion
            return [pscustomobject]@{
                Path        = (Resolve-Path $c).Path
                ExePath     = $exe
                FileVersion = $fv
                # '1.7.104.0' -> '1-7-104-0' (Address Library naming)
                Dotted      = ($fv -split '\.')[0..3] -join '-'
                # '1.7.104.0' -> '1_7_104'   (SKSE runtime DLL naming)
                Underscore  = (($fv -split '\.')[0..2]) -join '_'
            }
        }
    }

    return $null
}

function Get-TGLogPath {
    param([string]$GameInfoPath, [string]$Explicit)

    if ($Explicit) { return $Explicit }

    $docs = [Environment]::GetFolderPath('MyDocuments')

    $candidates = @(
        (Join-Path $docs 'My Games\Skyrim Special Edition\SKSE\TrueGaze.log'),
        (Join-Path $docs 'My Games\Skyrim Special Edition GOG\SKSE\TrueGaze.log')
    )

    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $candidates[0]
}

# ---------------------------------------------------------------------------
# Read the PE export table.
#
# The SKSE loader contract is defined entirely by which symbols the DLL exports.
# A plugin that exports only SKSEPlugin_Load is on the legacy path; one that also
# exports SKSEPlugin_Version declares its runtime compatibility properly. Both
# should be present, and a missing one is invisible without checking.
#
# dumpbin is preferred when a Visual Studio install provides it. The fallback
# scans for the export names directly, which is reliable because these strings
# exist only as export names in this binary.
# ---------------------------------------------------------------------------
function Get-TGExports {
    param([Parameter(Mandatory)][string]$Path)

    $wanted = @('SKSEPlugin_Load', 'SKSEPlugin_Query', 'SKSEPlugin_Version')

    $dumpbin = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe' -ErrorAction SilentlyContinue |
    Select-Object -First 1
    if (-not $dumpbin) {
        $dumpbin = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    }

    if ($dumpbin) {
        # Relaxed for the call: dumpbin writes benign chatter to stderr, and with
        # $ErrorActionPreference='Stop' PowerShell would raise it as terminating.
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $out = @(& $dumpbin.FullName /EXPORTS $Path 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
        }
        finally {
            $ErrorActionPreference = $previous
        }

        $found = @()
        foreach ($w in $wanted) { if ($out -match [regex]::Escape($w)) { $found += $w } }
        return [pscustomobject]@{ Method = 'dumpbin'; Found = $found; Wanted = $wanted }
    }

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
    $found = @()
    foreach ($w in $wanted) { if ($ascii.Contains($w)) { $found += $w } }
    return [pscustomobject]@{ Method = 'string-scan'; Found = $found; Wanted = $wanted }
}

function Get-TGSha256 {
    param([Parameter(Mandatory)][string]$Path)
    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash
}

# ===========================================================================
#  PRE-FLIGHT CHECKS
# ===========================================================================
function Invoke-TGPreFlight {
    param(
        [string]$GamePath,
        [string]$PluginPath
    )

    Write-Host ''
    Write-Host 'TrueGaze - Pre-Flight Health Check' -ForegroundColor Cyan
    Write-Host '==================================' -ForegroundColor Cyan

    # -- 1. Game install ----------------------------------------------------
    Write-Host ''
    Write-Host 'Game install' -ForegroundColor White

    $game = Get-TGGameInfo -Explicit $GamePath
    if (-not $game) {
        Add-TGResult -Name 'Skyrim Special Edition located' -Status 'Fail' `
            -Detail 'No SkyrimSE.exe found in any known location.' `
            -Fix 'Pass -GamePath with your install directory.'
        Write-Host ''
        Write-Host 'Cannot continue without a game install.' -ForegroundColor Red
        return
    }
    Add-TGResult -Name 'Skyrim Special Edition located' -Status 'Pass' -Detail $game.Path
    Add-TGResult -Name 'Game version detected' -Status 'Pass' `
        -Detail ("$($game.FileVersion)  (Address Library expects versionlib-$($game.Dotted).bin)")

    # -- 2. SKSE -----------------------------------------------------------
    Write-Host ''
    Write-Host 'SKSE' -ForegroundColor White

    $loader = Join-Path $game.Path 'skse64_loader.exe'
    if (Test-Path $loader) {
        Add-TGResult -Name 'skse64_loader.exe present' -Status 'Pass'

        # The runtime DLL name encodes the game build it was compiled against.
        # A mismatch here is the classic "SKSE reported a different game version"
        # failure, and it is silent until launch.
        $skseDlls = @(Get-ChildItem $game.Path -Filter 'skse64_*.dll' -ErrorAction SilentlyContinue)
        if ($skseDlls.Count -eq 0) {
            Add-TGResult -Name 'SKSE runtime DLL present' -Status 'Fail' `
                -Detail 'skse64_loader.exe exists but no skse64_<version>.dll beside it.' `
                -Fix 'The SKSE archive was partially extracted. Re-extract it over the game folder.'
        }
        else {
            $match = $skseDlls | Where-Object { $_.Name -match [regex]::Escape($game.Underscore) }
            if ($match) {
                Add-TGResult -Name 'SKSE runtime matches game version' -Status 'Pass' `
                    -Detail $match[0].Name
            }
            else {
                Add-TGResult -Name 'SKSE runtime matches game version' -Status 'Fail' `
                    -Detail ("Found: " + (($skseDlls | ForEach-Object { $_.Name }) -join ', ') + "  Expected something like skse64_$($game.Underscore).dll") `
                    -Fix 'You have the wrong SKSE build for this game version. Download the AE build matching your SkyrimSE.exe.'
            }
        }
    }
    else {
        $ae = 'https://www.nexusmods.com/skyrimspecialedition/mods/30379'
        Add-TGResult -Name 'skse64_loader.exe present' -Status 'Fail' `
            -Detail 'SKSE is not installed. Nothing loads without it.' `
            -Fix "Download the AE build (Nexus-only): $ae"
    }

    # -- 3. Address Library ------------------------------------------------
    Write-Host ''
    Write-Host 'Address Library' -ForegroundColor White

    $pluginsDir = Join-Path $game.Path 'Data\SKSE\Plugins'
    $expectedLib = "versionlib-$($game.Dotted).bin"
    $libPath = Join-Path $pluginsDir $expectedLib

    if (Test-Path $libPath) {
        $mb = [math]::Round((Get-Item $libPath).Length / 1MB, 1)
        Add-TGResult -Name 'Address Library present and version-matched' -Status 'Pass' `
            -Detail "$expectedLib ($mb MB)"
    }
    else {
        # A library for a *different* version is worse than none: it looks present
        # but every address lookup silently resolves to the wrong place.
        $anyLib = @(Get-ChildItem $pluginsDir -Filter 'versionlib-*.bin' -ErrorAction SilentlyContinue)
        if ($anyLib.Count -gt 0) {
            Add-TGResult -Name 'Address Library present and version-matched' -Status 'Fail' `
                -Detail ("Found " + (($anyLib | ForEach-Object { $_.Name }) -join ', ') + " but need $expectedLib") `
                -Fix 'Download the Address Library entry matching your exact game version.'
        }
        else {
            $alm = 'https://www.nexusmods.com/skyrimspecialedition/mods/32444'
            Add-TGResult -Name 'Address Library present and version-matched' -Status 'Fail' `
                -Detail "Missing $expectedLib" `
                -Fix "TrueGaze resolves game offsets through this file. Download 'Address Library for SKSE Plugins' (AE): $alm"
        }
    }

    # -- 4. VC++ runtime ---------------------------------------------------
    Write-Host ''
    Write-Host 'Runtime dependencies' -ForegroundColor White

    $sys = Join-Path $env:SystemRoot 'System32'
    $missing = @()
    foreach ($dll in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        if (-not (Test-Path (Join-Path $sys $dll))) { $missing += $dll }
    }
    if ($missing.Count -eq 0) {
        Add-TGResult -Name 'Microsoft VC++ 2015-2022 x64 runtime' -Status 'Pass' -Detail 'msvcp140 + vcruntime140 present'
    }
    else {
        Add-TGResult -Name 'Microsoft VC++ 2015-2022 x64 runtime' -Status 'Fail' `
            -Detail ("Missing: " + ($missing -join ', ')) `
            -Fix 'Install the x64 Visual C++ Redistributable for Visual Studio 2015-2022.'
    }

    # -- 5. Plugin binary --------------------------------------------------
    Write-Host ''
    Write-Host 'Plugin binary' -ForegroundColor White

    if (-not $PluginPath) {
        $PluginPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'build\windows-release\Release\TrueGaze.dll'
    }

    if (-not (Test-Path $PluginPath)) {
        Add-TGResult -Name 'Plugin DLL exists' -Status 'Fail' -Detail $PluginPath `
            -Fix 'Build first: cmake --build --preset release'
        return
    }
    $kb = [math]::Round((Get-Item $PluginPath).Length / 1KB, 1)
    $built = (Get-Item $PluginPath).LastWriteTime
    Add-TGResult -Name 'Plugin DLL exists' -Status 'Pass' -Detail "$kb KB, built $built"

    $exports = Get-TGExports -Path $PluginPath
    $required = @('SKSEPlugin_Load', 'SKSEPlugin_Query', 'SKSEPlugin_Version')
    $missingExports = @($required | Where-Object { $_ -notin $exports.Found })

    if ($missingExports.Count -eq 0) {
        Add-TGResult -Name 'SKSE loader contract satisfied' -Status 'Pass' `
            -Detail "All 3 exports present (via $($exports.Method))"
    }
    else {
        Add-TGResult -Name 'SKSE loader contract satisfied' -Status 'Fail' `
            -Detail ("Missing export(s): " + ($missingExports -join ', ')) `
            -Fix 'SKSEPluginInfo is not being emitted. Check src/Main.cpp has not been mangled by a formatter.'
    }

    # The hook target is the single most dangerous thing in this codebase: a wrong
    # vtable index corrupts unrelated entries. Confirm the corrected target is the
    # one that actually got compiled in.
    $ascii = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($PluginPath))
    if ($ascii.Contains('slot 0xAD')) {
        Add-TGResult -Name 'Gaze driver targets Actor::Update (slot 0xAD)' -Status 'Pass'
    }
    else {
        Add-TGResult -Name 'Gaze driver targets Actor::Update (slot 0xAD)' -Status 'Fail' `
            -Detail 'The hook target string is not in the binary.' `
            -Fix 'Rebuild. An old binary may still target the invalid Main vtable slot 0x05.'
    }
    if ($ascii.Contains('Gaze tick threw')) {
        Add-TGResult -Name 'Tick exception guard compiled in' -Status 'Pass'
    }
    else {
        Add-TGResult -Name 'Tick exception guard compiled in' -Status 'Warn' `
            -Detail 'Guard string not found; an unguarded tick can crash the game.' `
            -Fix 'Rebuild from current source.'
    }

    # -- 6. Deployed copy --------------------------------------------------
    Write-Host ''
    Write-Host 'Deployment' -ForegroundColor White

    $deployed = Join-Path $pluginsDir 'TrueGaze.dll'
    if (Test-Path $deployed) {
        $same = (Get-TGSha256 -Path $deployed) -eq (Get-TGSha256 -Path $PluginPath)
        if ($same) {
            Add-TGResult -Name 'Deployed DLL matches the build' -Status 'Pass'
        }
        else {
            # This has bitten twice. The packaged copy silently went stale and the
            # crashing build was shipped.
            $dAge = (Get-Item $deployed).LastWriteTime
            Add-TGResult -Name 'Deployed DLL matches the build' -Status 'Fail' `
                -Detail "Deployed copy is from $dAge and differs from the current build." `
                -Fix 'Re-run the deploy step. You are about to test a stale binary.'
        }
    }
    else {
        Add-TGResult -Name 'Deployed DLL matches the build' -Status 'Warn' `
            -Detail 'TrueGaze.dll is not in Data\SKSE\Plugins yet.' `
            -Fix 'Run Deploy-TrueGaze.ps1.'
    }

    $iniDeployed = Join-Path $pluginsDir 'TrueGaze.ini'
    if (Test-Path $iniDeployed) {
        $iniText = Get-Content $iniDeployed -Raw
        if ($iniText -match '(?im)^\s*bEnableTrueGaze\s*=\s*(true|false|0|1)') {
            $val = $Matches[1]
            Add-TGResult -Name 'TrueGaze.ini deployed and parseable' -Status 'Pass' `
                -Detail "bEnableTrueGaze=$val"
            if ($val -match '^(false|0)$') {
                Add-TGResult -Name 'Gaze kinematics enabled' -Status 'Warn' `
                    -Detail 'bEnableTrueGaze is false. The plugin will load but do nothing.' `
                    -Fix 'Set bEnableTrueGaze=true once the load-only test has passed.'
            }
        }
        else {
            Add-TGResult -Name 'TrueGaze.ini deployed and parseable' -Status 'Warn' `
                -Detail 'File exists but bEnableTrueGaze was not found.'
        }
    }
    else {
        Add-TGResult -Name 'TrueGaze.ini deployed and parseable' -Status 'Warn' `
            -Detail 'No INI in Data\SKSE\Plugins; compiled defaults will be used.'
    }

    # -- 7. Log directory --------------------------------------------------
    Write-Host ''
    Write-Host 'Diagnostics' -ForegroundColor White

    $log = Get-TGLogPath -GameInfoPath $game.Path
    $logDir = Split-Path $log -Parent
    $probeDir = if (Test-Path $logDir) { $logDir } else { Split-Path $logDir -Parent }

    if (Test-Path $probeDir) {
        try {
            $probeFile = Join-Path $probeDir '.truegaze-writetest'
            Set-Content -Path $probeFile -Value 'probe' -ErrorAction Stop
            Remove-Item $probeFile -Force
            Add-TGResult -Name 'Log directory writable' -Status 'Pass' -Detail $log
        }
        catch {
            Add-TGResult -Name 'Log directory writable' -Status 'Fail' `
                -Detail "$probeDir is not writable." `
                -Fix 'Run as a user that can write to Documents\My Games.'
        }
    }
    else {
        Add-TGResult -Name 'Log directory writable' -Status 'Warn' `
            -Detail "$probeDir does not exist yet; SKSE will create it on first launch."
    }

    # -- 8. Expectations that will not be met ------------------------------
    Write-Host ''
    Write-Host 'Known-incomplete features (expected, not failures)' -ForegroundColor White

    $oar = Join-Path $game.Path 'Data\meshes\actors\character\animations\OpenAnimationReplacer'
    if (Test-Path $oar) {
        Add-TGResult -Name 'OAR conditions will NOT fire' -Status 'Warn' `
            -Detail 'OpenAnimationReplacer is installed, but TrueGaze registration is unimplemented.' `
            -Fix 'Tracked as issue #6. Gaze itself is unaffected.'
    }
    else {
        Add-TGResult -Name 'OAR not installed (so its gap is moot)' -Status 'Skip'
    }

    # Configuration is INI-only (Data/SKSE/Plugins/TrueGaze.ini).
    $ini = Join-Path $game.Path 'Data\SKSE\Plugins\TrueGaze.ini'
    if (Test-Path $ini) {
        Add-TGResult -Name 'Configuration INI deployed' -Status 'Pass' `
            -Detail 'Data\SKSE\Plugins\TrueGaze.ini present. Edit with TrueGazeConfig.html.'
    }
    else {
        Add-TGResult -Name 'Configuration INI missing' -Status 'Warn' `
            -Detail 'Data\SKSE\Plugins\TrueGaze.ini not found; compiled defaults will be used.' `
            -Fix 'Re-run deploy so TrueGaze.ini is copied into the game Data folder.'
    }

    # -- 9. Legacy SkyUI / Papyrus / MCM artifacts -------------------------
    #
    # TrueGaze is deliberately vanilla-UI: configuration lives in
    # Data\SKSE\Plugins\TrueGaze.ini and is edited through TrueGazeConfig.html.
    # There is no ESP, no Papyrus script, no MCM menu and no SkyUI dependency.
    #
    # Leftover files from the abandoned MCM era are worse than harmless: a stale
    # TrueGaze.esp in the load order, or a TrueGaze_MCM.pex that SkyUI still
    # tries to bind, can produce confusing in-game behaviour and log noise that
    # looks like a TrueGaze bug. Detect them and tell the user to remove them.
    Write-Host ''
    Write-Host 'Legacy SkyUI / Papyrus / MCM artifacts (should be absent)' -ForegroundColor White

    $dataDir = Join-Path $game.Path 'Data'
    $legacy = @(
        @{ Path = (Join-Path $dataDir 'TrueGaze.esp'); What = 'plugin (ESP)' }
        @{ Path = (Join-Path $dataDir 'TrueGaze.esl'); What = 'plugin (ESL)' }
        @{ Path = (Join-Path $dataDir 'TrueGaze_MCM.pex'); What = 'MCM Papyrus script' }
        @{ Path = (Join-Path $dataDir 'TrueGaze.pex'); What = 'Papyrus script' }
        @{ Path = (Join-Path $dataDir 'MCM\Config\TrueGaze'); What = 'MCM Helper config folder' }
        @{ Path = (Join-Path $dataDir 'Interface\MCM\Config\TrueGaze'); What = 'legacy MCM config folder' }
    )

    $foundLegacy = @($legacy | Where-Object { Test-Path $_.Path })

    # Translation files are matched by wildcard because the old MCM shipped six.
    $legacyTranslations = @()
    $transDir = Join-Path $dataDir 'Interface\Translations'
    if (Test-Path $transDir) {
        $legacyTranslations = @(Get-ChildItem -Path $transDir -Filter 'TrueGaze_*.txt' -File -ErrorAction SilentlyContinue)
    }

    if ($foundLegacy.Count -eq 0 -and $legacyTranslations.Count -eq 0) {
        Add-TGResult -Name 'No legacy SkyUI/Papyrus/MCM artifacts' -Status 'Pass' `
            -Detail 'Vanilla-UI configuration confirmed: INI + TrueGazeConfig.html only.'
    }
    else {
        $names = @($foundLegacy | ForEach-Object { Split-Path $_.Path -Leaf })
        $names += @($legacyTranslations | ForEach-Object { $_.Name })
        Add-TGResult -Name 'Legacy SkyUI/Papyrus/MCM artifacts present' -Status 'Warn' `
            -Detail ("Found: " + ($names -join ', ')) `
            -Fix 'Run Deploy-TrueGaze.ps1 (or LaunchTrueGaze.bat) to remove them; TrueGaze is vanilla-UI and needs none of these.'
    }

    Invoke-TGSummary
}

# ===========================================================================
#  POST-RUN LOG ANALYSIS
# ===========================================================================
function Invoke-TGPostRun {
    param(
        [string]$LogPath,
        [string]$GamePath
    )

    Write-Host ''
    Write-Host 'TrueGaze - Post-Run Log Analysis' -ForegroundColor Cyan
    Write-Host '================================' -ForegroundColor Cyan

    $log = Get-TGLogPath -GameInfoPath $GamePath -Explicit $LogPath

    Write-Host ''
    if (-not (Test-Path $log)) {
        Add-TGResult -Name 'TrueGaze.log exists' -Status 'Fail' -Detail $log `
            -Fix 'The plugin never ran. Check that you launched via skse64_loader.exe, and look at skse64.log.'
        Invoke-TGSummary
        return
    }

    $text = Get-Content $log -Raw
    $age = (Get-Item $log).LastWriteTime
    Add-TGResult -Name 'TrueGaze.log exists' -Status 'Pass' -Detail "$log  (written $age)"

    # Each marker is an exact string the engine emits. Checking them in order
    # turns "it did not work" into "it stopped here".
    $markers = @(
        @{ Name = 'Plugin loaded'; Pattern = 'SKSE plugin loaded successfully'; Required = $true },
        @{ Name = 'Messaging listener bound'; Pattern = 'Game data loaded'; Required = $true },
        @{ Name = 'Game data loaded'; Pattern = 'Game data loaded'; Required = $true },
        @{ Name = 'Configuration loaded'; Pattern = 'Configuration loaded'; Required = $false },
        @{ Name = 'Gaze driver installed'; Pattern = 'Gaze driver installed\.'; Required = $true },
        @{ Name = 'Engine ready'; Pattern = 'Gaze engine ready'; Required = $true },
        @{ Name = 'Skeleton probed'; Pattern = 'Skeleton probe for'; Required = $false }
    )

    Write-Host ''
    Write-Host 'Startup sequence' -ForegroundColor White
    foreach ($m in $markers) {
        if ($text -match $m.Pattern) {
            Add-TGResult -Name $m.Name -Status 'Pass'
        }
        elseif ($m.Required) {
            Add-TGResult -Name $m.Name -Status 'Fail' -Detail 'Not found in the log.'
        }
        else {
            Add-TGResult -Name $m.Name -Status 'Warn' -Detail 'Not found (may be expected in this configuration).'
        }
    }

    # The bone-name assumption is the most likely silent failure in the whole
    # engine, so report the actual probe result rather than just its presence.
    Write-Host ''
    Write-Host 'Skeleton resolution (the critical unknown)' -ForegroundColor White

    $probes = [regex]::Matches($text, 'Skeleton probe for ([0-9A-Fa-f]{8}): spine=(\w+) neck=(\w+) head=(\w+) eyeL=(\w+) eyeR=(\w+) \((\d+) of 5 resolved\)(?: origin=(\w+))?')
    if ($probes.Count -eq 0) {
        Add-TGResult -Name 'Skeleton probe result' -Status 'Warn' `
            -Detail 'No probe line. Either no eligible actor was in range, or the tick never ran.' `
            -Fix 'Stand within 15 m of a living NPC, then check again.'
    }
    else {
        $p = $probes[0]
        $found = [int]$p.Groups[7].Value
        $detail = "spine=$($p.Groups[2].Value) neck=$($p.Groups[3].Value) head=$($p.Groups[4].Value) eyeL=$($p.Groups[5].Value) eyeR=$($p.Groups[6].Value)"
        if ($p.Groups[8].Success) {
            $detail += " origin=$($p.Groups[8].Value)"
        }

        if ($p.Groups[4].Value -eq 'yes') {
            Add-TGResult -Name 'Head bone resolved' -Status 'Pass' -Detail $detail
        }
        else {
            Add-TGResult -Name 'Head bone resolved' -Status 'Fail' `
                -Detail $detail `
                -Fix 'Bone names do not match this rig. Extend the candidate lists in src/Engine/GazeEngine.cpp.'
        }
        if ($p.Groups[5].Value -eq 'yes' -and $p.Groups[6].Value -eq 'yes') {
            Add-TGResult -Name 'Eye bones resolved' -Status 'Pass'
        }
        else {
            Add-TGResult -Name 'Eye bones resolved' -Status 'Warn' `
                -Detail 'Eye nodes missing; geometric head-socket fallback may still provide the visual origin.' `
                -Fix 'Expected on many vanilla humanoid rigs. Validate the reported origin and perceptual result.'
        }
        Add-TGResult -Name "Skeleton coverage ($found of 5)" -Status $(if ($found -ge 4) { 'Pass' } else { 'Warn' })
    }

    # Faults.
    Write-Host ''
    Write-Host 'Faults' -ForegroundColor White

    $threw = [regex]::Matches($text, 'Gaze tick threw')
    if ($threw.Count -eq 0) {
        Add-TGResult -Name 'No tick exceptions' -Status 'Pass'
    }
    else {
        Add-TGResult -Name 'No tick exceptions' -Status 'Fail' `
            -Detail "$($threw.Count) occurrence(s). The guard caught them, but something is wrong." `
            -Fix 'Search the log for "Gaze tick threw" and send the message.'
    }

    $budget = [regex]::Matches($text, 'Frame budget exceeded')
    if ($budget.Count -eq 0) {
        Add-TGResult -Name 'Frame budget respected' -Status 'Pass'
    }
    else {
        Add-TGResult -Name 'Frame budget respected' -Status 'Warn' `
            -Detail "$($budget.Count) overrun warning(s)."
    }

    $headMiss = [regex]::Matches($text, 'No head bone found')
    if ($headMiss.Count -eq 0) {
        Add-TGResult -Name 'No missing-head warnings' -Status 'Pass'
    }
    else {
        Add-TGResult -Name 'No missing-head warnings' -Status 'Fail' `
            -Detail "$($headMiss.Count) actor(s) had no head bone." `
            -Fix 'Extend the bone-name candidates in src/Engine/GazeEngine.cpp.'
    }

    Invoke-TGSummary
}

# ===========================================================================
#  SUMMARY
# ===========================================================================
function Invoke-TGSummary {
    $fail = @($script:TGResults | Where-Object { $_.Status -eq 'Fail' })
    $warn = @($script:TGResults | Where-Object { $_.Status -eq 'Warn' })
    $pass = @($script:TGResults | Where-Object { $_.Status -eq 'Pass' })

    Write-Host ''
    Write-Host '--------------------------------------------' -ForegroundColor DarkGray
    Write-Host ("  Passed: {0}   Warnings: {1}   Failed: {2}" -f $pass.Count, $warn.Count, $fail.Count) `
        -ForegroundColor $(if ($fail.Count -gt 0) { 'Red' } elseif ($warn.Count -gt 0) { 'Yellow' } else { 'Green' })

    if ($fail.Count -gt 0) {
        Write-Host ''
        Write-Host '  Must fix before launching:' -ForegroundColor Red
        foreach ($f in $fail) {
            Write-Host ("    - " + $f.Name) -ForegroundColor Red
            if ($f.Fix) { Write-Host ("      " + $f.Fix) -ForegroundColor DarkCyan }
        }
    }

    Write-Host ''
    if ($fail.Count -eq 0) {
        if ($script:TGPostRun) {
            Write-Host '  VERDICT: The plugin ran. See the markers above for what it did.' -ForegroundColor Green
        }
        else {
            Write-Host '  VERDICT: Clear to launch.' -ForegroundColor Green
        }
    }
    else {
        if ($script:TGPostRun) {
            Write-Host '  VERDICT: The last run did not complete cleanly. See the failures above.' -ForegroundColor Red
        }
        else {
            Write-Host '  VERDICT: NOT ready. Fix the failures above.' -ForegroundColor Red
        }
    }
    Write-Host ''
}

# ===========================================================================
#  ENTRY POINT
# ===========================================================================
if ($MyInvocation.InvocationName -ne '.') {
    if ($PostRun) {
        $script:TGPostRun = $true
        Invoke-TGPostRun -LogPath $LogPath -GamePath $GamePath
    }
    else {
        $script:TGPostRun = $false
        Invoke-TGPreFlight -GamePath $GamePath -PluginPath $PluginPath
    }
    exit $(if (@($script:TGResults | Where-Object { $_.Status -eq 'Fail' }).Count -gt 0) { 1 } else { 0 })
}
