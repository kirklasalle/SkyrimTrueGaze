<#
.SYNOPSIS
    Installs the TrueGaze charter-integrity git hooks into this repository.

.DESCRIPTION
    Copies scripts/hooks/* into .git/hooks/ and marks them executable.

    The pre-commit hook enforces the LAW10-CHARTER control locally: the 10 Laws
    and 4 Core Tenets of the Permanent Active Directives are pinned by digest and
    cannot be altered without the commit being refused.

    This is a local control. It is not a substitute for server-side CI, and it
    does not protect against a commit made with --no-verify or by a contributor
    who never ran this installer. See GOVERNANCE.md for the honest status.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts/install-hooks.ps1
#>

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$hooksSource = Join-Path $PSScriptRoot "hooks"
$hooksTarget = Join-Path $repoRoot ".git\hooks"

Write-Host "TrueGaze - Charter Integrity Hook Installer" -ForegroundColor Cyan
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host ""

if (!(Test-Path $hooksTarget)) {
    Write-Error "Not a git repository: $hooksTarget does not exist."
}

if (!(Test-Path $hooksSource)) {
    Write-Error "Hook source directory not found: $hooksSource"
}

$installed = 0
foreach ($hook in Get-ChildItem -Path $hooksSource -File) {
    $destination = Join-Path $hooksTarget $hook.Name
    Copy-Item -Path $hook.FullName -Destination $destination -Force
    Write-Host "  installed: $($hook.Name) -> .git/hooks/$($hook.Name)" -ForegroundColor Green
    $installed++
}

Write-Host ""
Write-Host "Installed $installed hook(s)." -ForegroundColor Cyan
Write-Host ""

# Sanity check: the verifier must run clean before enforcing it.
Write-Host "Running a charter verification to confirm the hook will behave correctly..." -ForegroundColor Yellow
& python (Join-Path $PSScriptRoot "verify_charter.py")
$result = $LASTEXITCODE

Write-Host ""
if ($result -eq 0) {
    Write-Host "Charter verifier passes. The pre-commit gate is active." -ForegroundColor Green
}
else {
    Write-Host "Charter verifier returned exit code $result." -ForegroundColor Yellow
    Write-Host "The hook is installed but will refuse commits until this is resolved." -ForegroundColor Yellow
}
Write-Host ""
Write-Host "Bypass (deliberately): git commit --no-verify" -ForegroundColor DarkGray
Write-Host "Amendment procedure:   GOVERNANCE.md" -ForegroundColor DarkGray
