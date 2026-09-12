scriptName TrueGaze hidden

; TrueGaze™ - Papyrus Script API
; A First-Party Product of Kirk LaSalle's Human Communication Eye Protocol (HCEP)
; Exposes biological gaze queries and HCEP cognitive modes to Papyrus modders.
;
; IMPORTANT: every function below is registered natively by
; src/Integrations/PapyrusInterface.cpp. Names, parameter lists, and return
; types must match that file exactly or the Papyrus VM will fail at the call
; site. See also GOVERNANCE.md.
;
; Call GetVersion() first if you need to detect an out-of-date plugin; it returns
; the script API version (currently 1), not the plugin version.

; --- Cognitive Mode Constants ---
int property MODE_LOGIC  = 0 autoreadonly
int property MODE_AFFECT = 1 autoreadonly
int property MODE_SPIRIT = 2 autoreadonly
int property MODE_HEART  = 3 autoreadonly
int property MODE_THINK  = 4 autoreadonly

; --- Gaze Region Constants (0-12) ---
int property REGION_LEFT_EYE     = 0  autoreadonly
int property REGION_RIGHT_EYE    = 1  autoreadonly
int property REGION_MOUTH        = 2  autoreadonly
int property REGION_FOREHEAD     = 3  autoreadonly
int property REGION_CHIN         = 4  autoreadonly
int property REGION_TORSO        = 5  autoreadonly
int property REGION_RIGHT_HAND   = 6  autoreadonly
int property REGION_LEFT_HAND    = 7  autoreadonly
int property REGION_GROUND       = 8  autoreadonly
int property REGION_UPPER_LEFT   = 9  autoreadonly
int property REGION_UPPER_RIGHT  = 10 autoreadonly
int property REGION_HORIZON      = 11 autoreadonly
int property REGION_DEFOCUSED    = 12 autoreadonly

; @brief Returns the script API version. Compare against a known value to detect
;        an out-of-date plugin rather than faulting at a call site.
int function GetVersion() global native

; @brief True when the plugin is connected to the HCEP Desktop perception suite.
bool function IsHcepConnected() global native

; @brief True when the actor has held mutual eye contact with the player for at
;        least afDurationThreshold seconds.
; @param afDurationThreshold Minimum sustained contact, in seconds (e.g. 1.5).
bool function IsMutualGaze(Actor akActor, float afDurationThreshold = 1.5) global native

; @brief The actor's current HCEP mode (0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART,
;        4=THINK). Returns -1 if the actor has no live simulation state.
int function GetActorMode(Actor akActor) global native

; @brief The classified gaze region the actor is looking at (0-12, see the
;        REGION_* constants). Returns -1 if unavailable.
int function GetGazeRegion(Actor akActor) global native

; @brief The reference the actor is currently looking at, or None.
;        Only meaningful when the actor's target is a reference (player or
;        another actor); static scenery targets return None.
ObjectReference function GetGazeTarget(Actor akActor) global native

; @brief Forces an actor into a specific HCEP mode.
; @param aiMode One of the MODE_* constants. Values outside 0-4 are ignored.
; @param afDurationSec Reserved; duration-limited overrides are not yet honoured.
function OverrideActorMode(Actor akActor, int aiMode, float afDurationSec = 0.0) global native

; @brief True while the actor's eyes are mid-ballistic-saccade (a 20-50 ms window).
;        Useful for synchronising micro-expressions.
bool function IsSaccadeActive(Actor akActor) global native

; @brief The actor's current gaze yaw deflection in degrees, relative to its
;        forward. Positive is to the actor's left.
float function GetGazeYaw(Actor akActor) global native

; @brief The actor's current gaze pitch deflection in degrees. Positive is up.
float function GetGazePitch(Actor akActor) global native
