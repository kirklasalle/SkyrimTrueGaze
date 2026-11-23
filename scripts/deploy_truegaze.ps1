# TrueGaze deploy + hash verification (run with game NOT running)
param(
    [string]$GamePath = ''
)
$ErrorActionPreference = 'Stop'
$proj = Split-Path -Parent $PSScriptRoot
if (-not $GamePath) {
    foreach ($k in 'HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition', 'HKLM:\SOFTWARE\Bethesda Softworks\Skyrim Special Edition') {
        if (Test-Path $k) {
            $p = (Get-ItemProperty $k -Name 'Installed Path' -ErrorAction SilentlyContinue).'Installed Path'
            if ($p -and (Test-Path (Join-Path $p 'SkyrimSE.exe'))) { $GamePath = $p.TrimEnd('\'); break }
        }
    }
}
if (-not $GamePath) { throw 'Skyrim installation not found; pass -GamePath.' }
$game = Join-Path $GamePath 'Data'

if (Get-Process SkyrimSE -ErrorAction SilentlyContinue) {
    Write-Output 'GAME RUNNING - ABORT DEPLOY'
    exit 1
}

$pairs = @(
    @{ src = "$proj\skyrim\SKSE\Plugins\TrueGaze.dll"; dst = "$game\SKSE\Plugins\TrueGaze.dll" },
    @{ src = "$proj\skyrim\meshes\TrueGaze\GazeRegionPanel.nif"; dst = "$game\meshes\TrueGaze\GazeRegionPanel.nif" },
    @{ src = "$proj\skyrim\meshes\TrueGaze\GazeBeam.nif"; dst = "$game\meshes\TrueGaze\GazeBeam.nif" },
    @{ src = "$proj\skyrim\textures\TrueGaze\GazeBeamGlow.dds"; dst = "$game\textures\TrueGaze\GazeBeamGlow.dds" },
    @{ src = "$proj\skyrim\textures\TrueGaze\GazeRegionPanel.dds"; dst = "$game\textures\TrueGaze\GazeRegionPanel.dds" },
    @{ src = "$proj\skyrim\TrueGaze_ExpandedPlay.esp"; dst = "$game\TrueGaze_ExpandedPlay.esp" },
    @{ src = "$proj\skyrim\SEQ\TrueGaze_ExpandedPlay.seq"; dst = "$game\SEQ\TrueGaze_ExpandedPlay.seq" }
)

$allOk = $true
foreach ($p in $pairs) {
    $dstDir = Split-Path $p.dst -Parent
    New-Item -ItemType Directory -Force $dstDir | Out-Null
    Copy-Item $p.src $p.dst -Force
    $h1 = (Get-FileHash $p.src -Algorithm SHA256).Hash
    $h2 = (Get-FileHash $p.dst -Algorithm SHA256).Hash
    $ok = ($h1 -eq $h2)
    if (-not $ok) { $allOk = $false }
    $name = Split-Path $p.dst -Leaf
    Write-Output ("{0}  {1}  {2}" -f ($(if ($ok) { 'MATCH' } else { 'MISMATCH' })), $name, $h2.Substring(0, 8))
}

if ($allOk) { Write-Output 'DEPLOY VERIFIED: all hashes match' }
else { Write-Output 'HASH MISMATCH DETECTED' }
