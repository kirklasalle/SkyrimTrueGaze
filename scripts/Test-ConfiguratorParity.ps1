#Requires -Version 5.1
<#
.SYNOPSIS
    Configurator parity check (R15 C4.2) — standalone dry-run form.

.DESCRIPTION
    Parses TrueGazeConfig.html's DEFAULTS block and diffs the shipped INI
    values. Same logic as stage 6 of Invoke-CiGate.ps1, runnable alone.

.EXAMPLE
    pwsh ./scripts/Test-ConfiguratorParity.ps1
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    $htmlPath = Join-Path $repoRoot 'TrueGazeConfig.html'
    $iniPath = Join-Path $repoRoot 'skyrim/SKSE/Plugins/TrueGaze.ini'

    # Parse the INI into a section->key->value map.
    $ini = @{}
    $section = ''
    foreach ($line in (Get-Content $iniPath)) {
        $t = $line.Trim()
        if ($t -match '^\[(.+?)\]$') { $section = $Matches[1]; $ini[$section] = @{}; continue }
        if ($t -match '^([fbis]\w+)\s*=\s*(.+?)\s*$') { $ini[$section][$Matches[1]] = $Matches[2] }
    }

    # Parse the HTML DEFAULTS block: lines of the form  key: ['type', default, ...]
    $html = Get-Content $htmlPath -Raw
    $parityFailures = 0
    $sectionName = ''
    foreach ($line in ($html -split "`n")) {
        if ($line -match "^\s+'(\w+)':\s*\{") { $sectionName = $Matches[1]; continue }
        if ($line -match "^\s+(\w+):\s*\['(bool|float|int|choice|text)',\s*([^,]+),") {
            $key = $Matches[1]
            $htmlDefault = $Matches[3].Trim()
            $htmlDefault = $htmlDefault.Trim([char]39, [char]34)
            if ($ini.ContainsKey($sectionName) -and $ini[$sectionName].ContainsKey($key)) {
                $iniValue = "$($ini[$sectionName][$key])"
                # Normalise INI bools: the engine accepts 1/0 and true/false —
                # but ONLY for keys the HTML schema types as 'bool' (an int key
                # like iRayRenderMode legitimately holds a numeric 1).
                if ($Matches[2] -eq 'bool') {
                    if ($iniValue -eq '1') { $iniValue = 'true' }
                    if ($iniValue -eq '0') { $iniValue = 'false' }
                }
                # Normalise the pipe path: the INI holds '\\.\pipe\...' (doubled)
                # and the HTML JS literal holds '\\\\.\pipe\\...' (doubled twice).
                # Backslash count is an encoding artefact, not a value difference —
                # compare backslash-stripped forms for string keys.
                $iniNorm = $iniValue.Replace('\', '')
                $htmlNorm = $htmlDefault.Replace('\', '')
                if ($htmlNorm -notmatch '^(true|false)$') {
                    $iniF = 0.0; $htmlF = 0.0
                    if ([double]::TryParse($iniNorm, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$iniF) -and
                        [double]::TryParse($htmlNorm, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$htmlF)) {
                        if ([Math]::Abs($iniF - $htmlF) -gt 0.0005) {
                            Write-Warning "PARITY: [$sectionName] $key HTML=$htmlNorm INI=$iniNorm"
                            $parityFailures++
                        }
                        continue
                    }
                }
                if ($iniNorm -ne $htmlNorm) {
                    Write-Warning "PARITY: [$sectionName] $key HTML=$htmlNorm INI=$iniNorm"
                    $parityFailures++
                }
            }
        }
    }

    if ($parityFailures -gt 0) {
        Write-Host "Parity drifts: $parityFailures" -ForegroundColor Red
        exit 1
    }
    Write-Host 'HTML DEFAULTS match the shipped INI (0 drifts).' -ForegroundColor Green
    exit 0
}
finally { Pop-Location }
