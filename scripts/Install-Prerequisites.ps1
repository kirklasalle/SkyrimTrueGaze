<#
.SYNOPSIS
    Installs the two Nexus-only prerequisites for TrueGaze: SKSE64 and the
    Address Library.
.DESCRIPTION
    SKSE and the Address Library cannot be downloaded programmatically:

      * SKSE 2.3.1 (the AE build for game 1.7.104) is distributed via Nexus
        only. silverlock.org links to Nexus for the current AE build; the only
        direct .7z links are for GOG (1.6.1179) and SE (1.5.97), neither of
        which is this game version.
      * The Address Library "All in one" is Nexus-only, and its permissions
        forbid redistribution.

    So this script does everything EXCEPT the one manual step no script may
    do: it tells you the exact file to download and where to save it, and it
    locates, extracts, copies and verifies it once it is there.

    It uses 7-Zip (7z.exe) for extraction. 7-Zip is a hard requirement here:
    Windows tar cannot decompress LZMA 7z archives.

.PARAMETER DownloadDir
    Where the script looks for and tells you to save the downloads.
    Default: the repository's downloads/ folder.

.PARAMETER GamePath
    Skyrim install directory. Auto-detected when omitted.

.PARAMETER SevenZip
    Path to 7z.exe. Auto-detected when omitted.

.PARAMETER AutoInstall
    Install every archive already present in DownloadDir without prompting.
    Useful for repeat runs.

.EXAMPLE
    .\Install-Prerequisites.ps1
    .\Install-Prerequisites.ps1 -AutoInstall

.NOTES
    Part of TrueGaze(TM), an HCEP product by Kirk LaSalle.
#>

[CmdletBinding()]
param(
    [string]$DownloadDir,
    [string]$GamePath,
    [string]$SevenZip,
    [switch]$AutoInstall
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $DownloadDir) { $DownloadDir = Join-Path $projectRoot 'downloads' }

function Write-Step { param([string]$t) Write-Host ''; Write-Host $t -ForegroundColor Cyan; Write-Host ('-' * 60) -ForegroundColor DarkGray }
function Write-Ok   { param([string]$t) Write-Host ("  OK    " + $t) -ForegroundColor Green }
function Write-Bad  { param([string]$t) Write-Host ("  FAIL  " + $t) -ForegroundColor Red }
function Write-Warn { param([string]$t) Write-Host ("  WARN  " + $t) -ForegroundColor Yellow }
function Write-Info { param([string]$t) Write-Host ("  ..    " + $t) -ForegroundColor DarkGray }

Write-Host ''
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host '  TrueGaze - Prerequisite Installer'                       -ForegroundColor Cyan
Write-Host '  SKSE64 + Address Library'                                 -ForegroundColor Cyan
Write-Host '  An HCEP Product by Kirk LaSalle'                          -ForegroundColor Cyan
Write-Host '============================================================' -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Locate 7-Zip
# ---------------------------------------------------------------------------
Write-Step '1. Locate 7-Zip'

$sevenZipExe = $null
$known = @(
    $SevenZip,
    'G:\Program Files\7-Zip\7z.exe',
    'C:\Program Files\7-Zip\7z.exe',
    'C:\Program Files (x86)\7-Zip\7z.exe',
    "$env:ProgramFiles\7-Zip\7z.exe",
    "$env:ProgramFiles(x86)\7-Zip\7z.exe"
) | Where-Object { $_ }

foreach ($candidate in $known) {
    if ($candidate -and (Test-Path $candidate)) {
        $sevenZipExe = $candidate
        break
    }
}

if (-not $sevenZipExe) {
    $where = & where.exe 7z 2>$null | Select-Object -First 1
    if ($where) { $sevenZipExe = $where.Trim() }
}

if (-not $sevenZipExe -or -not (Test-Path $sevenZipExe)) {
    Write-Bad '7-Zip (7z.exe) not found.'
    Write-Host '  Install 7-Zip, or pass -SevenZip with the full path to 7z.exe.' -ForegroundColor DarkCyan
    exit 1
}
Write-Ok "7-Zip found: $sevenZipExe"

# ---------------------------------------------------------------------------
# Locate the game
# ---------------------------------------------------------------------------
Write-Step '2. Locate Skyrim'

if (-not $GamePath) {
    $candidates = @()
    try {
        $reg = Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition' -Name 'Installed Path' -ErrorAction Stop
        if ($reg.'Installed Path') { $candidates += $reg.'Installed Path' }
    } catch { }

    foreach ($drive in @('C', 'D', 'E', 'F', 'G', 'H')) {
        # Probe the drive letter first: Test-Path on a path under a nonexistent
        # drive throws a DriveNotFoundException rather than returning false.
        if (-not (Test-Path "$drive`:\")) { continue }
        $candidates += "$drive`:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"
        $candidates += "$drive`:\Steam\steamapps\common\Skyrim Special Edition"
        $candidates += "$drive`:\SteamLibrary\steamapps\common\Skyrim Special Edition"
    }

    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'SkyrimSE.exe'))) {
            $GamePath = $c
            break
        }
    }
}

