# World-Class Automated TrueGaze Configurator & Launcher

Transform the TrueGaze HTML configurator into a zero-friction, fully automated control center for the player. The configurator will automatically detect Windows installation paths, game saves, and SKSE status without requiring manual folder browsing, allow direct one-click saving to the game directory, and launch the game directly from the browser.

## User Review Required

> [!IMPORTANT]
> **Native Companion Micro-Bridge**: Because web browsers (Chrome, Edge, Firefox) run in an isolated sandbox that prevents web pages from reading the Windows Registry, accessing arbitrary disk drives, or launching executables, we provide a native PowerShell micro-bridge (`scripts\TrueGazeBridgeServer.ps1`). 
> When launched via [`Launch-TrueGazeConfig.cmd`](file:///d:/Projects/SkyrimTrueGaze/Launch-TrueGazeConfig.cmd), this micro-bridge runs locally on `http://127.0.0.1:48152` using Windows built-in .NET `HttpListener` (zero extra dependencies). It powers all automated scanning, direct INI saving, and 1-click game launching directly from the HTML page.

> [!TIP]
> If the HTML page is opened directly as a `file://` link without running the launcher script, it gracefully detects the bridge status and displays an "Automation Bridge Offline" guide, while retaining manual fallback controls so it never breaks.

---

## Audit of Current Configurator & Findings

1. **Scan Button Opens Browse Dialog**:
   - `TrueGazeConfig.html` currently relies on `window.showDirectoryPicker()`. This prompts the user with a Windows folder browser where they must navigate their drives manually.
   - It cannot query the Windows Registry, cannot read user Documents/Saves, and cannot scan across drives programmatically.
2. **Manual File Handling**:
   - The user currently has to drag `TrueGaze.ini` or use file open dialogs.
   - Saving often triggers browser download prompts ("Export Copy") rather than directly updating `Data\SKSE\Plugins\TrueGaze.ini`.
3. **No Game Launch Capability**:
   - The player must switch out of the browser to run batch files or open Steam/Mod Organizer to start Skyrim.
4. **Lack of Environment & Save Awareness**:
   - The player cannot see if their game version, SKSE, Address Library, or deployed DLL is ready, nor can they see their current save file status or read `TrueGaze.log`.

---

## Proposed Changes

### Automation Subsystem & Companion Micro-Bridge

#### [NEW] [`TrueGazeBridgeServer.ps1`](file:///d:/Projects/SkyrimTrueGaze/scripts/TrueGazeBridgeServer.ps1)
A lightweight PowerShell HTTP server listening on `http://127.0.0.1:48152` using `System.Net.HttpListener`:
- **`GET /` & static files**: Serves `TrueGazeConfig.html` directly, eliminating browser CORS/sandbox limitations.
- **`GET /api/scan`**:
  - Queries `HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition` & `Bethesda Softworks` keys.
  - Inspects all Steam `libraryfolders.vdf` on all drives (C:, D:, E:, G:, etc.).
  - Probes GOG registry and standard installation locations.
  - Queries `[Environment]::GetFolderPath('MyDocuments')` -> `My Games\Skyrim Special Edition`.
  - Scans `Saves\`, counting `.ess` saves and reading the latest save filename and timestamp.
  - Inspects `SkyrimSE.exe` version, `skse64_loader.exe`, matching Address Library `.bin`, and `Data\SKSE\Plugins\TrueGaze.dll`.
  - Checks if `SkyrimSE.exe` is currently running.
- **`GET /api/ini`**: Reads and returns the active `TrueGaze.ini` from `Data\SKSE\Plugins\` or repo fallback.
- **`POST /api/ini`**: Atomically saves the updated INI directly to `Data\SKSE\Plugins\TrueGaze.ini`, creating a `.bak` backup first.
- **`POST /api/deploy`**: Auto-deploys `TrueGaze.dll` and `TrueGaze.ini` to `Data\SKSE\Plugins\` in one click if missing.
- **`POST /api/launch`**: Launches `skse64_loader.exe` (or `SkyrimSE.exe`) with the proper working directory to start Skyrim.
- **`GET /api/log`**: Streams the tail of `SKSE\TrueGaze.log` for live diagnostics.

#### [MODIFY] [`Launch-TrueGazeConfig.ps1`](file:///d:/Projects/SkyrimTrueGaze/scripts/Launch-TrueGazeConfig.ps1)
- Start the bridge server process in the background (or reuse an already running instance).
- Open the user's default browser directly to `http://127.0.0.1:48152/`.

#### [MODIFY] [`Launch-TrueGazeConfig.cmd`](file:///d:/Projects/SkyrimTrueGaze/Launch-TrueGazeConfig.cmd)
- Ensure clean one-click launching of the automation bridge and browser page.

---

### World-Class Web Configurator & UI

#### [MODIFY] [`TrueGazeConfig.html`](file:///d:/Projects/SkyrimTrueGaze/TrueGazeConfig.html)
1. **Automated Zero-Search Onboarding**:
   - On page load, immediately calls `/api/scan`.
   - If detected, auto-loads the active `Data\SKSE\Plugins\TrueGaze.ini` without any user interaction.
2. **System Status & Save HUD Banner**:
   - Displays a sleek Nordic HUD card at the top:
     - **Game Path & Version**: `Skyrim Special Edition v1.7.104.0` (at `G:\Program Files (x86)\...`)
     - **Current Save**: Latest save name, character, and timestamp (e.g. `Quicksave0 ... (11:38 AM)`)
     - **SKSE & DLL Status**: `SKSE64 Ready` · `TrueGaze.dll Deployed`
     - **Process Status**: `Idle` or `Skyrim Running`
3. **One-Click Actions Toolbar**:
   - **`[ Launch Skyrim ]`** (Primary golden glowing button): Launches Skyrim via SKSE directly from the page.
   - **`[ Scan System ]`**: Automatically refreshes Windows registry, drive scans, and save files (replacing the old folder browse dialog).
   - **`[ Save Changes ]`**: Writes directly to disk via `/api/ini` with zero download prompts.
   - **`[ Shipped Defaults ]`**: Resets sliders to default values.
4. **Quick Presets Bar**:
   - Fast tuning presets:
     - *Vanilla Balanced* (default)
     - *Subtle & Natural* (gentler saccades, reduced jitter)
     - *Intense & Expressive* (faster saccades, high responsiveness)
     - *Social & Dialog Focus* (enhanced mutual gaze and social triangle)
5. **Live Log & Diagnostic Console Drawer**:
   - An expandable "Engine Log" panel displaying the latest lines from `TrueGaze.log`.
6. **Graceful Fallback Mode**:
   - If running without the bridge (e.g. raw `file://`), shows an unobtrusive status banner explaining how to start the bridge, while keeping standard client-side file picker/dropzone options functional.

---

## Verification Plan

### Automated & Programmatic Verification
1. **Bridge Server Endpoint Tests**:
   - Start `scripts\TrueGazeBridgeServer.ps1` on port 48152 in a test process.
   - Execute `Invoke-RestMethod http://127.0.0.1:48152/api/scan` and verify:
     - Detects `G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition` from Windows Registry.
     - Detects `G:\Users\kirkl\Documents\My Games\Skyrim Special Edition` and lists latest saves.
     - Confirms `SkyrimSE.exe` version `1.7.104.0` and `skse64_loader.exe` existence.
   - Test `GET /api/ini` and verify valid INI content is returned.
   - Test `POST /api/ini` with sample configuration changes and verify disk write & backup creation.
   - Test `GET /api/log` and verify `TrueGaze.log` lines are returned.

2. **Browser Subagent / Automated Verification**:
   - Open `http://127.0.0.1:48152/` in the browser subagent.
   - Confirm the System Status HUD renders with detected Game path, Latest Save, and SKSE readiness.
   - Click "Scan System" and confirm zero browse/picker popups appear, and scan results update smoothly.
   - Verify slider adjustments and one-click "Save Changes".
   - Verify "Launch Skyrim" button reflects status correctly.
