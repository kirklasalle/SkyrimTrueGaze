# Character Gaze Profile — Domain Knowledge (researched 2026-09-25)

## SACRED INVARIANT — Player control model (Kirk, 2026-09-25) — WITH RATIONALE

- **3rd person**: TrueGaze fully controls the player (head/neck/spine/eyes/morphs).
- **1st person**: EYES ONLY. Head/neck/spine never touched.
- **WHY (Kirk's rationale — remember this):**
  1. **CANON**: the first-person view IS the character's eyes. The camera is the
     character's gaze; moving the head would move the player's own view = wrong canon.
  2. **VR**: the HMD owns the player's physical head. Writing head bones would fight
     the real-world head orientation → nausea/immersion break. Eyes-only is the only
     correct model for VR ("Mad God VR" is a supported target).
- This rationale is engineering guidance: any future feature must preserve eyes-only
  in 1st person because of canon + VR, not merely preference.

## Skyrim AI data (verified via UESP/GECK/SDK)

- **Confidence AV** (0-3): 0=Cowardly, 1=Cautious, 2=Brave, 3=Foolhardy. Read via `ActorValueOwner::GetBaseActorValue(ActorValue::kConfidence)`. SDK verified: `RE/A/ActorValueOwner.h` vfuncs 01/03.
- **Aggression AV** (0-3): 0=Unaggressive, 1=Aggressive(Enemies), 2=Very Aggressive(Enemies+Neutrals), 3=Frenzied(anyone).
- **Assistance AV** (0-3): helps nobody/friends/allies/anyone.
- **Morality**: via crime faction (0=any crime, 3=no crime). `Actor::GetCrimeFaction()`.
- **Relationship rank** −4..+4: Archnemesis(−4), Enemy(−3), Foe(−2), Rival(−1), Acquaintance(0), Friend(1), Confidant(2), Ally(3), Lover(4). SDK verified: `RE/B/BGSRelationship.h` (kLover=0 in its enum is a different ordering — use rank ints −4..4), `BGSRelationship::GetRelationship(TESNPC*, TESNPC*)` static.
- **Mood AV** exists (0=Neutral..7=Angry?) — optional future input.
- `Actor::IsChild()`, `IsGuard()`, `IsHumanoid()`, `IsPlayerRef()`, `IsInCombat()` all exist in SDK.

## Psychology grounding (web research 2026-09-25)

- Gaze avoidance correlates with personality: shyness, low confidence, social anxiety (Shackelford et al., Personality & Individual Differences, S0191886996001481).
- Affective eye contact has robust attentional/emotional effects (Frontiers in Psychology 2018, "Affective Eye Contact: An Integrative Review").
- Personality shapes gaze patterns without compromising expression recognition (SAGE QJEP 2025, 116-participant eye-tracking study).
- Gaze aversion in conversation = cognitive load management, not just avoidance (PMC8188832, MDPI Entropy 14(1):1).
- Gaze semantics in person perception (Nature Sci Rep 2024, s41598-024-51331-0).

## Design (Kirk-approved thesis)

- **GazeProfile spectrum**: every entity = point in (Confidence × Aggression × Relationship × StoryState) space, projected onto the HCEP-02 Enhanced Diagram (13 regions, 0-12).
- 3 inputs: (1) Temperament from vanilla AVs/factions/relationship — no new assets, no ESP; (2) Story state (dialogue/combat/scene-defer already sensed); (3) HCEP mode mapping (LOGIC/AFFECT/SPIRIT/HEART/THINK already per-actor).
- Profile modulates: CGA aversion rate+dwell, mutual-gaze threshold, triangle vertex selection WEIGHTS (diagram region meanings become probabilities), fixation durations, HCEP mode bias.
- Examples: Cowardly merchant idle = THINK-heavy + Far-Lower bias; Foolhardy guard combat = LOGIC lock; Lover dialogue = HEART + long mutual gaze + Chest visits; Scholar = SPIRIT/Third-eye; wolf = no triangle, fixation+head-dominant.
- Player profile = player's own behavior (crosshair mutual-gaze hold already measures attention).
- Dev panel: light up active region/vertex on diagram (bShowHcepPanel, dev-only).

## Plan doc

- `docs/Implementation Plan — Character Gaze Profile (Temperament-Driven Gaze).md` — phases C1-C5.