if (-not $GamePath -or -not (Test-Path (Join-Path $GamePath 'SkyrimSE.exe'))) {
    Write-Bad 'Skyrim Special Edition not found.'
    Write-Host '  Pass -GamePath with your install directory.' -ForegroundColor DarkCyan
    exit 1
}
Write-Ok "Game: $GamePath"

# ---------------------------------------------------------------------------
# Define what we are looking for
# ---------------------------------------------------------------------------
Write-Step '3. Scan for downloads'

if (-not (Test-Path $DownloadDir)) {
    New-Item -ItemType Directory -Path $DownloadDir -Force | Out-Null
}

# A download matches by filename looks. The exact keywords are chosen to be
# broad enough that a user saving with either the Nexus-generated name or the
# official one is still recognised.
function Test-IsSkseArchive {
    param([string]$Name)
    # Match the SKSE64 filename shape, while rejecting archives for runtimes
    # OTHER than this AE install. The dangerous ones:
    #   "gog"      -> GOG build 2.2.6 for game 1.6.1179
    #   "1_5"      -> SE build for game 1.5.97
    #   "1_6"      -> older AE build for game 1.6.xxx
    #   "2_00_20"  -> Special Edition build 2.0.20 for game 1.5.97
    # The final verify step re-checks the installed DLL name against the game
    # exe version, so this filter is the first of two gates.
    return $Name -match 'skse64' -and $Name -notmatch 'gog|1_5_97|1_6_|2_00_20'
}

function Test-IsAddressLibraryArchive {
    param([string]$Name)
    return $Name -match 'address.?library|all.?in.?one|SKSE_All' 
}

$allFiles = @(Get-ChildItem $DownloadDir -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -match '\.(7z|zip|rar|exe)$' })

$skseArchive = $allFiles | Where-Object { Test-IsSkseArchive $_.Name } | Select-Object -First 1
$alibArchive = $allFiles | Where-Object { Test-IsAddressLibraryArchive $_.Name } | Select-Object -First 1

Write-Info "Scanning: $DownloadDir ($($allFiles.Count) archive(s) present)"
if ($skseArchive)   { Write-Ok "SKSE archive: $($skseArchive.Name)" }
else                { Write-Warn "SKSE archive: not present" }
if ($alibArchive)   { Write-Ok "Address Library archive: $($alibArchive.Name)" }
else                { Write-Warn "Address Library archive: not present" }

