# ============================================================================
#  TrueGaze - Install One-Click Launchers
#
#  Creates Windows shortcuts so the whole build/deploy/verify/launch cycle is a
#  double-click. Also places a .cmd shim in the project root for use from any
#  shell that is not PowerShell.
#
#  Run once:
#      .\scripts\Install-OneClick.ps1
# ============================================================================

[CmdletBinding()]
param(
    [string]$GamePath,
    # Create the shortcut on the Desktop as well as in the project folder.
    [switch]$Desktop = $true
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
$deployScript = Join-Path $PSScriptRoot 'Deploy-TrueGaze.ps1'

if (-not (Test-Path $deployScript)) {
    throw "Deploy-TrueGaze.ps1 not found next to this script. Expected: $deployScript"
}

Write-Host ''
Write-Host 'TrueGaze - Installing one-click launchers' -ForegroundColor Cyan
Write-Host '===========================================' -ForegroundColor Cyan
Write-Host ''

# ---------------------------------------------------------------------------
# Shortcuts.
#
# TrueGaze.cmd is NOT generated here. It is a real, tracked, self-contained
# batch script that resolves its own location, so it works from any checkout
# without regeneration. An earlier version of this installer overwrote it with
# a thin wrapper; do not reintroduce that.
# ---------------------------------------------------------------------------
$cmdPath = Join-Path $projectRoot 'TrueGaze.cmd'
if (Test-Path $cmdPath) {
    Write-Host ("  OK   Found entry point: " + $cmdPath) -ForegroundColor Green
}
else {
    Write-Host ("  WARN TrueGaze.cmd is missing from " + $projectRoot) -ForegroundColor Yellow
    Write-Host "       The shortcuts below call the PowerShell scripts directly and" -ForegroundColor DarkGray
    Write-Host "       will still work, but the batch entry point is absent." -ForegroundColor DarkGray
}

# ---------------------------------------------------------------------------
# Shortcuts.
# ---------------------------------------------------------------------------
function New-TGShortcut {
    param(
        [string]$Path,
        [string]$Target,
        [string]$Arguments,
        [string]$Description,
        [string]$WorkingDir
    )

    $shell = New-Object -ComObject WScript.Shell
    $sc = $shell.CreateShortcut($Path)
    $sc.TargetPath = $Target
    $sc.Arguments = $Arguments
    $sc.Description = $Description
    $sc.WorkingDirectory = $WorkingDir
    $sc.WindowStyle = 1
    # A terminal icon rather than the PowerShell logo: this is a build tool.
    $sc.IconLocation = "$env:SystemRoot\System32\shell32.dll,21"
    $sc.Save()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($shell) | Out-Null
}

$psExe = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'

$shortcuts = @(
    @{
        Name   = 'TrueGaze - Build and Launch.lnk'
        Args   = "-NoProfile -ExecutionPolicy Bypass -File `"$deployScript`""
        Desc   = 'Build, deploy, verify and launch TrueGaze'
    },
    @{
        Name   = 'TrueGaze - Safe Load-Only Test.lnk'
        Args   = "-NoProfile -ExecutionPolicy Bypass -File `"$deployScript`" -LoadOnly"
        Desc   = 'Deploy with simulation disabled and launch, to prove the plugin loads safely'
    },
    @{
        Name   = 'TrueGaze - Verify Only.lnk'
        Args   = "-NoProfile -ExecutionPolicy Bypass -File `"$deployScript`" -NoLaunch"
        Desc   = 'Build, deploy and run the pre-flight health check without launching'
    },
    @{
        Name   = 'TrueGaze - Analyse Last Run.lnk'
        Args   = "-NoProfile -ExecutionPolicy Bypass -File `"$deployScript`" -PostRun"
        Desc   = 'Parse TrueGaze.log and report what happened on the last run'
    }
)

$made = @()
foreach ($s in $shortcuts) {
    $p = Join-Path $projectRoot $s.Name
    New-TGShortcut -Path $p -Target $psExe -Arguments $s.Args -Description $s.Desc -WorkingDir $projectRoot
    $made += $p
    Write-Host ("  OK   " + $s.Name) -ForegroundColor Green
}

if ($Desktop) {
    # Keep the Desktop uncluttered: one shortcut, not four.
    $desktopDir = [Environment]::GetFolderPath('Desktop')
    $dp = Join-Path $desktopDir 'TrueGaze Test.lnk'
    New-TGShortcut -Path $dp -Target $psExe `
        -Arguments "-NoProfile -ExecutionPolicy Bypass -File `"$deployScript`"" `
        -Description 'Build, deploy, verify and launch TrueGaze' `
        -WorkingDir $projectRoot
    Write-Host ("  OK   " + $dp) -ForegroundColor Green
}

Write-Host ''
Write-Host 'Done. Double-click "TrueGaze - Build and Launch" to run the full cycle.' -ForegroundColor Cyan
Write-Host ''
Write-Host 'Recommended order for a first test:' -ForegroundColor White
Write-Host '  1. TrueGaze - Verify Only          (fix anything it reports)' -ForegroundColor Gray
Write-Host '  2. TrueGaze - Safe Load-Only Test  (proves it loads without crashing)' -ForegroundColor Gray
Write-Host '  3. TrueGaze - Build and Launch     (the real test)' -ForegroundColor Gray
Write-Host '  4. TrueGaze - Analyse Last Run     (what actually happened)' -ForegroundColor Gray
Write-Host ''
