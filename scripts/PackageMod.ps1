<#
.SYNOPSIS
    Packages TrueGaze into a release-ready archive for Nexus Mods, MO2, and Vortex.
.DESCRIPTION
    Builds the latest Release x64 binary, verifies all assets (OAR, SKSE),
    and creates a clean distributable zip archive in the dist/ folder.
#>

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$skyrimDir = Join-Path $projectRoot "skyrim"
$distDir = Join-Path $projectRoot "dist"
$releaseDll = Join-Path $projectRoot "build\windows-release\Release\TrueGaze.dll"
$pluginTarget = Join-Path $skyrimDir "SKSE\Plugins\TrueGaze.dll"
$beamAssetPath = Join-Path $skyrimDir "meshes\effects\fxsoulcairnbeam.nif"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  TrueGaze Mod Packaging Pipeline                      " -ForegroundColor Cyan
Write-Host "  An HCEP Product by Kirk LaSalle                      " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Compile latest Release binary
Write-Host "`n[1/4] Building latest Release x64 binary..." -ForegroundColor Yellow
Set-Location $projectRoot
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = 'D:\vcpkg' }
& cmake --preset windows-release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configure failed with exit code $LASTEXITCODE"
}
& cmake --build --preset release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake build failed with exit code $LASTEXITCODE"
}

# 2. Deploy binary and configurator suite to skyrim structure
Write-Host "`n[2/4] Deploying TrueGaze.dll and Configurator suite to skyrim/..." -ForegroundColor Yellow
Copy-Item -Path $releaseDll -Destination $pluginTarget -Force

# Stage Configurator files for Mod Organizer 2, Vortex, and manual modders
Copy-Item -Path (Join-Path $projectRoot "TrueGazeConfig.html") -Destination (Join-Path $skyrimDir "TrueGazeConfig.html") -Force
Copy-Item -Path (Join-Path $projectRoot "Launch-TrueGazeConfig.cmd") -Destination (Join-Path $skyrimDir "Launch-TrueGazeConfig.cmd") -Force
$toolsDir = Join-Path $skyrimDir "tools\TrueGazeConfig"
if (!(Test-Path $toolsDir)) { New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null }
Copy-Item -Path (Join-Path $projectRoot "scripts\Launch-TrueGazeConfig.ps1") -Destination (Join-Path $toolsDir "Launch-TrueGazeConfig.ps1") -Force
Copy-Item -Path (Join-Path $projectRoot "scripts\TrueGazeBridgeServer.ps1") -Destination (Join-Path $toolsDir "TrueGazeBridgeServer.ps1") -Force

Write-Host "  OK   TrueGaze Configurator HTML and Launchers staged into skyrim/" -ForegroundColor DarkGray

# 3. Create dist output directory
if (!(Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir | Out-Null
}

# 4. Generate release archives
$version = "1.0.0"
$archiveName = "TrueGaze-v$version-SkyrimSE-AE-VR.zip"
$archivePath = Join-Path $distDir $archiveName
$symbolsName = "TrueGaze-v$version-Symbols.zip"
$symbolsPath = Join-Path $distDir $symbolsName
$releasePdb = Join-Path $projectRoot "build\windows-release\Release\TrueGaze.pdb"

if (Test-Path $archivePath) {
    Remove-Item $archivePath -Force
}
if (Test-Path $symbolsPath) {
    Remove-Item $symbolsPath -Force
}

Write-Host "`n[3/5] Compressing mod package into $archiveName..." -ForegroundColor Yellow
Compress-Archive -Path "$skyrimDir\*" -DestinationPath $archivePath

Write-Host "`n[4/5] Compressing companion debug symbols into $symbolsName..." -ForegroundColor Yellow
if (Test-Path $releasePdb) {
    Compress-Archive -Path $releasePdb -DestinationPath $symbolsPath
    Write-Host "  OK   Companion PDB included: TrueGaze.pdb" -ForegroundColor DarkGray
} else {
    Write-Warning "PDB not found at $releasePdb; symbols archive omitted."
}

# 5. Compute SHA-256 Hashes & Summary
$mainHash = (Get-FileHash -Path $archivePath -Algorithm SHA256).Hash
$mainSize = (Get-Item $archivePath).Length / 1KB

Write-Host ""
Write-Host "========================================================" -ForegroundColor Green
Write-Host "[5/5] Production Release Packages Created Successfully!" -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Green
Write-Host ("  Mod Package:    " + $archivePath) -ForegroundColor White
Write-Host ("  Size:           " + [math]::Round($mainSize, 2) + " KB") -ForegroundColor White
Write-Host ("  SHA-256:        " + $mainHash) -ForegroundColor Cyan

if (Test-Path $symbolsPath) {
    $symHash = (Get-FileHash -Path $symbolsPath -Algorithm SHA256).Hash
    $symSize = (Get-Item $symbolsPath).Length / 1KB
    Write-Host ""
    Write-Host ("  Symbols:        " + $symbolsPath) -ForegroundColor White
    Write-Host ("  Size:           " + [math]::Round($symSize, 2) + " KB") -ForegroundColor White
    Write-Host ("  SHA-256:        " + $symHash) -ForegroundColor Cyan
}

Write-Host ""
Write-Host "Ready for publication on Nexus Mods and GitHub Releases!" -ForegroundColor Green

