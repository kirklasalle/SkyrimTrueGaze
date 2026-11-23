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

.PARAMETER ModPath
    Stage the mod into an isolated MO2/Vortex mod folder instead of the live
    game's Data directory. Staging never removes existing files or launches
    the game.

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
    [string]$ModPath,
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
function Write-Warn { param([string]$t) Write-Host ("  WARN " + $t) -ForegroundColor Yellow }

function Disable-TGEngine {
    param([Parameter(Mandatory)][string]$Path)

    $encoding = [System.Text.Encoding]::GetEncoding(28591)
    $text = $encoding.GetString([System.IO.File]::ReadAllBytes($Path))
    $pattern = '(?im)^(?<prefix>[ \t]*bEnableTrueGaze[ \t]*=[ \t]*)(?<value>true|false|1|0)(?<suffix>[ \t]*(?:;[^\r\n]*)?\r?)$'
    $matches = [regex]::Matches($text, $pattern)

    if ($matches.Count -ne 1) {
        throw "Load-only requires exactly one bEnableTrueGaze boolean in [$Path]; found $($matches.Count)."
    }

    $match = $matches[0]
    if ($match.Groups['value'].Value -notmatch '^(?i:false|0)$') {
        $text = [regex]::Replace($text, $pattern, {
            param($entry)
            $entry.Groups['prefix'].Value + 'false' + $entry.Groups['suffix'].Value
        })
        [System.IO.File]::WriteAllBytes($Path, $encoding.GetBytes($text))
    }

    $verified = $encoding.GetString([System.IO.File]::ReadAllBytes($Path))
    $matches = [regex]::Matches($verified, $pattern)
    if ($matches.Count -ne 1 -or $matches[0].Groups['value'].Value -notmatch '^(?i:false|0)$') {
        throw "Could not verify bEnableTrueGaze=false in [$Path]."
    }
}

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
    if ($ModPath) { throw '-ModPath cannot be combined with -PostRun.' }
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

