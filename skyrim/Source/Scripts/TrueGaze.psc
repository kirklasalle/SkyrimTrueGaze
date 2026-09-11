scriptName TrueGaze hidden

; TrueGaze™ - Papyrus Script API
; A First-Party Product of Kirk LaSalle's Human Communication Eye Protocol (HCEP)
; Exposes biological gaze queries and HCEP cognitive modes to Papyrus modders.

; --- Cognitive Mode Constants ---
int property MODE_LOGIC  = 0 autoreadonly
int property MODE_AFFECT = 1 autoreadonly
int property MODE_SPIRIT = 2 autoreadonly
int property MODE_HEART  = 3 autoreadonly
int property MODE_THINK  = 4 autoreadonly

; --- Gaze Region Constants ---
int property REGION_LEFT_EYE   = 0 autoreadonly
int property REGION_RIGHT_EYE  = 1 autoreadonly
int property REGION_MOUTH      = 2 autoreadonly
int property REGION_FOREHEAD   = 3 autoreadonly
int property REGION_CHIN       = 4 autoreadonly
int property REGION_TORSO      = 5 autoreadonly
int property REGION_WEAPON     = 6 autoreadonly
int property REGION_SHIELD     = 7 autoreadonly
int property REGION_GROUND     = 8 autoreadonly
int property REGION_DEFOCUSED  = 12 autoreadonly

; @brief Returns the installed TrueGaze plugin version.
int function GetVersion() global native

; @brief Checks whether the in-game plugin is actively connected to the HCEP Desktop Suite.
bool function IsHcepConnected() global native

; @brief Checks whether the specified actor is currently maintaining mutual eye contact with the player.
bool function IsMutualGaze(Actor akActor, float afDurationThreshold = 1.5) global native

; @brief Returns the current HCEP cognitive mode of the actor (0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART, 4=THINK).
int function GetActorMode(Actor akActor) global native

; @brief Returns the 3D gaze target object or actor that the observer is currently looking at.
ObjectReference function GetGazeTarget(Actor akActor) global native

; @brief Forces an actor into a specific HCEP cognitive mode for a given duration in seconds.
function OverrideActorMode(Actor akActor, int aiMode, float afDurationSec) global native
