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
$gameData = if ($GamePath) { Join-Path $GamePath 'Data' } else { '' }
$paths = @(
    (Join-Path $proj 'build\windows-release\Release\TrueGaze.dll'),
    (Join-Path $proj 'skyrim\SKSE\Plugins\TrueGaze.dll')
)
if ($gameData) { $paths += (Join-Path $gameData 'SKSE\Plugins\TrueGaze.dll') }
foreach ($path in $paths) {
    if (Test-Path $path) {
        $item = Get-Item $path
        $hash = (Get-FileHash $path -Algorithm SHA256).Hash
        Write-Output "$path|$($item.Length)|$($item.LastWriteTime.ToString('s'))|$hash"
    }
    else {
        Write-Output "MISSING|$path"
    }
}
$proc = Get-Process SkyrimSE -ErrorAction SilentlyContinue
if ($proc) {
    Write-Output "PROCESS|SkyrimSE|PID=$($proc.Id)|START=$($proc.StartTime.ToString('s'))"
}
else {
    Write-Output 'PROCESS|SkyrimSE|NOT RUNNING'
}
