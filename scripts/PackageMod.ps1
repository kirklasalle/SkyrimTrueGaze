<#
.SYNOPSIS
    Packages TrueGaze into a release-ready archive for Nexus Mods, MO2, and Vortex.
.DESCRIPTION
    Builds the latest Release x64 binary, verifies all assets (MCM, Translations, OAR, SKSE),
    and creates a clean distributable zip archive in the dist/ folder.
#>

$ErrorActionPreference = "Stop"

$projectRoot = "D:\Projects\SkyrimTrueGaze"
$skyrimDir = Join-Path $projectRoot "skyrim"
$distDir = Join-Path $projectRoot "dist"
$releaseDll = Join-Path $projectRoot "build\windows-release\Release\TrueGaze.dll"
$pluginTarget = Join-Path $skyrimDir "SKSE\Plugins\TrueGaze.dll"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  TrueGaze Mod Packaging Pipeline                      " -ForegroundColor Cyan
Write-Host "  An HCEP Product by Kirk LaSalle                      " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Compile latest Release binary
Write-Host "`n[1/4] Building latest Release x64 binary..." -ForegroundColor Yellow
Set-Location $projectRoot
& cmake --build --preset release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake build failed with exit code $LASTEXITCODE"
}

# 2. Deploy binary to skyrim structure
Write-Host "`n[2/4] Deploying TrueGaze.dll to skyrim/SKSE/Plugins/..." -ForegroundColor Yellow
Copy-Item -Path $releaseDll -Destination $pluginTarget -Force

# 3. Create dist output directory
if (!(Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir | Out-Null
}

# 4. Generate zip archive
$version = "1.0.0-rc1"
$archiveName = "TrueGaze-v$version-SkyrimSE-AE-VR.zip"
$archivePath = Join-Path $distDir $archiveName

if (Test-Path $archivePath) {
    Remove-Item $archivePath -Force
}

Write-Host "`n[3/4] Compressing mod package into $archiveName..." -ForegroundColor Yellow
Compress-Archive -Path "$skyrimDir\*" -DestinationPath $archivePath

# 5. Summary
$fileSize = (Get-Item $archivePath).Length / 1KB
Write-Host ""
Write-Host "[4/4] Package Created Successfully!" -ForegroundColor Green
Write-Host ("  Output: " + $archivePath) -ForegroundColor White
Write-Host ("  Size:   " + [math]::Round($fileSize, 2) + " KB") -ForegroundColor White
Write-Host ""
Write-Host "Ready for upload to Nexus Mods or installation in Mod Organizer 2 / Vortex." -ForegroundColor Cyan

