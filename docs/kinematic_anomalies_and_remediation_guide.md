# Helgen Cart Kinematics & Scripted Scene Anomaly Resolution Guide

## 1. Problem Overview
In Skyrim, scripted scenes, quest staging, vehicle rigs (`PrisonerCarriage01`), and seated furniture animations (`CartIdle`, chairs, thrones, benches) often orient an actor's upper body at an angle divergent from the physics capsule rotation (`actor->GetAngleZ()`).

In the Helgen cart ride:
- Carriage travels North (+Y), `actor->GetAngleZ() = 0°`.
- Prisoners sit sideways across the cart bed on benches, facing East (+X, +90°).
- When TrueGaze calculated `localYaw = WrapPi(bearing - actor->GetAngleZ())`, it calculated yaw relative to North.
- Applying this yaw delta to `NPC Head [Head]` atop an already +90°-rotated torso resulted in a systematic +90° yaw bias (Ralof looked sideways at Lokir when speaking to the Player, and rotated 180° backward when speaking to Lokir).
- Additionally, vanilla procedure headtrack slots (`high->headTrackTarget[kProcedure]`) pointed to fellow prisoners while speech was addressed to the Player, causing scene deferrals that bypassed TrueGaze.

## 2. Core Remediation Architecture

### Torso-Relative Reference Orientation
Do not calculate azimuth deltas from `actor->GetAngleZ()`. Instead, sample the upper torso bone (`NPC Spine2 [Spn2]`) world rotation basis matrix column Y (NetImmerse forward):
```cpp
ReferenceOrientation GetActorReferenceOrientation(RE::Actor* actor, const ActorGazeRuntime& state) noexcept {
    ReferenceOrientation ref{};
    ref.headingRad = actor ? actor->GetAngleZ() : 0.0f;
    ref.pitchRad = 0.0f;

    if (state.cachedSpine) {
        const auto& m = state.cachedSpine->world.rotate;
        const RE::NiPoint3 forward = m.GetVectorY();
        const float horizLenSq = forward.x * forward.x + forward.y * forward.y;
        if (horizLenSq >= 0.04f) {
            ref.headingRad = std::atan2(forward.x, forward.y);
            ref.pitchRad = std::atan2(forward.z, std::sqrt(horizLenSq));
            return ref;
        }
    }
    // Fallback to occupied furniture or vehicle parent
    if (auto* process = actor->GetActorRuntimeData().currentProcess) {
        if (auto furnHandle = process->GetOccupiedFurniture()) {
            if (auto furnPtr = furnHandle.get()) {
                ref.headingRad = furnPtr->GetAngleZ();
                return ref;
            }
        }
    }
    return ref;
}
```

### Visual Cone Alignment
Center observer visual cone checks and ambient forward gaze on the anatomical chest facing:
```cpp
float GetObserverHeadingRad(RE::Actor* observer, const ActorGazeRuntime* state) noexcept;
```

### Speech Priority Over Procedure Deferral
Voice is ground truth. In `TargetSelector.cpp` Section 1c, prioritize speech over procedure slots:
1. `if (playerDirected && playerDirectedActor)` ➔ Target Player.
2. `if (high && isSpeaking)` ➔ Target recipient in `lastSpokenToArray` (Player or NPC).
3. `if (state->dialoguePlayerHoldSec > 0.0f)` ➔ Hold eye contact on Player.
4. `if (anyDirected && directedActor)` ➔ Defer to vanilla scene direction.

### Biomechanical Clamping
Clamp cervical yaw in seated postures to biological limits:
```cpp
if (isSeated) {
    desiredYaw = std::clamp(desiredYaw, -BoneController::CHAIN_YAW_LIMIT, BoneController::CHAIN_YAW_LIMIT); // ±70°
}
```

## 3. Anomaly Resilience Rules for Future Scenes
1. **Always resolve skeleton bones on frame 1** (`EnsureSkeletonResolved(actor, state)`) before target evaluation and deflection calculation.
2. **Never assume capsule root angle equals chest facing.**
3. **Use three-tier fallbacks** (Spine2 ➔ Occupied Furniture ➔ Actor Root) to prevent null dereferences or singularities during asset streaming.
4. **Enforce latching timers** (Dialogue Player Hold = 3.0s, Target Fixation Stability = 1.5s) to eliminate edge-chatter and head micro-flapping.
5. **Isolate exceptions** via `__try / __except` around actor update loops so mesh or mod bugs never crash the game.
