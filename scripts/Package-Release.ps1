#Requires -Version 5.1
# Package-Release.ps1 — builds the Nexus Mods release zip for TrueGaze
param(
    [string]$Version = "1.0.4"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$outDir = Join-Path $root "dist"
$stageDir = Join-Path $outDir ".stage"
$zipPath = Join-Path $outDir "TrueGaze-v$Version-SkyrimSE-AE-VR.zip"
$symPath = Join-Path $outDir "TrueGaze-v$Version-Symbols.zip"
$relDll = Join-Path $root "build\windows-release\Release\TrueGaze.dll"
$relPdb = Join-Path $root "build\windows-release\Release\TrueGaze.pdb"

Write-Host "=== TrueGaze v$Version — Packaging ===" -ForegroundColor Cyan

# Clean up old artefacts
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory $stageDir | Out-Null

# --- SKSE Plugin -------------------------------------------------------
$pluginDir = Join-Path $stageDir "SKSE\Plugins"
New-Item -ItemType Directory $pluginDir -Force | Out-Null
Copy-Item $relDll $pluginDir
Copy-Item (Join-Path $root "skyrim\SKSE\Plugins\TrueGaze.ini") $pluginDir

# --- Configurator -------------------------------------------------------
Copy-Item (Join-Path $root "skyrim\TrueGazeConfig.html")         $stageDir
Copy-Item (Join-Path $root "skyrim\Launch-TrueGazeConfig.cmd")   $stageDir
Copy-Item (Join-Path $root "skyrim\TrueGaze_Configurator_Guide.txt") $stageDir

# --- OAR animation conditions (no hand-crafted NIFs) --------------------
$oarDst = Join-Path $stageDir "meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze"
New-Item -ItemType Directory $oarDst -Force | Out-Null
Copy-Item (Join-Path $root "skyrim\meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json") $oarDst

# --- Verify contents ---------------------------------------------------
Write-Host "`nPackage contents:" -ForegroundColor Yellow
Get-ChildItem $stageDir -Recurse | Where-Object { !$_.PSIsContainer } |
ForEach-Object { "  " + $_.FullName.Replace($stageDir + "\", "") }

# --- Zip ----------------------------------------------------------------
if (Test-Path $zipPath) { Remove-Item $zipPath }
Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath
$zipSizeKB = [math]::Round((Get-Item $zipPath).Length / 1KB)
Write-Host "Created: $zipPath  (${zipSizeKB}KB)" -ForegroundColor Green

# --- Symbols zip --------------------------------------------------------
if (Test-Path $relPdb) {
    if (Test-Path $symPath) { Remove-Item $symPath }
    Compress-Archive -Path $relPdb -DestinationPath $symPath
    $symSizeKB = [math]::Round((Get-Item $symPath).Length / 1KB)
    Write-Host "Created: $symPath  (${symSizeKB}KB)" -ForegroundColor Green
}

# --- Cleanup stage -------------------------------------------------------
Remove-Item $stageDir -Recurse -Force
Write-Host 'Packaging complete.' -ForegroundColor Cyan
