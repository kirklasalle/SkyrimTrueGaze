<#
.SYNOPSIS
    One-click build, deploy, verify and launch for the TrueGaze SKSE plugin.
.DESCRIPTION
    Does the whole loop in the right order and refuses to launch into a state it
    knows is broken:

      1. BUILD    Compile the Release plugin.
      2. DEPLOY   Copy TrueGaze.dll and TrueGaze.ini into Data\SKSE\Plugins.
      3. VERIFY   Run the pre-flight health check.
      4. LAUNCH   Start skse64_loader.exe, but only if VERIFY passed.

    Step 3 is the point of the script. It is cheap to check the loader contract,
    the Address Library version and the deployed binary hash *before* a two-minute
    game launch, and expensive not to.

.PARAMETER GamePath
    Skyrim install directory. Auto-detected when omitted.

.PARAMETER NoBuild
    Skip the compile step and deploy the existing binary.

.PARAMETER NoLaunch
    Build, deploy and verify, but do not start the game.

.PARAMETER Force
    Launch even when the health check reports a failure. For deliberate
    experimentation only - it will usually crash the game.

.PARAMETER LoadOnly
    Deploy with bEnableTrueGaze=false. Proves the plugin loads and installs its
    hook without running the kinematics engine. This is the recommended first run.

.PARAMETER PostRun
    Analyse the log from the last run instead of deploying.

.PARAMETER ForceIni
    Overwrite the deployed TrueGaze.ini with the shipped defaults.

    By default the deployed INI is LEFT ALONE on deploy, because it holds your
    live tuning and anything the console commands have persisted. Use this only
    when you deliberately want to reset configuration to the shipped defaults.
    Skip build/deploy/launch and just analyse the log from the last session.

.EXAMPLE
    .\Deploy-TrueGaze.ps1 -LoadOnly     # first run: prove it loads safely
    .\Deploy-TrueGaze.ps1               # normal loop
    .\Deploy-TrueGaze.ps1 -PostRun      # what actually happened?
    .\Deploy-TrueGaze.ps1 -NoLaunch     # build + verify only

.NOTES
    Part of TrueGaze(TM), an HCEP product by Kirk LaSalle.
#>

[CmdletBinding()]
param(
    [string]$GamePath,
    [switch]$NoBuild,
    [switch]$NoLaunch,
    [switch]$Force,
    [switch]$LoadOnly,
    [switch]$PostRun,
    [switch]$ForceIni
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
$builtDll = Join-Path $projectRoot 'build\windows-release\Release\TrueGaze.dll'
$builtIni = Join-Path $projectRoot 'skyrim\SKSE\Plugins\TrueGaze.ini'
$healthScript = Join-Path $PSScriptRoot 'Test-TrueGazeHealth.ps1'

function Write-Step {
    param([string]$Number, [string]$Text)
    Write-Host ''
    Write-Host ("[" + $Number + "] " + $Text) -ForegroundColor Cyan
    Write-Host ('-' * 60) -ForegroundColor DarkGray
}

function Write-Ok { param([string]$t) Write-Host ("  OK   " + $t) -ForegroundColor Green }
function Write-Bad { param([string]$t) Write-Host ("  FAIL " + $t) -ForegroundColor Red }
function Write-Info { param([string]$t) Write-Host ("  ..   " + $t) -ForegroundColor DarkGray }

# Run a native executable and capture everything it writes.
#
# The naive form - `& $exe ... 2>&1` with $ErrorActionPreference='Stop' - is a
# trap: PowerShell promotes any stderr line from a native command to a
# terminating error, and cmake writes ordinary progress to stderr. The build
# would report failure while having actually succeeded.
#
# Two things are needed, both of them deliberate:
#   * ErrorActionPreference is relaxed for the duration of the call, so stderr
#     is captured as data rather than raised.
#   * The native exit code is read from $LASTEXITCODE immediately, before
#     anything else can overwrite it. $? is not used: it reflects PowerShell's
#     own stream handling, not the process result.
function Invoke-TGNative {
    param(
        [Parameter(Mandatory)][string]$Exe,
        [string[]]$Arguments = @()
    )

    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $Exe @Arguments 2>&1)
        $code = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }

    # Render each captured record to plain text so callers can treat the result
    # as strings regardless of how PowerShell typed it (ErrorRecord vs string).
    $text = @($output | ForEach-Object { $_.ToString() })

    return [pscustomobject]@{ ExitCode = $code; Output = $text }
}

