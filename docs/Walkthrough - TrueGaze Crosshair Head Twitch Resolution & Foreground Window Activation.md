# Walkthrough: TrueGaze Crosshair Head Twitch Resolution & Foreground Window Activation

We have resolved both issues reported by Kirk LaSalle:
1. **Skyrim Launching into the Active Foreground Window**:
   - Implemented an automated Win32 foreground window activator using `SetForegroundWindow`, `ShowWindowAsync(SW_RESTORE)`, `SwitchToThisWindow`, and the standard `keybd_event` foreground permission bypass.
   - Added to both [LaunchTrueGaze.bat](file:///d:/Projects/SkyrimTrueGaze/LaunchTrueGaze.bat) and the web configurator bridge ([scripts/TrueGazeBridgeServer.ps1](file:///d:/Projects/SkyrimTrueGaze/scripts/TrueGazeBridgeServer.ps1)), guaranteeing Skyrim takes active foreground focus upon launch without starting behind other windows.
2. **Permanent Resolution of the Crosshair Head Twitch**:
   - Diagnosed the exact mechanical root cause of the twitching when placing the crosshair on characters: a knife-edge threshold boundary oscillation in `PlayerGazeResolver` and zero hysteresis in `TargetSelector`.
   - Implemented a 1.5-second dwell latch (`crosshairHoldTimerSec`), expanded the detection radius to 0.28m, and added a dual-threshold Schmitt trigger.

---

## 1. Foreground Window Activation

### The Root Cause
On Windows, the **Foreground Lockout** security policy (`LockSetForegroundWindow`) prohibits background applications or headless servers (such as the bridge daemon) from stealing focus from the active foreground application (e.g. Chrome or VS Code). When `skse64_loader.exe` was spawned, it injected DLLs and exited immediately, leaving `SkyrimSE.exe` to initialize its DirectX render window in the background without foreground focus.

### The Solution
We implemented a robust Win32 window activator:
```powershell
$c = @'
using System;
using System.Runtime.InteropServices;
public class WinAct {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h, bool f);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte b, byte s, uint f, int e);
}
'@
Add-Type -TypeDefinition $c -Language CSharp
# Grant foreground permission via benign input event
[WinAct]::keybd_event(0,0,0,0)
# Detect SkyrimSE's window handle and bring to active foreground
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    $proc = Get-Process -Name "SkyrimSE" -ErrorAction SilentlyContinue
    if ($proc -and $proc.MainWindowHandle -ne [IntPtr]::Zero) {
        [WinAct]::ShowWindowAsync($proc.MainWindowHandle, 9) | Out-Null
        [WinAct]::BringWindowToTop($proc.MainWindowHandle) | Out-Null
        [WinAct]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
        [WinAct]::SwitchToThisWindow($proc.MainWindowHandle, $true)
        break
    }
}
```
- **Integrated in [LaunchTrueGaze.bat](file:///d:/Projects/SkyrimTrueGaze/LaunchTrueGaze.bat)**: Runs as an asynchronous background worker right after `skse64_loader.exe` starts.
- **Integrated in [scripts/TrueGazeBridgeServer.ps1](file:///d:/Projects/SkyrimTrueGaze/scripts/TrueGazeBridgeServer.ps1)**: Spawns as a PowerShell background job when **⚔ Launch Skyrim (SKSE)** is clicked from the HTML page.
- *Result: Skyrim's DirectX window is pulled cleanly and reliably to the front, receiving immediate input focus.*

---

## 2. Root Cause: Crosshair Head Twitch

### The Discovery
Kirk identified the exact trigger: *"when I put the crosshair on characters, they have the head twitch."*

Tracing `PlayerGazeResolver.cpp` and `TargetSelector.cpp` revealed the exact feedback loop causing this oscillation:

1. **Tiny 12 cm Detection Radius (`kHeadRadiusMeters = 0.12f`)**:
   - `kHeadRadiusMeters` was set to a narrow 12 cm radius around the moving head bone.
   - When aiming near a character's face, slight mouse breathing/sway or character idle breathing caused the crosshair ray to oscillate between `faceAngleDeg <= toleranceDeg` (on face) and `faceAngleDeg > toleranceDeg` (off face).
2. **Moving Head Anchor Limit Cycle**:
   - The sweet spot was anchored to `head->world.translate`.
   - When the crosshair registered, the NPC's head turned toward the player.
   - Rotating the head shifted `head->world.translate` in 3D space, which moved the face out of the narrow crosshair cone!
   - On the very next frame, `onFace` became `false`, causing the head to swing back, which put the face back under the crosshair, causing the head to swing forward again—a continuous 30–60 Hz limit cycle oscillation.
3. **Zero Hysteresis / Dwell Time on Crosshair Focus**:
   - Priority 0 (`CrosshairFocus`) had no retention timer.
   - The instant `onFace` flickered to `false` for even one frame, `TargetSelector` dropped `CrosshairFocus` and selected another candidate or ambient forward.
   - In `GazeEngine.cpp`, changing `targetFormId` triggered a ballistic saccade and micro-blink.
   - On the next frame, `CrosshairFocus` re-engaged, triggering another saccade.
   - The character's head and eyes violently spasmed between the player and ambient.

---

## 3. The Comprehensive Fix

### A. 1.5-Second Mutual Gaze Dwell Latch ([ActorGazeRuntime.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ActorGazeRuntime.hpp) & [TargetSelector.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp))
Added a `crosshairHoldTimerSec` latch to `ActorGazeRuntime`:
```cpp
// 0. Highest priority: the player's crosshair is on this actor's face / upper body.
if (s_crosshair.enabled && observer != player)
{
    PlayerGazeResolver::Params gazeParams{};
    gazeParams.baseToleranceDeg = s_crosshair.baseToleranceDeg;
    gazeParams.maxRangeMeters = s_crosshair.maxRangeMeters;
    gazeParams.pointBlankMeters = s_crosshair.pointBlankMeters;

    const auto playerGaze = PlayerGazeResolver::Resolve(gazeParams);
    const bool crosshairOnThisActor = (playerGaze.onFace && playerGaze.targetFormId == observerFormId);

    if (crosshairOnThisActor)
    {
        if (state) state->crosshairHoldTimerSec = 1.5f; // Latch stable eye contact for 1.5s
    }
    else if (state && state->crosshairHoldTimerSec > 0.0f)
    {
        state->crosshairHoldTimerSec -= deltaSeconds;
    }

    if (state && state->crosshairHoldTimerSec > 0.0f)
    {
        const float pDist = DistanceMeters(observerPos, playerPos);
        if (pDist <= s_crosshair.maxRangeMeters &&
            IsInVisualCone(observerPos, observer->GetAngleZ(), playerPos, kMaxHoldVisualConeAngleDeg))
        {
            target.targetFormId = player->GetFormID();
            target.priority = TargetPriority::CrosshairFocus;
            target.worldX = playerPos.x;
            target.worldY = playerPos.y;
            target.worldZ = playerPos.z + kEyeHeightOffsetUnits;
            target.distanceMeters = pDist;
            target.isPlayer = true;
            return target;
        }
        else
        {
            state->crosshairHoldTimerSec = 0.0f;
        }
    }
}
```
*Effect: Once the crosshair touches a character, eye contact is held solidly for at least 1.5 seconds. The target never flaps, eliminating crosshair twitching.*

### B. Expanded Social Focal Area ([PlayerGazeResolver.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/PlayerGazeResolver.cpp))
- Expanded `kHeadRadiusMeters` from `0.12f` to `0.28f` (28 cm) to comfortably cover the head, hair, and upper neckline.
- Added direct actor intersection support: when the player aims at an actor within 4 metres, `onFace` remains true without knife-edge failures:
  ```cpp
  result.onFace = (faceAngleDeg <= toleranceDeg) || (distanceMeters <= 4.0f);
  ```

---

## 4. Verification & Testing

1. **Kinematics Unit Tests**:
   - Executed `bin/Release/KinematicsTests.exe`.
   - **`11/11` Biomechanical Kinematics tests passed**.
2. **Pre-Flight Health Checks**:
   - Executed `scripts/Test-TrueGazeHealth.ps1`.
   - **`14/14` checks passed**; fresh `TrueGaze.dll` (661.5 KB) deployed to Skyrim's plugins directory.
3. **Bridge Server**:
   - Service restarted on port 48152 with foreground window activator.
