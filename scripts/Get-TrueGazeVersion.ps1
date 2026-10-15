#Requires -Version 5.1
<#
.SYNOPSIS
    Prints the TrueGaze version from the single source of truth.

.DESCRIPTION
    Audit 2026-10-15 F-07: the version was hand-maintained in six places and
    drifted (packaging scripts said 1.0.5, CI fallback 1.0.7, plugin 1.0.8).
    `project(TrueGaze VERSION x.y.z)` in the root CMakeLists.txt is now the only
    place it is written; the plugin reads it via the generated
    TrueGazeVersion.h, and scripts read it through this helper.

.EXAMPLE
    $version = & (Join-Path $PSScriptRoot 'Get-TrueGazeVersion.ps1')
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$cmakeLists = Join-Path (Split-Path -Parent $PSScriptRoot) 'CMakeLists.txt'
if (-not (Test-Path $cmakeLists)) {
    throw "Get-TrueGazeVersion: CMakeLists.txt not found at $cmakeLists"
}

$content = Get-Content $cmakeLists -Raw
$match = [regex]::Match($content, 'project\(\s*TrueGaze\s+VERSION\s+(\d+\.\d+\.\d+)')
if (-not $match.Success) {
    throw "Get-TrueGazeVersion: could not find 'project(TrueGaze VERSION x.y.z)' in $cmakeLists"
}

$match.Groups[1].Value
