$ErrorActionPreference = 'Stop'
$gameData = 'G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data'
$paths = @(
    'd:\Projects\SkyrimTrueGaze\build\windows-debug\Debug\TrueGaze.dll',
    'd:\Projects\SkyrimTrueGaze\skyrim\SKSE\Plugins\TrueGaze.dll',
    (Join-Path $gameData 'SKSE\Plugins\TrueGaze.dll')
)
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
