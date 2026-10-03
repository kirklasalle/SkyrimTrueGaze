# TrueGaze deploy + hash verification (run with game NOT running)
$ErrorActionPreference = 'Stop'
$game = 'G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data'

if (Get-Process SkyrimSE -ErrorAction SilentlyContinue) {
    Write-Output 'GAME RUNNING - ABORT DEPLOY'
    exit 1
}

$proj = 'd:\Projects\SkyrimTrueGaze'
$pairs = @(
    @{ src = "$proj\skyrim\SKSE\Plugins\TrueGaze.dll"; dst = "$game\SKSE\Plugins\TrueGaze.dll" },
    @{ src = "$proj\scratch\arrow_extract\meshes\marker_arrow.nif"; dst = "$game\meshes\marker_arrow.nif" },
    @{ src = "$proj\skyrim\meshes\TrueGaze\GazeRegionPanel.nif"; dst = "$game\meshes\TrueGaze\GazeRegionPanel.nif" },
    @{ src = "$proj\skyrim\meshes\TrueGaze\GazeBeam.nif"; dst = "$game\meshes\TrueGaze\GazeBeam.nif" },
    @{ src = "$proj\skyrim\textures\TrueGaze\GazeBeamGlow.dds"; dst = "$game\textures\TrueGaze\GazeBeamGlow.dds" },
    @{ src = "$proj\skyrim\textures\TrueGaze\GazeRegionPanel.dds"; dst = "$game\textures\TrueGaze\GazeRegionPanel.dds" }
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
