# ============================================================================
#  Activate-TrueGazeWindow.ps1
#  Brings the SkyrimSE render window to the foreground after SKSE launches it.
#
#  Why this is a separate file
#  ---------------------------------------------------------------------------
#  The Win32 foreground activation requires an Add-Type C# definition. An
#  earlier revision embedded that C# inline inside a batch `powershell -Command`
#  argument. cmd.exe cannot parse a multi-line quoted argument: every source
#  line leaked out and was executed as a batch command, and the embedded '@
#  here-string opened a string that never terminated. Keeping the definition
#  in a real .ps1 file and invoking it with `powershell -File` removes every
#  batch/PowerShell quoting boundary.
#
#  Windows applies Foreground Lockout (LockSetForegroundWindow): a background
#  process may not steal focus. skse64_loader.exe injects and exits, leaving
#  SkyrimSE.exe to create its DirectX window behind whatever had focus. The
#  keybd_event trick releases the lockout for this thread, then the window is
#  restored, raised and focused.
# ============================================================================

$ErrorActionPreference = 'SilentlyContinue'

$source = @'
using System;
using System.Runtime.InteropServices;

public class TrueGazeWinAct
{
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h, bool f);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte b, byte s, uint f, int e);
}
'@

try {
    Add-Type -TypeDefinition $source -Language CSharp
}
catch {
    # Type already loaded in this session; safe to continue.
}

# Release the foreground lockout for this process, then wait for the game
# window to exist. Polls for up to 20 seconds (40 x 500 ms).
try { [TrueGazeWinAct]::keybd_event(0, 0, 0, 0) } catch {}

for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500

    $proc = Get-Process -Name 'SkyrimSE' -ErrorAction SilentlyContinue
    if ($null -eq $proc) { continue }

    $proc.Refresh()
    $handle = $proc.MainWindowHandle
    if ($handle -eq [IntPtr]::Zero) { continue }

    [TrueGazeWinAct]::ShowWindowAsync($handle, 9)      | Out-Null   # SW_RESTORE
    [TrueGazeWinAct]::BringWindowToTop($handle)        | Out-Null
    [TrueGazeWinAct]::SetForegroundWindow($handle)     | Out-Null
    [TrueGazeWinAct]::SwitchToThisWindow($handle, $true)
    break
}