# ---------------------------------------------------------------------------
# Extract and install SKSE
# ---------------------------------------------------------------------------
function Install-Skse {
    param([string]$Archive)

    Write-Step '4. Install SKSE64'

    $staging = Join-Path $env:TEMP ("tg_skse_" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging -Force | Out-Null

    try {
        & $sevenZipExe x "-o$staging" -y $Archive *> $null
        if ($LASTEXITCODE -ne 0) { throw "7z extraction failed for $Archive" }

        # SKSE archives place files at the root or under one folder. Find the
        # loader and the runtime DLLs anywhere in the staging tree.
        $loader = Get-ChildItem $staging -Recurse -Filter 'skse64_loader.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
        $dlls   = @(Get-ChildItem $staging -Recurse -Filter 'skse64_*.dll' -ErrorAction SilentlyContinue)
        $scriptsDir = Get-ChildItem $staging -Recurse -Directory -Filter 'Scripts' -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'Data\\Scripts$' } | Select-Object -First 1

        if (-not $loader) { throw 'skse64_loader.exe not found in the archive. Wrong file?' }

        Copy-Item $loader.FullName (Join-Path $GamePath 'skse64_loader.exe') -Force
        Write-Ok 'skse64_loader.exe -> game root'

        foreach ($d in $dlls) {
            Copy-Item $d.FullName (Join-Path $GamePath $d.Name) -Force
            Write-Ok "$($d.Name) -> game root"
        }

        if ($scriptsDir) {
            $destScripts = Join-Path $GamePath 'Data\Scripts'
            if (-not (Test-Path $destScripts)) { New-Item -ItemType Directory -Path $destScripts -Force | Out-Null }
            Copy-Item (Join-Path $scriptsDir.FullName '*') $destScripts -Recurse -Force
            Write-Ok "Data\Scripts -> $destScripts"
        }

        # Steam loader dll sits at the game root too.
        $steamDll = Get-ChildItem $staging -Recurse -Filter 'skse64_steam_loader.dll' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($steamDll) {
            Copy-Item $steamDll.FullName (Join-Path $GamePath $steamDll.Name) -Force
            Write-Ok "$($steamDll.Name) -> game root"
        }
    }
    finally {
        Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------
# Extract and install the Address Library
# ---------------------------------------------------------------------------
function Install-AddressLibrary {
    param([string]$Archive)

    Write-Step '5. Install Address Library'

    $staging = Join-Path $env:TEMP ("tg_alib_" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging -Force | Out-Null

    try {
        & $sevenZipExe x "-o$staging" -y $Archive *> $null
        if ($LASTEXITCODE -ne 0) { throw "7z extraction failed for $Archive" }

        # The "All in one" package contains versionlib-*.bin (AE) and version-*.bin
        # (SE) files. Install any that were found, so the correct one for this game
        # is present regardless of package layout.
        $bins = @(Get-ChildItem $staging -Recurse -Include 'versionlib-*.bin', 'version-*.bin' -ErrorAction SilentlyContinue)
        if ($bins.Count -eq 0) { throw 'No versionlib-*.bin or version-*.bin found. Wrong file?' }

        $pluginsDir = Join-Path $GamePath 'Data\SKSE\Plugins'
        if (-not (Test-Path $pluginsDir)) { New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null }

        foreach ($b in $bins) {
            Copy-Item $b.FullName (Join-Path $pluginsDir $b.Name) -Force
        }
        Write-Ok "$($bins.Count) address database file(s) -> Data\SKSE\Plugins"
    }
    finally {
        Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($skseArchive)   { Install-Skse $skseArchive.FullName }
if ($alibArchive)   { Install-AddressLibrary $alibArchive.FullName }

# ---------------------------------------------------------------------------
# Verify the result
# ---------------------------------------------------------------------------
Write-Step '6. Verify'

$errors = 0
$loader = Join-Path $GamePath 'skse64_loader.exe'
if (Test-Path $loader) { Write-Ok 'skse64_loader.exe present' }
else { Write-Bad 'skse64_loader.exe missing'; $errors++ }

# The runtime DLL name encodes the game version SKSE was built for. It must
# match, or the game refuses to start with a version-mismatch error.
$exeVersion = (Get-Item (Join-Path $GamePath 'SkyrimSE.exe')).VersionInfo.FileVersion
$skseVer = (($exeVersion -split '\.')[0..2]) -join '_'   # 1.7.104.0 -> 1_7_104
$expectedDll = "skse64_${skseVer}.dll"

if (Test-Path (Join-Path $GamePath $expectedDll)) {
    Write-Ok "$expectedDll present"
}
else {
    Write-Bad "$expectedDll missing (SKSE build does not match game $exeVersion)"
    $errors++
}

$alib = Join-Path $GamePath "Data\SKSE\Plugins\versionlib-$(($exeVersion -split '\.')[0..3] -join '-').bin"
if (Test-Path $alib) {
    Write-Ok (Split-Path $alib -Leaf) + " present"
}
else {
    Write-Bad "versionlib-$(($exeVersion -split '\.')[0..3] -join '-').bin missing"
    $errors++
}

# ---------------------------------------------------------------------------
# Summary, or the exact instructions if something is still missing
# ---------------------------------------------------------------------------
Write-Step 'Summary'

if ($errors -eq 0) {
    Write-Host ''
    Write-Host '  All prerequisites are installed and version-matched.' -ForegroundColor Green
    Write-Host '  Run  TrueGaze.cmd verify   to confirm, then test.' -ForegroundColor White
    Write-Host ''
    exit 0
}

Write-Host ''
Write-Host '  Still missing (one manual step remains - Nexus requires a login):' -ForegroundColor Yellow
Write-Host ''

if (-not $skseArchive -or -not (Test-Path $loader)) {
    Write-Host ("  SKSE64 (AE build, for game version {0})" -f $exeVersion)
    Write-Host '    Download: https://www.nexusmods.com/skyrimspecialedition/mods/30379' -ForegroundColor White
    Write-Host ("    Save to : {0}\skse64_<version>.7z" -f $DownloadDir) -ForegroundColor White
    Write-Host ''
}

if (-not $alibArchive -or -not (Test-Path $alib)) {
    Write-Host '  Address Library for SKSE Plugins (All in one, AE)'
    Write-Host '    Download: https://www.nexusmods.com/skyrimspecialedition/mods/32444' -ForegroundColor White
    Write-Host ("    Save to : {0}\Address_Library_All_In_One-<version>.7z" -f $DownloadDir) -ForegroundColor White
    Write-Host ''
}

Write-Host '  Then run this script again. It will extract, install and verify automatically.' -ForegroundColor Cyan
Write-Host ''
exit 1