if ($ModPath) {
    $gameRoot = [System.IO.Path]::GetFullPath($game.Path).TrimEnd('\')
    $deploymentRoot = [System.IO.Path]::GetFullPath($ModPath).TrimEnd('\')
    if ($deploymentRoot.Equals($gameRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        $deploymentRoot.StartsWith($gameRoot + '\', [System.StringComparison]::OrdinalIgnoreCase) -or
        $gameRoot.StartsWith($deploymentRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "-ModPath must be an isolated mod folder outside the game directory: $deploymentRoot"
    }

    $NoLaunch = $true
    Write-Info "Isolated staging mode: $deploymentRoot (game launch disabled)"
}
else {
    $deploymentRoot = Join-Path $game.Path 'Data'
}

$pluginsDir = Join-Path $deploymentRoot 'SKSE\Plugins'

# ===========================================================================
# 1. BUILD
# ===========================================================================
Write-Step '1/4' 'Build'
if ($NoBuild) {
    Write-Info 'Skipped (-NoBuild).'
}
else {
    if (-not $env:VCPKG_ROOT) {
        $vcpkgCandidate = @('C:\vcpkg', 'D:\vcpkg', 'C:\dev\vcpkg', (Join-Path $env:USERPROFILE 'vcpkg')) |
            Where-Object { Test-Path (Join-Path $_ 'scripts\buildsystems\vcpkg.cmake') } |
            Select-Object -First 1
        if ($vcpkgCandidate) { $env:VCPKG_ROOT = $vcpkgCandidate }
    }
    if (-not $env:VCPKG_ROOT) {
        Write-Bad "VCPKG_ROOT is not set and vcpkg was not found in standard locations. Set `$env:VCPKG_ROOT and retry."
        exit 1
    }
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
$repoUi = Join-Path $projectRoot 'skyrim\Interface\TrueGazeIncoming.swf'
if (-not (Test-Path $repoUi)) {
    throw "Incoming UI asset missing: $repoUi. Build the current plugin before deployment."
}
$repoReticleUi = Join-Path $projectRoot 'skyrim\Interface\TrueGazeReticle.swf'
if (-not (Test-Path $repoReticleUi)) {
    throw "Reticle UI asset missing: $repoReticleUi. Build the current plugin before deployment."
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

if ($LoadOnly) {
    if (-not (Test-Path $destIni)) {
        Write-Bad 'Load-only requested, but no deployed TrueGaze.ini is available to disable the engine.'
        exit 1
    }

    Disable-TGEngine -Path $destIni
    Write-Ok 'Load-only enabled in deployed TrueGaze.ini; user settings and file encoding were preserved.'
}

# ---------------------------------------------------------------------------
# Deploy TrueGaze custom visual assets (meshes & textures)
# ---------------------------------------------------------------------------
$destUiDir = Join-Path $deploymentRoot 'Interface'
New-Item -ItemType Directory -Path $destUiDir -Force | Out-Null
Copy-Item -LiteralPath $repoUi -Destination $destUiDir -Force
Copy-Item -LiteralPath $repoReticleUi -Destination $destUiDir -Force
Write-Ok "Interface UI (Incoming + Reticle) -> $destUiDir"

$repoMeshes = Join-Path $projectRoot 'skyrim\meshes\TrueGaze'
$repoTextures = Join-Path $projectRoot 'skyrim\textures\TrueGaze'
$destMeshes = Join-Path $deploymentRoot 'meshes\TrueGaze'
$destTextures = Join-Path $deploymentRoot 'textures\TrueGaze'

if (Test-Path $repoMeshes) {
    if (-not (Test-Path $destMeshes)) {
        New-Item -ItemType Directory -Path $destMeshes -Force | Out-Null
    }
    Copy-Item -Path (Join-Path $repoMeshes '*') -Destination $destMeshes -Force
    $meshCount = (Get-ChildItem -Path $destMeshes -File).Count
    Write-Ok "TrueGaze meshes -> $destMeshes ($meshCount files)"
}

if (Test-Path $repoTextures) {
    if (-not (Test-Path $destTextures)) {
        New-Item -ItemType Directory -Path $destTextures -Force | Out-Null
    }
    Copy-Item -Path (Join-Path $repoTextures '*') -Destination $destTextures -Force
    $texCount = (Get-ChildItem -Path $destTextures -File).Count
    Write-Ok "TrueGaze textures -> $destTextures ($texCount files)"
}

$oarSource = Join-Path $projectRoot 'skyrim\meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json'
if (Test-Path $oarSource) {
    $oarTarget = Join-Path $deploymentRoot 'meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze'
    New-Item -ItemType Directory -Path $oarTarget -Force | Out-Null
    Copy-Item -Path $oarSource -Destination $oarTarget -Force
    Write-Ok "OAR conditions -> $oarTarget"
}

$repoVoice = Join-Path $projectRoot 'skyrim\Sound\Voice\TrueGaze_ExpandedPlay.esp'
if (Test-Path $repoVoice) {
    $destVoice = Join-Path $deploymentRoot 'Sound\Voice\TrueGaze_ExpandedPlay.esp'
    if ((Test-Path $destVoice) -and -not $ModPath) {
        Remove-Item -Path $destVoice -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Path $destVoice -Force | Out-Null
    Copy-Item -Path (Join-Path $repoVoice '*') -Destination $destVoice -Recurse -Force
    $voiceCount = (Get-ChildItem -Path $destVoice -File -Recurse).Count
    Write-Ok "Dialogue voice assets -> $destVoice ($voiceCount file(s))"
}

$repoSeq = Join-Path $projectRoot 'skyrim\SEQ\TrueGaze_ExpandedPlay.seq'
if (Test-Path $repoSeq) {
    $destSeqDir = Join-Path $deploymentRoot 'SEQ'
    New-Item -ItemType Directory -Path $destSeqDir -Force | Out-Null
    $destSeq = Join-Path $destSeqDir 'TrueGaze_ExpandedPlay.seq'
    Copy-Item -LiteralPath $repoSeq -Destination $destSeq -Force
    $seqBytes = (Get-Item $destSeq).Length
    Write-Ok "Start-Game-Enabled Quest SEQ -> $destSeq ($seqBytes bytes)"
}

$eslSource = Join-Path $projectRoot 'skyrim\TrueGaze_ExpandedPlay.esp'
if (Test-Path $eslSource) {
    $eslDest = Join-Path $deploymentRoot 'TrueGaze_ExpandedPlay.esp'
    Copy-Item -LiteralPath $eslSource -Destination $eslDest -Force
    $eslBytes = (Get-Item $eslDest).Length
    Write-Ok "Optional ESL plugin -> $eslDest ($eslBytes bytes, ESL-flagged, Skyrim.esm only)"

    if (-not $ModPath) {
        $pluginsTxt = Join-Path $env:LOCALAPPDATA 'Skyrim Special Edition\plugins.txt'
        if (Test-Path $pluginsTxt) {
            $lines = @(Get-Content -LiteralPath $pluginsTxt)
            $alreadyListed = $false
            for ($idx = 0; $idx -lt $lines.Count; $idx++) {
                if ($lines[$idx].Trim() -match '^\*?TrueGaze_ExpandedPlay\.esp$') {
                    $lines[$idx] = '*TrueGaze_ExpandedPlay.esp'
                    $alreadyListed = $true
                    break
                }
            }
            if (-not $alreadyListed) {
                $lines += '*TrueGaze_ExpandedPlay.esp'
            }
            Set-Content -LiteralPath $pluginsTxt -Value $lines -Encoding ASCII
            Write-Ok "Activated *TrueGaze_ExpandedPlay.esp in $pluginsTxt"
        }
    }
}

# ---------------------------------------------------------------------------
# Remove legacy SkyUI / Papyrus / MCM artifacts from direct game installs.
#
# TrueGaze is deliberately vanilla-UI: configuration is the INI edited through
# TrueGazeConfig.html. There is no ESP, no Papyrus script, no MCM menu and no
# SkyUI dependency. Files left over from the abandoned MCM era are actively
# harmful - a stale TrueGaze.esp in the load order or a TrueGaze_MCM.pex that
# SkyUI still binds produces confusing in-game behaviour and log noise that
# looks like a TrueGaze defect. Never remove unrelated files from a mod staging
# folder; its contents may be managed by MO2/Vortex or another tool.
# ---------------------------------------------------------------------------
if ($ModPath) {
    Write-Info 'Legacy-file cleanup skipped in isolated staging mode.'
}
else {
    $legacyPaths = @(
        @{ Path = (Join-Path $deploymentRoot 'TrueGaze.esp'); Label = 'TrueGaze.esp' }
        @{ Path = (Join-Path $deploymentRoot 'TrueGaze.esl'); Label = 'TrueGaze.esl' }
        @{ Path = (Join-Path $deploymentRoot 'TrueGaze_MCM.pex'); Label = 'TrueGaze_MCM.pex' }
        @{ Path = (Join-Path $deploymentRoot 'TrueGaze.pex'); Label = 'TrueGaze.pex' }
        @{ Path = (Join-Path $deploymentRoot 'MCM\Config\TrueGaze'); Label = 'MCM\Config\TrueGaze\' }
        @{ Path = (Join-Path $deploymentRoot 'Interface\MCM\Config\TrueGaze'); Label = 'Interface\MCM\Config\TrueGaze\' }
    )

    $removed = @()
    foreach ($entry in $legacyPaths) {
        if (Test-Path $entry.Path) {
            Remove-Item -Path $entry.Path -Recurse -Force -ErrorAction SilentlyContinue
            if (-not (Test-Path $entry.Path)) { $removed += $entry.Label }
        }
    }

    # The old MCM shipped six translation files (TrueGaze_<LANG>.txt).
    $transDir = Join-Path $deploymentRoot 'Interface\Translations'
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
}

# ===========================================================================
# 3. VERIFY
# ===========================================================================
Write-Step '3/4' 'Verify (pre-flight health check)'

$verifyCommand = "& '" + $healthScript.Replace("'", "''") + "' -GamePath '" +
    $game.Path.Replace("'", "''") + "' -PluginPath '" +
    $destDll.Replace("'", "''") + "' -DeploymentPath '" +
    $deploymentRoot.Replace("'", "''") + "'"
$verifyEncoded = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($verifyCommand))
$verify = Invoke-TGNative -Exe 'powershell' -Arguments @(
    '-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $verifyEncoded)
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
    if ($ModPath) {
        Write-Host 'Isolated mod staged at:' -ForegroundColor Green
        Write-Host ("  " + $deploymentRoot) -ForegroundColor White
        Write-Host 'Install or enable this folder through MO2/Vortex; the live game was not modified.' -ForegroundColor DarkGray
        exit 0
    }
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
