# ============================================================================
#  TrueGazeBridgeServer.ps1
#  Native PowerShell Automation & Bridge Server for TrueGaze Configurator
#  Provides REST APIs for:
#    - Automated Windows registry and drive scanning (Skyrim install & saves)
#    - Direct INI load & save with automatic backup (.bak)
#    - One-click DLL & INI deployment to Data\SKSE\Plugins\
#    - One-click SKSE / Skyrim launch directly from the browser
#    - Live SKSE\TrueGaze.log inspection
# ============================================================================

[CmdletBinding()]
param(
    [int]$Port = 48152,
    [switch]$Foreground
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$htmlPath = Join-Path $projectRoot 'TrueGazeConfig.html'

function Get-SkyrimInstallation {
    $candidates = [System.Collections.Generic.List[string]]::new()

    # 1. Bethesda Registry (Steam, GOG, Retail)
    foreach ($hive in @(
            'HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition',
            'HKLM:\SOFTWARE\Bethesda Softworks\Skyrim Special Edition'
        )) {
        try {
            if (Test-Path $hive) {
                $p = (Get-ItemProperty -Path $hive -Name 'Installed Path' -ErrorAction SilentlyContinue).'Installed Path'
                if ($p -and (Test-Path $p)) { $candidates.Add($p.TrimEnd('\')) }
            }
        }
        catch {}
    }

    # 2. GOG Registry
    try {
        $gog = 'HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games\1453375253'
        if (Test-Path $gog) {
            $p = (Get-ItemProperty -Path $gog -Name 'path' -ErrorAction SilentlyContinue).'path'
            if ($p -and (Test-Path $p)) { $candidates.Add($p.TrimEnd('\')) }
        }
    }
    catch {}

    # 3. Steam libraryfolders.vdf probing across all system drives
    $steamRoots = @(
        'C:\Program Files (x86)\Steam',
        'C:\Program Files\Steam',
        'G:\Program Files (x86)\Steam',
        'D:\Steam',
        'E:\Steam',
        'F:\Steam'
    )
    # Also check registry for SteamPath
    try {
        $sp = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -Name 'SteamPath' -ErrorAction SilentlyContinue).SteamPath
        if ($sp) { $steamRoots += ($sp -replace '/', '\') }
    }
    catch {}

    foreach ($sr in $steamRoots) {
        $drive = Split-Path $sr -Qualifier
        if ($drive -and -not (Test-Path ($drive + '\'))) { continue }
        $vdf = Join-Path $sr 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            try {
                $content = Get-Content $vdf -Raw
                foreach ($m in [regex]::Matches($content, '"path"\s+"([^"]+)"')) {
                    $lib = $m.Groups[1].Value -replace '\\\\', '\'
                    if (Test-Path ($lib + '\')) {
                        $candidates.Add((Join-Path $lib 'steamapps\common\Skyrim Special Edition'))
                    }
                }
            }
            catch {}
        }
        $candidates.Add((Join-Path $sr 'steamapps\common\Skyrim Special Edition'))
    }

    # 4. Standard common drive paths
    foreach ($letter in @('C', 'D', 'E', 'F', 'G', 'H', 'I', 'J')) {
        if (Test-Path "$letter`:\") {
            $candidates.Add("$letter`:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition")
            $candidates.Add("$letter`:\SteamLibrary\steamapps\common\Skyrim Special Edition")
            $candidates.Add("$letter`:\Steam\steamapps\common\Skyrim Special Edition")
            $candidates.Add("$letter`:\Games\Skyrim Special Edition")
        }
    }

    foreach ($c in $candidates) {
        if (-not $c) { continue }
        $drive = Split-Path $c -Qualifier
        if ($drive -and -not (Test-Path ($drive + '\'))) { continue }
        $exe = Join-Path $c 'SkyrimSE.exe'
        if (Test-Path $exe) {
            $fv = (Get-Item $exe).VersionInfo.FileVersion
            $loader = Join-Path $c 'skse64_loader.exe'
            $pluginsDir = Join-Path $c 'Data\SKSE\Plugins'
            $iniPath = Join-Path $pluginsDir 'TrueGaze.ini'
            $dllPath = Join-Path $pluginsDir 'TrueGaze.dll'

            # Find matching address library
            $addrLib = $null
            if (Test-Path $pluginsDir) {
                $addrMatches = Get-ChildItem $pluginsDir -Filter 'versionlib-*.bin' -ErrorAction SilentlyContinue
                if ($addrMatches) { $addrLib = $addrMatches[0].Name }
            }

            return @{
                found            = $true
                path             = (Resolve-Path $c).Path
                exePath          = $exe
                fileVersion      = $fv
                skseLoaderExists = (Test-Path $loader)
                skseLoaderPath   = $loader
                pluginsDir       = $pluginsDir
                pluginsDirExists = (Test-Path $pluginsDir)
                iniPath          = $iniPath
                iniExists        = (Test-Path $iniPath)
                dllPath          = $dllPath
                dllExists        = (Test-Path $dllPath)
                dllSize          = if (Test-Path $dllPath) { (Get-Item $dllPath).Length } else { 0 }
                addressLib       = $addrLib
            }
        }
    }

    return @{ found = $false }
}

function Get-SkyrimDocumentsInfo {
    $docs = [Environment]::GetFolderPath('MyDocuments')
    $skyrimDocs = Join-Path $docs 'My Games\Skyrim Special Edition'
    if (-not (Test-Path $skyrimDocs)) {
        $gogDocs = Join-Path $docs 'My Games\Skyrim Special Edition GOG'
        if (Test-Path $gogDocs) { $skyrimDocs = $gogDocs }
    }

    if (-not (Test-Path $skyrimDocs)) {
        return @{ found = $false; docsPath = $docs }
    }

    $savesDir = Join-Path $skyrimDocs 'Saves'
    $saveCount = 0
    $latestSave = $null
    if (Test-Path $savesDir) {
        $saves = Get-ChildItem $savesDir -Filter '*.ess' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
        $saveCount = $saves.Count
        if ($saves.Count -gt 0) {
            $top = $saves[0]
            # Parse human-readable parts from save name if standard pattern (e.g. Save1_Kirk_Riverwood...)
            $charName = "Unknown"
            $location = "Skyrim"
            $parts = $top.BaseName -split '_'
            if ($parts.Count -ge 4) {
                $charName = $parts[3]
                if ($parts.Count -ge 5) { $location = $parts[4] }
            }

            $latestSave = @{
                filename  = $top.Name
                lastWrite = $top.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')
                sizeBytes = $top.Length
                character = $charName
                location  = $location
            }
        }
    }

    $logPath = Join-Path $skyrimDocs 'SKSE\TrueGaze.log'

    return @{
        found          = $true
        docsPath       = $docs
        skyrimDocsPath = $skyrimDocs
        savesDir       = $savesDir
        saveCount      = $saveCount
        latestSave     = $latestSave
        logPath        = $logPath
        logExists      = (Test-Path $logPath)
        logSize        = if (Test-Path $logPath) { (Get-Item $logPath).Length } else { 0 }
    }
}

function Get-FullSystemScan {
    $game = Get-SkyrimInstallation
    $docs = Get-SkyrimDocumentsInfo

    $proc = Get-Process -Name 'SkyrimSE' -ErrorAction SilentlyContinue
    $isGameRunning = ($null -ne $proc)

    $repoIni = Join-Path $projectRoot 'skyrim\SKSE\Plugins\TrueGaze.ini'
    $repoDll = Join-Path $projectRoot 'skyrim\SKSE\Plugins\TrueGaze.dll'
    $buildDll = Join-Path $projectRoot 'build\windows-release\Release\TrueGaze.dll'

    return @{
        timestamp     = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
        game          = $game
        documents     = $docs
        gameRunning   = $isGameRunning
        gameProcessId = if ($isGameRunning) { $proc.Id } else { $null }
        repo          = @{
            root           = $projectRoot
            iniPath        = $repoIni
            iniExists      = (Test-Path $repoIni)
            builtDllPath   = $buildDll
            builtDllExists = (Test-Path $buildDll)
            repoDllPath    = $repoDll
            repoDllExists  = (Test-Path $repoDll)
        }
    }
}

function Send-JsonResponse($response, $data, [int]$statusCode = 200) {
    $response.StatusCode = $statusCode
    $response.ContentType = 'application/json; charset=utf-8'
    $response.Headers.Add('Access-Control-Allow-Origin', '*')
    $response.Headers.Add('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
    $response.Headers.Add('Access-Control-Allow-Headers', 'Content-Type')
    
    $json = $data | ConvertTo-Json -Depth 6
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $response.ContentLength64 = $bytes.Length
    $response.OutputStream.Write($bytes, 0, $bytes.Length)
    $response.OutputStream.Close()
}

function Send-TextResponse($response, [string]$text, [string]$contentType = 'text/plain; charset=iso-8859-1', [int]$statusCode = 200) {
    $response.StatusCode = $statusCode
    $response.ContentType = $contentType
    $response.Headers.Add('Access-Control-Allow-Origin', '*')
    $response.Headers.Add('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
    $response.Headers.Add('Access-Control-Allow-Headers', 'Content-Type')

    # Use Latin-1 (ISO-8859-1) encoding to preserve Skyrim INI byte fidelity (e.g. ™ trademark on line 1)
    $encoding = [System.Text.Encoding]::GetEncoding('iso-8859-1')
    $bytes = $encoding.GetBytes($text)
    $response.ContentLength64 = $bytes.Length
    $response.OutputStream.Write($bytes, 0, $bytes.Length)
    $response.OutputStream.Close()
}

function Send-FileResponse($response, [string]$filePath, [string]$contentType = 'text/html; charset=utf-8') {
    if (-not (Test-Path $filePath)) {
        Send-JsonResponse $response @{ error = 'File not found' } 404
        return
    }
    $response.StatusCode = 200
    $response.ContentType = $contentType
    $response.Headers.Add('Access-Control-Allow-Origin', '*')
    $bytes = [System.IO.File]::ReadAllBytes($filePath)
    $response.ContentLength64 = $bytes.Length
    $response.OutputStream.Write($bytes, 0, $bytes.Length)
    $response.OutputStream.Close()
}

# Start HTTP Listener
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://127.0.0.1:$Port/")

try {
    $listener.Start()
    Write-Host "TrueGaze Bridge Server running on http://127.0.0.1:$Port/" -ForegroundColor DarkCyan
    Write-Host "Press Ctrl+C to terminate or send POST /api/shutdown" -ForegroundColor DarkGray
}
catch {
    Write-Error "Failed to start HttpListener on port $Port : $($_.Exception.Message)"
    exit 1
}

try {
    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response
        $urlPath = $request.Url.AbsolutePath
        $method = $request.HttpMethod

        # Handle CORS preflight
        if ($method -eq 'OPTIONS') {
            $response.StatusCode = 204
            $response.Headers.Add('Access-Control-Allow-Origin', '*')
            $response.Headers.Add('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
            $response.Headers.Add('Access-Control-Allow-Headers', 'Content-Type')
            $response.OutputStream.Close()
            continue
        }

        try {
            if ($urlPath -eq '/' -or $urlPath -eq '/index.html' -or $urlPath -eq '/TrueGazeConfig.html') {
                Send-FileResponse $response $htmlPath 'text/html; charset=utf-8'
            }
            elseif ($urlPath -eq '/truegaze.jpe') {
                $img = Join-Path $projectRoot 'truegaze.jpe'
                Send-FileResponse $response $img 'image/jpeg'
            }
            elseif ($urlPath -eq '/api/scan') {
                $scan = Get-FullSystemScan
                Send-JsonResponse $response $scan
            }
            elseif ($urlPath -eq '/api/ini' -and $method -eq 'GET') {
                $scan = Get-FullSystemScan
                $target = $request.QueryString['path']
                if (-not $target) {
                    if ($scan.game.iniExists) {
                        $target = $scan.game.iniPath
                    }
                    elseif ($scan.repo.iniExists) {
                        $target = $scan.repo.iniPath
                    }
                }

                if ($target -and (Test-Path $target)) {
                    $raw = [System.IO.File]::ReadAllText($target, [System.Text.Encoding]::GetEncoding('iso-8859-1'))
                    Send-TextResponse $response $raw 'text/plain; charset=iso-8859-1'
                }
                else {
                    Send-JsonResponse $response @{ error = "INI not found at $target" } 404
                }
            }
            elseif ($urlPath -eq '/api/ini' -and $method -eq 'POST') {
                $reader = New-Object System.IO.StreamReader($request.InputStream, [System.Text.Encoding]::GetEncoding('iso-8859-1'))
                $content = $reader.ReadToEnd()
                $reader.Close()

                $scan = Get-FullSystemScan
                $target = $request.QueryString['path']
                if (-not $target) {
                    if ($scan.game.found) {
                        $target = $scan.game.iniPath
                    }
                    else {
                        $target = $scan.repo.iniPath
                    }
                }

                $dir = Split-Path -Parent $target
                if (-not (Test-Path $dir)) {
                    New-Item -ItemType Directory -Path $dir -Force | Out-Null
                }

                if (Test-Path $target) {
                    $bak = "$target.bak"
                    Copy-Item -Path $target -Destination $bak -Force
                }

                [System.IO.File]::WriteAllText($target, $content, [System.Text.Encoding]::GetEncoding('iso-8859-1'))
                Send-JsonResponse $response @{
                    success   = $true
                    path      = $target
                    backedUp  = (Test-Path "$target.bak")
                    sizeBytes = (Get-Item $target).Length
                    message   = "Successfully saved to $target"
                }
            }
            elseif ($urlPath -eq '/api/deploy' -and $method -eq 'POST') {
                $scan = Get-FullSystemScan
                if (-not $scan.game.found) {
                    Send-JsonResponse $response @{ error = "Skyrim installation not detected" } 400
                    continue
                }

                $pluginsDir = $scan.game.pluginsDir
                if (-not (Test-Path $pluginsDir)) {
                    New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
                }

                # Copy DLL
                $sourceDll = if ($scan.repo.builtDllExists) { $scan.repo.builtDllPath } else { $scan.repo.repoDllPath }
                if (-not (Test-Path $sourceDll)) {
                    Send-JsonResponse $response @{ error = "No built or repository TrueGaze.dll found to deploy" } 404
                    continue
                }
                Copy-Item -Path $sourceDll -Destination (Join-Path $pluginsDir 'TrueGaze.dll') -Force

                # Copy INI if not present
                $destIni = Join-Path $pluginsDir 'TrueGaze.ini'
                if (-not (Test-Path $destIni) -and $scan.repo.iniExists) {
                    Copy-Item -Path $scan.repo.iniPath -Destination $destIni -Force
                }

                Send-JsonResponse $response @{
                    success     = $true
                    deployedDll = (Join-Path $pluginsDir 'TrueGaze.dll')
                    deployedIni = $destIni
                    dllSize     = (Get-Item (Join-Path $pluginsDir 'TrueGaze.dll')).Length
                }
            }
            elseif ($urlPath -eq '/api/launch' -and $method -eq 'POST') {
                $scan = Get-FullSystemScan
                if (-not $scan.game.found) {
                    Send-JsonResponse $response @{ error = "Skyrim installation not detected" } 400
                    continue
                }

                if ($scan.gameRunning) {
                    Send-JsonResponse $response @{
                        error          = "Skyrim is already running (PID: $($scan.gameProcessId)). Please close it before launching."
                        alreadyRunning = $true
                        pid            = $scan.gameProcessId
                    } 409
                    continue
                }

                $launchBat = Join-Path $projectRoot 'LaunchTrueGaze.bat'
                if (Test-Path $launchBat) {
                    # Invoke the exact artifact that works when double-clicked. Keep
                    # preflight, SKSE startup, and foreground activation in one path.
                    $psi = [System.Diagnostics.ProcessStartInfo]::new()
                    $psi.FileName = $launchBat
                    $psi.WorkingDirectory = $projectRoot
                    $psi.UseShellExecute = $true
                    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Normal
                    $p = [System.Diagnostics.Process]::Start($psi)

                    Send-JsonResponse $response @{
                        success    = $true
                        launched   = 'LaunchTrueGaze.bat'
                        fullPath   = $launchBat
                        pid        = if ($p) { $p.Id } else { $null }
                        workingDir = $projectRoot
                        isSkse     = $true
                    }
                    continue
                }

                $exeToLaunch = if ($scan.game.skseLoaderExists) { $scan.game.skseLoaderPath } else { $scan.game.exePath }
                $gameRoot = $scan.game.path

                # Fallback: launch executable directly
                $psi = New-Object System.Diagnostics.ProcessStartInfo
                $psi.FileName = $exeToLaunch
                $psi.WorkingDirectory = $gameRoot
                $psi.UseShellExecute = $true
                $p = [System.Diagnostics.Process]::Start($psi)

                Send-JsonResponse $response @{
                    success    = $true
                    launched   = (Split-Path -Leaf $exeToLaunch)
                    fullPath   = $exeToLaunch
                    pid        = if ($p) { $p.Id } else { $null }
                    workingDir = $gameRoot
                    isSkse     = ($exeToLaunch -eq $scan.game.skseLoaderPath)
                }
            }
            elseif ($urlPath -eq '/api/log') {
                $docs = Get-SkyrimDocumentsInfo
                $tail = 60
                if ($request.QueryString['lines']) {
                    [int]::TryParse($request.QueryString['lines'], [ref]$tail) | Out-Null
                }
                if ($docs.logExists) {
                    $lines = @()
                    try {
                        $fs = [System.IO.FileStream]::new($docs.logPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
                        $sr = [System.IO.StreamReader]::new($fs, [System.Text.Encoding]::UTF8)
                        $allLines = [System.Collections.Generic.List[string]]::new()
                        while (-not $sr.EndOfStream) {
                            $allLines.Add($sr.ReadLine())
                        }
                        $sr.Close()
                        $fs.Close()
                        $startIdx = [Math]::Max(0, $allLines.Count - $tail)
                        $lines = @($allLines.GetRange($startIdx, $allLines.Count - $startIdx))
                    }
                    catch {
                        $lines = @("Error reading log: $($_.Exception.Message)")
                    }
                    Send-JsonResponse $response @{
                        exists    = $true
                        path      = $docs.logPath
                        lastWrite = (Get-Item $docs.logPath).LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')
                        lines     = $lines
                    }
                }
                else {
                    Send-JsonResponse $response @{
                        exists = $false
                        path   = $docs.logPath
                        lines  = @("Log file not created yet. Run Skyrim with TrueGaze installed to generate SKSE logs.")
                    }
                }
            }
            elseif ($urlPath -eq '/api/shutdown' -and $method -eq 'POST') {
                Send-JsonResponse $response @{ success = $true; message = 'Server shutting down' }
                break
            }
            else {
                Send-JsonResponse $response @{ error = "Not Found: $urlPath" } 404
            }
        }
        catch {
            Send-JsonResponse $response @{ error = $_.Exception.Message } 500
        }
    }
}
finally {
    $listener.Stop()
    $listener.Close()
    Write-Host "TrueGaze Bridge Server stopped." -ForegroundColor DarkYellow
}
