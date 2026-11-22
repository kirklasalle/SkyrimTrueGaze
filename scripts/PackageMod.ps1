<#
.SYNOPSIS
    Packages TrueGaze into a release-ready archive for mod managers (MO2, Vortex).
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

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  TrueGaze Mod Packaging Pipeline                      " -ForegroundColor Cyan
Write-Host "  An HCEP Product by Kirk LaSalle                      " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Compile latest Release binary
Write-Host "`n[1/5] Building latest Release x64 binary..." -ForegroundColor Yellow
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
Write-Host "`n[2/5] Deploying TrueGaze.dll and Configurator suite to skyrim/..." -ForegroundColor Yellow
Copy-Item -Path $releaseDll -Destination $pluginTarget -Force
if (-not (Test-Path (Join-Path $skyrimDir "Interface\TrueGazeIncoming.swf"))) {
    throw "Generated incoming UI asset missing after build."
}
if (-not (Test-Path (Join-Path $skyrimDir "Interface\TrueGazeReticle.swf"))) {
    throw "Generated reticle UI asset missing after build."
}

# Stage Configurator files for Mod Organizer 2, Vortex, and manual modders
Copy-Item -Path (Join-Path $projectRoot "TrueGazeConfig.html") -Destination (Join-Path $skyrimDir "TrueGazeConfig.html") -Force
Copy-Item -Path (Join-Path $projectRoot "Launch-TrueGazeConfig.cmd") -Destination (Join-Path $skyrimDir "Launch-TrueGazeConfig.cmd") -Force
$toolsDir = Join-Path $skyrimDir "tools\TrueGazeConfig"
if (!(Test-Path $toolsDir)) { New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null }
Copy-Item -Path (Join-Path $projectRoot "scripts\Launch-TrueGazeConfig.ps1") -Destination (Join-Path $toolsDir "Launch-TrueGazeConfig.ps1") -Force
Copy-Item -Path (Join-Path $projectRoot "scripts\TrueGazeBridgeServer.ps1") -Destination (Join-Path $toolsDir "TrueGazeBridgeServer.ps1") -Force
$docsDir = Join-Path $skyrimDir "docs"
New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
foreach ($doc in @("USER_GUIDE.md", "EXPANDED_PLAY_GUIDE.md", "TRUEGAZE_LORE_AND_STORY.md", "REALTIME_PERSUASION_AND_TRANSACTIONS_GDD.md", "GAMEPLAY_DESIGN_DOCUMENT.md", "IMPLEMENTATION_PLAN_EXPANDED_PLAY_G0_G3.md", "DEVELOPER_GUIDE.md", "CONSOLE_COMMANDS_CARD.md", "CONFIGURATION_AUDIT.md", "INCOMING_GAZE_PANEL_DESIGN.md", "STATUS.md", "TEST_SCENARIO.md")) {
    $docPath = Join-Path $projectRoot ("docs\" + $doc)
    if (Test-Path -LiteralPath $docPath) {
        Copy-Item -LiteralPath $docPath -Destination $docsDir -Force
    }
}
$loreImgSrc = Join-Path $projectRoot "docs\images\lore"
if (Test-Path $loreImgSrc) {
    $loreImgDst = Join-Path $docsDir "images\lore"
    New-Item -ItemType Directory -Path $loreImgDst -Force | Out-Null
    Copy-Item (Join-Path $loreImgSrc "*") $loreImgDst -Force
}
foreach ($img in @("truegaze_hero_v200.png", "expanded_play_architecture_and_switches.png", "expanded_play_hud_and_regions.png", "expanded_play_moth_stillness_and_mechanics.png", "promo_plate_01_realtime_oblivion_persuasion.png", "promo_plate_02_two_layer_feature_poster.png")) {
    $imgPath = Join-Path $projectRoot ("docs\images\" + $img)
    if (Test-Path -LiteralPath $imgPath) {
        $imgDst = Join-Path $docsDir "images"
        New-Item -ItemType Directory -Path $imgDst -Force | Out-Null
        Copy-Item -LiteralPath $imgPath -Destination $imgDst -Force
    }
}

# Stage GPL-3.0 license, third-party notices/exceptions, and source offer in skyrim/
Copy-Item -LiteralPath (Join-Path $projectRoot "LICENSE") -Destination (Join-Path $skyrimDir "LICENSE") -Force
$commonLibLicDst = Join-Path $skyrimDir "licenses\CommonLibSSE-NG"
New-Item -ItemType Directory -Path $commonLibLicDst -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "extern\CommonLibSSE-NG\COPYING.txt") -Destination $commonLibLicDst -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "extern\CommonLibSSE-NG\EXCEPTIONS.md") -Destination $commonLibLicDst -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "extern\CommonLibSSE-NG\licenses\LICENSE-MIT.txt") -Destination $commonLibLicDst -Force

$oarLicDst = Join-Path $skyrimDir "licenses\OpenAnimationReplacer-API"
New-Item -ItemType Directory -Path $oarLicDst -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "extern\OpenAnimationReplacer-API\COPYING") -Destination $oarLicDst -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "extern\OpenAnimationReplacer-API\EXCEPTIONS") -Destination $oarLicDst -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "extern\OpenAnimationReplacer-API\NOTICE.md") -Destination $oarLicDst -Force

$pkgVersion = & (Join-Path $PSScriptRoot 'Get-TrueGazeVersion.ps1')
$sourceOffer = @"
TrueGaze v$pkgVersion - Licensing & Corresponding Source Offer (GPL-3.0 § 6)

TrueGaze.dll and its associated software components are licensed under the GNU
General Public License, version 3 (see LICENSE). Third-party license notices and
exceptions for CommonLibSSE-NG (GPL-3.0 + Modding & Linking Exceptions) and the
Open Animation Replacer Conditions API (GPL-3.0 + Modding & Linking Exceptions)
are included under licenses\.

Corresponding Source Code:
  https://github.com/kirklasalle/SkyrimTrueGaze

Written Offer:
  In accordance with GPL-3.0 Section 6(b), you may obtain the complete
  machine-readable Corresponding Source for this binary release from the
  repository above or by contacting the maintainer via the repository issues page.
"@
[System.IO.File]::WriteAllText((Join-Path $skyrimDir "SOURCE_OFFER.txt"), $sourceOffer, [System.Text.Encoding]::UTF8)

Write-Host "  OK   TrueGaze Configurator HTML and Launchers staged into skyrim/" -ForegroundColor DarkGray

# 3. Create dist output directory
if (!(Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir | Out-Null
}

# 4. Generate release archives
# Audit 2026-10-15 F-07: single source of truth (was a stale hardcoded "1.0.5").
$version = & (Join-Path $PSScriptRoot 'Get-TrueGazeVersion.ps1')
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
}
else {
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
Write-Host "Ready for publication on GitHub Releases!" -ForegroundColor Green

