# ============================================================================
#  Launch-TrueGazeConfig.ps1
#  One-click launcher for the TrueGaze World-Class Web Configurator
#  Starts the local automation bridge server and opens the browser.
# ============================================================================

$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$serverScript = Join-Path $scriptDir 'TrueGazeBridgeServer.ps1'
$port = 48152
$baseUrl = "http://127.0.0.1:$port/"

# This is a development companion, not a persistent Windows service. Always
# replace an existing hidden bridge so edits to launch/debug behavior are loaded.
try {
    $existing = Invoke-RestMethod -Uri "$($baseUrl)api/scan" -Method GET -UseBasicParsing -TimeoutSec 1 -ErrorAction Stop
    if ($existing) {
        try {
            Invoke-RestMethod -Uri "$($baseUrl)api/shutdown" -Method POST -UseBasicParsing -TimeoutSec 2 -ErrorAction SilentlyContinue | Out-Null
        } catch {}
        for ($i = 0; $i -lt 10; $i++) {
            if (-not (Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue)) { break }
            Start-Sleep -Milliseconds 100
        }
    }
} catch {}

# 1. Verify if the bridge is already running
$isRunning = $false
try {
    $scanUrl = "$($baseUrl)api/scan"
    $res = Invoke-RestMethod -Uri $scanUrl -Method GET -UseBasicParsing -TimeoutSec 1 -ErrorAction SilentlyContinue
    if ($res -and $res.timestamp) {
        $isRunning = $true
    }
} catch {}

if (-not $isRunning) {
    Write-Host "Starting TrueGaze Automation Bridge Server on $baseUrl..." -ForegroundColor DarkCyan
    Start-Process powershell -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', "`"$serverScript`""
    
    # Wait up to 5 seconds for the server to spin up
    $started = $false
    for ($i = 0; $i -lt 15; $i++) {
        Start-Sleep -Milliseconds 300
        try {
            $probe = Invoke-RestMethod -Uri "$($baseUrl)api/scan" -Method GET -UseBasicParsing -TimeoutSec 1 -ErrorAction SilentlyContinue
            if ($probe -and $probe.timestamp) {
                $started = $true
                break
            }
        } catch {}
    }

    if (-not $started) {
        Write-Warning "Bridge server did not respond in time; proceeding to open UI directly."
    }
} else {
    Write-Host "TrueGaze Automation Bridge is already active on $baseUrl." -ForegroundColor DarkGreen
}

# 2. Open the configurator URL in the default browser
Write-Host "Opening TrueGaze Configurator ($baseUrl)..." -ForegroundColor DarkCyan
Start-Process $baseUrl

Write-Host "TrueGaze Configuration is ready. Enjoy your journey in Skyrim!" -ForegroundColor DarkGreen