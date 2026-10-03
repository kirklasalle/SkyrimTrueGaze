$ErrorActionPreference = 'Stop'
$gameData = 'G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data'
$project = 'd:\Projects\SkyrimTrueGaze'

if (Get-Process SkyrimSE -ErrorAction SilentlyContinue) {
    Write-Output 'GAME RUNNING - ABORT CLEAN INSTALL'
    exit 1
}

$installFiles = @(
    @{ Source = "$project\skyrim\SKSE\Plugins\TrueGaze.dll"; Destination = "$gameData\SKSE\Plugins\TrueGaze.dll" },
    @{ Source = "$project\scratch\arrow_extract\meshes\marker_arrow.nif"; Destination = "$gameData\meshes\marker_arrow.nif" },
    @{ Source = "$project\skyrim\meshes\TrueGaze\GazeRegionPanel.nif"; Destination = "$gameData\meshes\TrueGaze\GazeRegionPanel.nif" },
    @{ Source = "$project\skyrim\meshes\TrueGaze\GazeBeam.nif"; Destination = "$gameData\meshes\TrueGaze\GazeBeam.nif" },
    @{ Source = "$project\skyrim\textures\TrueGaze\GazeBeamGlow.dds"; Destination = "$gameData\textures\TrueGaze\GazeBeamGlow.dds" },
    @{ Source = "$project\skyrim\textures\TrueGaze\GazeRegionPanel.dds"; Destination = "$gameData\textures\TrueGaze\GazeRegionPanel.dds" }
)

# Preserve user configuration; only back it up before replacing TrueGaze-owned binaries/assets.
$ini = "$gameData\SKSE\Plugins\TrueGaze.ini"
if (Test-Path $ini) {
    $backup = "$ini.clean-install-backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    Copy-Item $ini $backup -Force
    Write-Output "CONFIG BACKUP  $backup"
}

# Remove only files owned by this TrueGaze installation. Do not touch Skyrim, SKSE,
# other mods, or the user's INI.
foreach ($file in $installFiles) {
    if (Test-Path $file.Destination) {
        Remove-Item $file.Destination -Force
        Write-Output "REMOVED        $($file.Destination)"
    }
}

foreach ($file in $installFiles) {
    New-Item -ItemType Directory -Force (Split-Path $file.Destination -Parent) | Out-Null
    Copy-Item $file.Source $file.Destination -Force
    $sourceHash = (Get-FileHash $file.Source -Algorithm SHA256).Hash
    $installedHash = (Get-FileHash $file.Destination -Algorithm SHA256).Hash
    if ($sourceHash -ne $installedHash) {
        throw "HASH MISMATCH: $($file.Destination)"
    }
    Write-Output ("INSTALLED      {0}  {1}" -f (Split-Path $file.Destination -Leaf), $installedHash.Substring(0, 16))
}

Write-Output 'CLEAN INSTALL VERIFIED'