Write-Host ''
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host '  TrueGaze - One-Click Build / Deploy / Verify / Launch'      -ForegroundColor Cyan
Write-Host '  An HCEP Product by Kirk LaSalle'                          -ForegroundColor Cyan
Write-Host '============================================================' -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Post-run mode: just read what happened last time.
# ---------------------------------------------------------------------------
if ($PostRun) {
    $pr = Invoke-TGNative -Exe 'powershell' -Arguments @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $healthScript, '-PostRun')
    $pr.Output | ForEach-Object { Write-Host $_ }
    exit $pr.ExitCode
}

# ---------------------------------------------------------------------------
# Locate the game up front. Everything else depends on this path.
# ---------------------------------------------------------------------------
. $healthScript          # dot-source to reuse Get-TGGameInfo
$game = Get-TGGameInfo -Explicit $GamePath
if (-not $game) {
    Write-Host ''
    Write-Bad 'Skyrim Special Edition not found.'
    Write-Host '  Pass -GamePath with your install directory:' -ForegroundColor DarkCyan
    Write-Host '    .\Deploy-TrueGaze.ps1 -GamePath "G:\...\Skyrim Special Edition"' -ForegroundColor DarkGray
    exit 1
}

$pluginsDir = Join-Path $game.Path 'Data\SKSE\Plugins'

# ===========================================================================
# 1. BUILD
# ===========================================================================
Write-Step '1/4' 'Build'
if ($NoBuild) {
    Write-Info 'Skipped (-NoBuild).'
}
else {
    if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = 'D:\vcpkg' }
    Write-Info "VCPKG_ROOT = $env:VCPKG_ROOT"

    Push-Location $projectRoot
    try {
        $configure = Invoke-TGNative -Exe 'cmake' -Arguments @('--preset', 'windows-release')
        if ($configure.ExitCode -ne 0) {
            Write-Bad "Configure failed (exit $($configure.ExitCode))."
            $configure.Output | Where-Object { $_ -match 'error|Error|CMake Error' } | Select-Object -First 20 |
            ForEach-Object { Write-Host ('    ' + $_) -ForegroundColor Red }
            exit 1
        }
        $build = Invoke-TGNative -Exe 'cmake' -Arguments @('--build', '--preset', 'release')
    }
    finally { Pop-Location }

    if ($build.ExitCode -ne 0) {
        Write-Bad "Build failed (exit $($build.ExitCode))."
        $build.Output | Where-Object { $_ -match 'error|Error' } | Select-Object -First 20 |
        ForEach-Object { Write-Host ('    ' + $_) -ForegroundColor Red }
        exit 1
    }
    Write-Ok 'Build succeeded.'
}

if (-not (Test-Path $builtDll)) {
    Write-Bad "No binary at $builtDll"
    exit 1
}

# ===========================================================================
# 2. DEPLOY
# ===========================================================================
Write-Step '2/4' 'Deploy'

if (-not (Test-Path $pluginsDir)) {
    New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
    Write-Info "Created $pluginsDir"
}

$destDll = Join-Path $pluginsDir 'TrueGaze.dll'
$destIni = Join-Path $pluginsDir 'TrueGaze.ini'

Copy-Item -Path $builtDll -Destination $destDll -Force
$kb = [math]::Round((Get-Item $destDll).Length / 1KB, 1)
Write-Ok "TrueGaze.dll -> $destDll ($kb KB)"

