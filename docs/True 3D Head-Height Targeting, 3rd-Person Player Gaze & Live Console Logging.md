# True 3D Head-Height Targeting, 3rd-Person Player Gaze & Live Console Logging

## Problem & Analysis from In-Game Telemetry & Screenshots

### 1. NPCs Aiming at Chest / Coincidental Crouch Alignment (`ScreenShot63`, `63a`, `64a`, `65`, `67`)
- **Root Cause**: `TargetSelector.cpp` and `GazeEngine.cpp` both assumed a rigid static eye height offset of `+160.0f` above root translation (`playerPos.z + 160.0f` and `actorPos.z + 160.0f`).
- When both actors are in an interior cell (e.g. Riverwood Trader with floor $z = 0$), $dz = 160 - 160 = 0.0$, forcing pitch deflection to $0.0^\circ$ (flat horizontal).
- **Why Lucan looked at the chest**: Lucan leans forward on the counter in his animation idle. His torso/neck are naturally tilted down toward the table. With TrueGaze calculating a $0.0^\circ$ pitch relative to his body, his gaze shot down at the player's chest/arms.
- **Why Camilla looked at the chest while seated and only engaged when crouched (`ScreenShot64a`)**: Seated Camilla's actual head is at $z \approx 95$. Standing player head is at $z \approx 128$. To make eye contact, Camilla must look UP ($dz = +33$, pitch $\approx +13^\circ$). Because the engine calculated $dz = 0$, Camilla stared horizontally straight ahead into the standing player's chest/stomach. When the player crouched, the player's head dropped to $z \approx 95$, which happened to match Camilla's seated horizontal plane, triggering eye contact!

### 2. Player 3rd-Person Gaze Freezing / Idle (`ScreenShot66`)
- **Root Cause A (Targeting)**: In `TargetSelector.cpp`, when `observer == player`, the engine only checked `DialogueMenu` (topic selection menu), `PlayerGazeResolver::Resolve()` (crosshair directly on collision), or `CombatTarget`. In 3rd person with a free camera, if the player was standing near Lucan or Camilla without hovering the crosshair directly over them, the player fell through to ambient forward idle with 0 deflection.
- **Root Cause B (Skeleton Application)**: In `GazeEngine::ApplyToSkeleton`, `if (!isPlayer && head)` explicitly prevented applying head and neck rotation to the player! Since vanilla humanoid skeletons have no eye bones (`eyeL` and `eyeR` are null), the player's head and neck never rotated toward any target.

### 3. Console `stgverbose` Logger Level
- `CmdVerbose` toggled `cfg.logLevel` in memory and saved to `TrueGaze.ini`, but never updated `spdlog::default_logger()->set_level(...)`. Therefore, typing `stgverbose` in the console did not take effect in the active running session without a restart.

---

## Proposed Changes

### [Engine] 3D Bone Head Position & Elevation Solving
#### [MODIFY] [TargetSelector.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp)
- Add `GetActorHeadPosition(RE::Actor* actor)` which queries the actual `NPC Head [Head]` bone world transform (`head->world.translate`).
- Update all target position assignments (player, dialogue speaker, scene partner, headtrack target, combat target, nearby actor) to use the target's true head position in world coordinates.
- For `observer == player`: when not in `DialogueMenu`, crosshair focus, or combat, add a forward candidate scan for nearby conversational NPCs (within 4.5m and inside forward visual cone) so the player character actively looks at NPCs when standing near them in 3rd person.

#### [MODIFY] [GazeEngine.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp)
- In `ComputeDeflection`: pass the observer's actual head position (`state.cachedHead->world.translate`) into `WorldTargetToLocalGaze`.
- In `WorldTargetToLocalGaze`: compute $dz = \text{targetHead.z} - \text{observerHead.z}$. Seated characters looking at standing characters will pitch UP; standing characters looking at seated/leaning characters will pitch DOWN.
- In `ApplyToSkeleton`: allow `neck` and `head` rotation on the player character when in 3rd person (`camera && camera->IsInThirdPerson()`). (Keep 1st person untouched to prevent camera disruption, and keep `spine` untouched on player to prevent torso twitching during movement).

---

### [Integrations] Live Console Logger Level
#### [MODIFY] [ConsoleCommands.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/ConsoleCommands.cpp)
- In `CmdVerbose`: invoke `spdlog::default_logger()->set_level(cfg.logLevel == 1 ? spdlog::level::debug : spdlog::level::info)` so console toggling is instant.

---

## Verification Plan
1. **Automated Unit Tests**:
   - Run `cmake --build --preset release` to compile `TrueGaze.dll` and test binaries.
   - Run `KinematicsTests.exe` to verify 100% pass rate on all biomechanical kinematics tests.
2. **Binary Deployment**:
   - Deploy newly compiled release `TrueGaze.dll` to `G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\TrueGaze.dll`.
   - Synchronize with workspace `skyrim/SKSE/Plugins/TrueGaze.dll`.
3. **Packaging**:
   - Run `scripts/PackageMod.ps1` to produce fresh `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`.
