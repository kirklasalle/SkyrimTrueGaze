#Requires -Version 5.1
# Package-Release.ps1 — builds the release packages for TrueGaze
param(
    [string]$Version = "1.0.5"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$outDir = Join-Path $root "dist"
$stageDir = Join-Path $outDir ".stage"
$zipPath = Join-Path $outDir "TrueGaze-v$Version-SkyrimSE-AE-VR.zip"
$sevenZipPath = Join-Path $outDir "TrueGaze-v$Version-SkyrimSE-AE-VR.7z"
$symPath = Join-Path $outDir "TrueGaze-v$Version-Symbols.zip"
$relDll = Join-Path $root "build\windows-release\Release\TrueGaze.dll"
$relPdb = Join-Path $root "build\windows-release\Release\TrueGaze.pdb"
$packScript = Join-Path $root "scripts\package_7z.py"

Write-Host "=== TrueGaze v$Version - Packaging ===" -ForegroundColor Cyan

if (-not (Test-Path $relDll)) {
    throw "Release binary not found at $relDll. Build the release preset first."
}

# Clean up old artefacts
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory $stageDir | Out-Null

# --- SKSE Plugin -------------------------------------------------------
$pluginDir = Join-Path $stageDir "SKSE\Plugins"
New-Item -ItemType Directory $pluginDir -Force | Out-Null
Copy-Item $relDll $pluginDir
Copy-Item (Join-Path $root "skyrim\SKSE\Plugins\TrueGaze.ini") $pluginDir

# --- Configurator -------------------------------------------------------
Copy-Item (Join-Path $root "skyrim\TrueGazeConfig.html")             $stageDir
Copy-Item (Join-Path $root "skyrim\Launch-TrueGazeConfig.cmd")       $stageDir
Copy-Item (Join-Path $root "skyrim\TrueGaze_Configurator_Guide.txt") $stageDir

# --- OAR animation conditions (no hand-crafted NIFs) --------------------
$oarDst = Join-Path $stageDir "meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze"
New-Item -ItemType Directory $oarDst -Force | Out-Null
Copy-Item (Join-Path $root "skyrim\meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json") $oarDst

# --- Verify contents ---------------------------------------------------
Write-Host "`nPackage contents:" -ForegroundColor Yellow
Get-ChildItem $stageDir -Recurse | Where-Object { -not $_.PSIsContainer } |
ForEach-Object { "  " + $_.FullName.Replace($stageDir + "\", "") }

# --- Zip ----------------------------------------------------------------
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath
$zipSizeKB = [math]::Round((Get-Item $zipPath).Length / 1KB, 1)
$zipHash = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash
Write-Host "Created: $zipPath [$zipSizeKB KB]" -ForegroundColor Green

# --- 7z (via py7zr) ------------------------------------------------------
if (Test-Path $sevenZipPath) { Remove-Item $sevenZipPath -Force }
$sevenZipDone = $false
try {
    python $packScript $stageDir $sevenZipPath
    if (Test-Path $sevenZipPath) {
        $sevenZipSizeKB = [math]::Round((Get-Item $sevenZipPath).Length / 1KB, 1)
        $sevenZipHash = (Get-FileHash -Path $sevenZipPath -Algorithm SHA256).Hash
        Write-Host "Created: $sevenZipPath [$sevenZipSizeKB KB]" -ForegroundColor Green
        $sevenZipDone = $true
    }
} catch {
    Write-Warning "Failed to create 7z archive: $_"
}

# --- Symbols zip --------------------------------------------------------
$symDone = $false
if (Test-Path $relPdb) {
    if (Test-Path $symPath) { Remove-Item $symPath -Force }
    Compress-Archive -Path $relPdb -DestinationPath $symPath
    $symSizeKB = [math]::Round((Get-Item $symPath).Length / 1KB, 1)
    $symHash = (Get-FileHash -Path $symPath -Algorithm SHA256).Hash
    Write-Host "Created: $symPath [$symSizeKB KB]" -ForegroundColor Green
    $symDone = $true
}

# --- Cleanup stage -------------------------------------------------------
Remove-Item $stageDir -Recurse -Force

Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  TrueGaze v$Version Release Package Summary"             -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "ZIP Mod Package:  $zipPath" -ForegroundColor White
Write-Host "Size:             $zipSizeKB KB" -ForegroundColor White
Write-Host "SHA-256:          $zipHash" -ForegroundColor Yellow

if ($sevenZipDone) {
    Write-Host ""
    Write-Host "7Z Mod Package:   $sevenZipPath" -ForegroundColor White
    Write-Host "Size:             $sevenZipSizeKB KB" -ForegroundColor White
    Write-Host "SHA-256:          $sevenZipHash" -ForegroundColor Yellow
}

if ($symDone) {
    Write-Host ""
    Write-Host "Debug Symbols:    $symPath" -ForegroundColor White
    Write-Host "Size:             $symSizeKB KB" -ForegroundColor White
    Write-Host "SHA-256:          $symHash" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Packaging complete." -ForegroundColor Cyan