if (Test-Path $builtIni) {
    # ---------------------------------------------------------------------
    # Never clobber a user's tuned INI on a routine deploy.
    #
    # The packaged INI is the set of SHIPPED DEFAULTS. The deployed copy is the
    # user's live configuration - it holds their tuning, and anything the console
    # commands have persisted. Copying over it silently discarded all of that on
    # every deploy, which looked exactly like settings randomly reverting.
    #
    # So:
    #   * no deployed INI yet      -> copy the defaults (first install)
    #   * deployed INI exists      -> leave it alone, unless -ForceIni is given
    #   * keys missing from it     -> report them; do NOT rewrite the file
    #
    # The missing-key report matters: new engine versions add keys, and an older
    # deployed INI simply lacks them. That is safe - the engine keeps its compiled
    # default for any absent key - but the user should know they exist.
    # ---------------------------------------------------------------------
    if ((Test-Path $destIni) -and -not $ForceIni) {
        Write-Ok "TrueGaze.ini -> kept (existing user configuration preserved)"

        $packagedKeys = @()
        $deployedKeys = @()
        foreach ($line in Get-Content $builtIni) {
            if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=') {
                $packagedKeys += ($Matches[1] + '=' + $line.Split('=')[0].Trim())
            }
        }
        foreach ($line in Get-Content $destIni) {
            if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=') {
                $deployedKeys += ($Matches[1] + '=' + $line.Split('=')[0].Trim())
            }
        }
        $missing = @($packagedKeys | Where-Object { $deployedKeys -notcontains $_ })
        if ($missing.Count -gt 0) {
            Write-Info ("{0} key(s) in the newer defaults are absent from your INI; the engine " -f $missing.Count)
            Write-Info "  uses its compiled default for those. Compare with the HTML page if you want them."
        }
    }
    else {
        Copy-Item -Path $builtIni -Destination $destIni -Force
        if ($ForceIni) {
            Write-Warn "TrueGaze.ini -> overwritten (-ForceIni): your tuning was replaced with shipped defaults."
        }
        else {
            Write-Ok "TrueGaze.ini -> $destIni"
        }
    }
}
else {
    Write-Info 'No packaged INI found; compiled defaults will be used.'
}

