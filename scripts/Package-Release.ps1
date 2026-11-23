#Requires -Version 5.1
# Package-Release.ps1 — builds the release packages for TrueGaze
param(
    # Audit 2026-10-15 F-07: defaults to project(VERSION) in CMakeLists.txt
    # (previously a hardcoded, stale "1.0.5").
    [string]$Version = (& (Join-Path $PSScriptRoot 'Get-TrueGazeVersion.ps1'))
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
$incomingUi = Join-Path $root "skyrim\Interface\TrueGazeIncoming.swf"
if (-not (Test-Path $incomingUi)) {
    throw "Incoming UI missing at $incomingUi. Build the current Release target first."
}
$reticleUi = Join-Path $root "skyrim\Interface\TrueGazeReticle.swf"
if (-not (Test-Path $reticleUi)) {
    throw "Reticle UI missing at $reticleUi. Build the current Release target first."
}

# Clean up old artefacts
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory $stageDir | Out-Null

# --- SKSE Plugin -------------------------------------------------------
$pluginDir = Join-Path $stageDir "SKSE\Plugins"
New-Item -ItemType Directory $pluginDir -Force | Out-Null
Copy-Item $relDll $pluginDir
Copy-Item (Join-Path $root "skyrim\SKSE\Plugins\TrueGaze.ini") $pluginDir
New-Item -ItemType Directory (Join-Path $stageDir "Interface") -Force | Out-Null
Copy-Item $incomingUi (Join-Path $stageDir "Interface")
Copy-Item $reticleUi (Join-Path $stageDir "Interface")
foreach ($assetDirectory in @("meshes\TrueGaze", "textures\TrueGaze")) {
    $target = Join-Path $stageDir $assetDirectory
    New-Item -ItemType Directory $target -Force | Out-Null
    Copy-Item (Join-Path $root ("skyrim\" + $assetDirectory + "\*")) $target -Force
}
# SEC-01: Do NOT redistribute Bethesda's vanilla marker_arrow.nif — Skyrim loads
# meshes\marker_arrow.nif directly from the user's own Skyrim - Meshes0.bsa (and
# clean-room meshes\TrueGaze\GazeBeam.nif is already staged above).
$expandedEsl = Join-Path $root "skyrim\TrueGaze_ExpandedPlay.esp"
if (Test-Path $expandedEsl) {
    Copy-Item $expandedEsl (Join-Path $stageDir "TrueGaze_ExpandedPlay.esp") -Force
}
# ARCH-01: Stage start-game-enabled quest .seq & clean-room timing .wav voice assets
# so packaged releases match Deploy-TrueGaze.ps1 100%.
$expandedSeq = Join-Path $root "skyrim\SEQ\TrueGaze_ExpandedPlay.seq"
if (Test-Path $expandedSeq) {
    $seqStageDir = Join-Path $stageDir "SEQ"
    New-Item -ItemType Directory $seqStageDir -Force | Out-Null
    Copy-Item $expandedSeq (Join-Path $seqStageDir "TrueGaze_ExpandedPlay.seq") -Force
}
$expandedVoice = Join-Path $root "skyrim\Sound\Voice\TrueGaze_ExpandedPlay.esp"
if (Test-Path $expandedVoice) {
    $voiceStageDir = Join-Path $stageDir "Sound\Voice\TrueGaze_ExpandedPlay.esp"
    New-Item -ItemType Directory $voiceStageDir -Force | Out-Null
    Copy-Item (Join-Path $expandedVoice "*") $voiceStageDir -Recurse -Force
}

# --- Configurator -------------------------------------------------------
Copy-Item (Join-Path $root "TrueGazeConfig.html")                    $stageDir -Force
Copy-Item (Join-Path $root "Launch-TrueGazeConfig.cmd")              $stageDir -Force
Copy-Item (Join-Path $root "TrueGaze_Configurator_Guide.txt")        $stageDir -Force
$toolsDir = Join-Path $stageDir "tools\TrueGazeConfig"
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
Copy-Item (Join-Path $root "scripts\Launch-TrueGazeConfig.ps1")      $toolsDir -Force
Copy-Item (Join-Path $root "scripts\TrueGazeBridgeServer.ps1")       $toolsDir -Force
$docsDir = Join-Path $stageDir "docs"
New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
foreach ($doc in @("USER_GUIDE.md", "EXPANDED_PLAY_GUIDE.md", "TRUEGAZE_LORE_AND_STORY.md", "REALTIME_PERSUASION_AND_TRANSACTIONS_GDD.md", "GAMEPLAY_DESIGN_DOCUMENT.md", "IMPLEMENTATION_PLAN_EXPANDED_PLAY_G0_G3.md", "WP11_FACTION_SIGHT_VOICE_PERSUASION_DESIGN.md", "RD_INNOVATION_PROGRAM.md", "AUDIT_REMEDIATION_LOG.md", "DEVELOPER_GUIDE.md", "CONSOLE_COMMANDS_CARD.md", "CONFIGURATION_AUDIT.md", "INCOMING_GAZE_PANEL_DESIGN.md", "STATUS.md", "TEST_SCENARIO.md")) {
    $docPath = Join-Path $root ("docs\" + $doc)
    if (Test-Path -LiteralPath $docPath) {
        Copy-Item -LiteralPath $docPath -Destination $docsDir
    }
}
$loreImgSrc = Join-Path $root "docs\images\lore"
if (Test-Path $loreImgSrc) {
    $loreImgDst = Join-Path $docsDir "images\lore"
    New-Item -ItemType Directory -Path $loreImgDst -Force | Out-Null
    Copy-Item (Join-Path $loreImgSrc "*") $loreImgDst -Force
}
foreach ($img in @("truegaze_hero_v200.png", "expanded_play_architecture_and_switches.png", "expanded_play_hud_and_regions.png", "expanded_play_moth_stillness_and_mechanics.png", "promo_plate_01_realtime_oblivion_persuasion.png", "promo_plate_02_two_layer_feature_poster.png")) {
    $imgPath = Join-Path $root ("docs\images\" + $img)
    if (Test-Path -LiteralPath $imgPath) {
        $imgDst = Join-Path $docsDir "images"
        New-Item -ItemType Directory -Path $imgDst -Force | Out-Null
        Copy-Item -LiteralPath $imgPath -Destination $imgDst -Force
    }
}

# --- OAR animation conditions (no hand-crafted NIFs) --------------------
$oarDst = Join-Path $stageDir "meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze"
New-Item -ItemType Directory $oarDst -Force | Out-Null
Copy-Item (Join-Path $root "skyrim\meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json") $oarDst

# --- Licensing & GPL-3.0 Corresponding Source Offer ---------------------
Copy-Item (Join-Path $root "LICENSE") $stageDir
$commonLibLicDst = Join-Path $stageDir "licenses\CommonLibSSE-NG"
New-Item -ItemType Directory $commonLibLicDst -Force | Out-Null
Copy-Item (Join-Path $root "extern\CommonLibSSE-NG\COPYING.txt") $commonLibLicDst
Copy-Item (Join-Path $root "extern\CommonLibSSE-NG\EXCEPTIONS.md") $commonLibLicDst
Copy-Item (Join-Path $root "extern\CommonLibSSE-NG\licenses\LICENSE-MIT.txt") $commonLibLicDst

$oarLicDst = Join-Path $stageDir "licenses\OpenAnimationReplacer-API"
New-Item -ItemType Directory $oarLicDst -Force | Out-Null
Copy-Item (Join-Path $root "extern\OpenAnimationReplacer-API\COPYING") $oarLicDst
Copy-Item (Join-Path $root "extern\OpenAnimationReplacer-API\EXCEPTIONS") $oarLicDst
Copy-Item (Join-Path $root "extern\OpenAnimationReplacer-API\NOTICE.md") $oarLicDst

$sourceOffer = @"
TrueGaze v$Version - Licensing & Corresponding Source Offer (GPL-3.0 § 6)

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
[System.IO.File]::WriteAllText((Join-Path $stageDir "SOURCE_OFFER.txt"), $sourceOffer, [System.Text.Encoding]::UTF8)

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
}
catch {
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