# ---------------------------------------------------------------------------
# Remove legacy SkyUI / Papyrus / MCM artifacts.
#
# TrueGaze is deliberately vanilla-UI: configuration is the INI edited through
# TrueGazeConfig.html. There is no ESP, no Papyrus script, no MCM menu and no
# SkyUI dependency. Files left over from the abandoned MCM era are actively
# harmful - a stale TrueGaze.esp in the load order or a TrueGaze_MCM.pex that
# SkyUI still binds produces confusing in-game behaviour and log noise that
# looks like a TrueGaze defect. Delete them on every deploy so an upgrade from
# an old install self-heals.
# ---------------------------------------------------------------------------
$dataDir = Join-Path $game.Path 'Data'
$legacyPaths = @(
    @{ Path = (Join-Path $dataDir 'TrueGaze.esp'); Label = 'TrueGaze.esp' }
    @{ Path = (Join-Path $dataDir 'TrueGaze.esl'); Label = 'TrueGaze.esl' }
    @{ Path = (Join-Path $dataDir 'TrueGaze_MCM.pex'); Label = 'TrueGaze_MCM.pex' }
    @{ Path = (Join-Path $dataDir 'TrueGaze.pex'); Label = 'TrueGaze.pex' }
    @{ Path = (Join-Path $dataDir 'MCM\Config\TrueGaze'); Label = 'MCM\Config\TrueGaze\' }
    @{ Path = (Join-Path $dataDir 'Interface\MCM\Config\TrueGaze'); Label = 'Interface\MCM\Config\TrueGaze\' }
)

$removed = @()
foreach ($entry in $legacyPaths) {
    if (Test-Path $entry.Path) {
        Remove-Item -Path $entry.Path -Recurse -Force -ErrorAction SilentlyContinue
        if (-not (Test-Path $entry.Path)) { $removed += $entry.Label }
    }
}

# The old MCM shipped six translation files (TrueGaze_<LANG>.txt). None are
# needed by the vanilla-UI build.
$transDir = Join-Path $dataDir 'Interface\Translations'
if (Test-Path $transDir) {
    $legacyTranslations = @(Get-ChildItem -Path $transDir -Filter 'TrueGaze_*.txt' -File -ErrorAction SilentlyContinue)
    foreach ($t in $legacyTranslations) {
        Remove-Item -Path $t.FullName -Force -ErrorAction SilentlyContinue
        if (-not (Test-Path $t.FullName)) { $removed += ("Interface\Translations\" + $t.Name) }
    }
}

if ($removed.Count -gt 0) {
    Write-Ok ("Removed legacy SkyUI/Papyrus/MCM artifacts: " + ($removed -join ', '))
}
else {
    Write-Info 'No legacy SkyUI/Papyrus/MCM artifacts to remove.'
}

# ===========================================================================
# 3. VERIFY
# ===========================================================================
Write-Step '3/4' 'Verify (pre-flight health check)'

$verify = Invoke-TGNative -Exe 'powershell' -Arguments @(
    '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
    "& '$healthScript' -GamePath '$($game.Path)' -PluginPath '$builtDll'")
$verifyCode = $verify.ExitCode
$verify.Output | ForEach-Object { Write-Host $_ }

if ($verifyCode -ne 0 -and -not $Force) {
    Write-Host ''
    Write-Bad 'Health check failed. Not launching.'
    Write-Host '  Fix the failures listed above, then re-run.' -ForegroundColor DarkCyan
    Write-Host '  TrueGaze will not launch into a state it knows is broken.' -ForegroundColor DarkCyan
    Write-Host '  (Override deliberately with -Force.)' -ForegroundColor DarkGray
    exit 1
}

# ===========================================================================
# 4. LAUNCH
# ===========================================================================
Write-Step '4/4' 'Launch'

$loader = Join-Path $game.Path 'skse64_loader.exe'

if ($NoLaunch) {
    Write-Info 'Skipped (-NoLaunch).'
    Write-Host ''
    Write-Host 'Ready. Launch with:' -ForegroundColor Green
    Write-Host ("  " + $loader) -ForegroundColor White
    Write-Host ''
    Write-Host 'Then analyse the run with:' -ForegroundColor Green
    Write-Host '  .\Deploy-TrueGaze.ps1 -PostRun' -ForegroundColor White
    exit 0
}

if (-not (Test-Path $loader)) {
    Write-Bad "skse64_loader.exe not found at $loader"
    Write-Host '  SKSE must be installed before the game can be launched with the plugin.' -ForegroundColor DarkCyan
    exit 1
}

if ($verifyCode -ne 0 -and $Force) {
    Write-Host ''
    Write-Host '  WARNING: launching despite health check failures (-Force).' -ForegroundColor Yellow
    Write-Host '  Expect a crash. Keep a backup save.' -ForegroundColor Yellow
}

Write-Info 'Starting Skyrim via SKSE...'
Write-Host ''
Write-Host '  IMPORTANT: launch via skse64_loader.exe, never SkyrimSE.exe.' -ForegroundColor Yellow
Write-Host '           Steam'"'"'s Play button will NOT load the plugin.' -ForegroundColor Yellow
Write-Host ''
Write-Host '  After you quit the game, run:' -ForegroundColor Green
Write-Host '    .\Deploy-TrueGaze.ps1 -PostRun' -ForegroundColor White

# Start-Process with -WorkingDirectory is required: SKSE resolves Data\
# relative to the game folder.
Start-Process -FilePath $loader -WorkingDirectory $game.Path
exit 0
