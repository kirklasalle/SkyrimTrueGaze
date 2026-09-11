scriptName TrueGaze_MCM extends SKI_ConfigBase

; TrueGaze™ - Mod Configuration Menu Script
; A First-Party Product of Kirk LaSalle's Human Communication Eye Protocol (HCEP)
; Integrates with SkyUI SKI_ConfigBase for in-game user customization.

; --- Option IDs ---
int oid_enableMod
int oid_enableCreatures
int oid_saccadeSpeed
int oid_microJitter
int oid_gazeAversion
int oid_socialTriangle
int oid_connectHcep
int oid_mutualGazeThreshold
int oid_debugRays

; --- State Variables ---
bool property bEnableMod = true auto
bool property bEnableCreatures = true auto
float property fSaccadeSpeedMult = 1.0 auto
float property fMicroJitterAmp = 0.35 auto
bool property bGazeAversion = true auto
bool property bSocialTriangle = true auto
bool property bConnectHcep = true auto
float property fMutualGazeThreshold = 2.0 auto
bool property bDebugRays = false auto

event OnConfigInit()
    Pages = new string[2]
    Pages[0] = "$TRUEGAZE_PAGE_GENERAL"
    Pages[1] = "$TRUEGAZE_PAGE_BRIDGE"
endEvent

event OnPageReset(string page)
    if (page == "$TRUEGAZE_PAGE_GENERAL" || page == "")
        SetCursorFillMode(TOP_TO_BOTTOM)
        
        AddHeaderOption("$TRUEGAZE_HEADER_ENGINE")
        oid_enableMod = AddToggleOption("$TRUEGAZE_ENABLE_MOD", bEnableMod)
        oid_enableCreatures = AddToggleOption("$TRUEGAZE_ENABLE_CREATURES", bEnableCreatures)
        
        AddHeaderOption("$TRUEGAZE_HEADER_KINEMATICS")
        oid_saccadeSpeed = AddSliderOption("$TRUEGAZE_SACCADE_SPEED", fSaccadeSpeedMult, "{1}x")
        oid_microJitter = AddSliderOption("$TRUEGAZE_MICRO_JITTER", fMicroJitterAmp, "{2} deg")
        
        SetCursorPosition(1)
        AddHeaderOption("$TRUEGAZE_HEADER_SOCIAL")
        oid_gazeAversion = AddToggleOption("$TRUEGAZE_GAZE_AVERSION", bGazeAversion)
        oid_socialTriangle = AddToggleOption("$TRUEGAZE_SOCIAL_TRIANGLE", bSocialTriangle)
        
    elseIf (page == "$TRUEGAZE_PAGE_BRIDGE")
        SetCursorFillMode(TOP_TO_BOTTOM)
        
        AddHeaderOption("$TRUEGAZE_HEADER_HCEP")
        oid_connectHcep = AddToggleOption("$TRUEGAZE_CONNECT_HCEP", bConnectHcep)
        oid_mutualGazeThreshold = AddSliderOption("$TRUEGAZE_MUTUAL_GAZE", fMutualGazeThreshold, "{1} sec")
        
        SetCursorPosition(1)
        AddHeaderOption("$TRUEGAZE_HEADER_DEBUG")
        oid_debugRays = AddToggleOption("$TRUEGAZE_DEBUG_RAYS", bDebugRays)
    endIf
endEvent

event OnOptionSelect(int option)
    if (option == oid_enableMod)
        bEnableMod = !bEnableMod
        SetToggleOptionValue(oid_enableMod, bEnableMod)
    elseIf (option == oid_enableCreatures)
        bEnableCreatures = !bEnableCreatures
        SetToggleOptionValue(oid_enableCreatures, bEnableCreatures)
    elseIf (option == oid_gazeAversion)
        bGazeAversion = !bGazeAversion
        SetToggleOptionValue(oid_gazeAversion, bGazeAversion)
    elseIf (option == oid_socialTriangle)
        bSocialTriangle = !bSocialTriangle
        SetToggleOptionValue(oid_socialTriangle, bSocialTriangle)
    elseIf (option == oid_connectHcep)
        bConnectHcep = !bConnectHcep
        SetToggleOptionValue(oid_connectHcep, bConnectHcep)
    elseIf (option == oid_debugRays)
        bDebugRays = !bDebugRays
        SetToggleOptionValue(oid_debugRays, bDebugRays)
    endIf
endEvent

event OnOptionSliderOpen(int option)
    if (option == oid_saccadeSpeed)
        SetSliderDialogStartValue(fSaccadeSpeedMult)
        SetSliderDialogDefaultValue(1.0)
        SetSliderDialogRange(0.5, 2.0)
        SetSliderDialogInterval(0.1)
    elseIf (option == oid_microJitter)
        SetSliderDialogStartValue(fMicroJitterAmp)
        SetSliderDialogDefaultValue(0.35)
        SetSliderDialogRange(0.0, 1.0)
        SetSliderDialogInterval(0.05)
    elseIf (option == oid_mutualGazeThreshold)
        SetSliderDialogStartValue(fMutualGazeThreshold)
        SetSliderDialogDefaultValue(2.0)
        SetSliderDialogRange(0.5, 5.0)
        SetSliderDialogInterval(0.25)
    endIf
endEvent

event OnOptionSliderAccept(int option, float value)
    if (option == oid_saccadeSpeed)
        fSaccadeSpeedMult = value
        SetSliderOptionValue(oid_saccadeSpeed, fSaccadeSpeedMult, "{1}x")
    elseIf (option == oid_microJitter)
        fMicroJitterAmp = value
        SetSliderOptionValue(oid_microJitter, fMicroJitterAmp, "{2} deg")
    elseIf (option == oid_mutualGazeThreshold)
        fMutualGazeThreshold = value
        SetSliderOptionValue(oid_mutualGazeThreshold, fMutualGazeThreshold, "{1} sec")
    endIf
endEvent

event OnOptionHighlight(int option)
    if (option == oid_enableMod)
        SetInfoText("$TRUEGAZE_ENABLE_MOD_DESC")
    elseIf (option == oid_enableCreatures)
        SetInfoText("$TRUEGAZE_ENABLE_CREATURES_DESC")
    elseIf (option == oid_saccadeSpeed)
        SetInfoText("$TRUEGAZE_SACCADE_SPEED_DESC")
    elseIf (option == oid_microJitter)
        SetInfoText("$TRUEGAZE_MICRO_JITTER_DESC")
    elseIf (option == oid_gazeAversion)
        SetInfoText("$TRUEGAZE_GAZE_AVERSION_DESC")
    elseIf (option == oid_socialTriangle)
        SetInfoText("$TRUEGAZE_SOCIAL_TRIANGLE_DESC")
    elseIf (option == oid_connectHcep)
        SetInfoText("$TRUEGAZE_CONNECT_HCEP_DESC")
    elseIf (option == oid_mutualGazeThreshold)
        SetInfoText("$TRUEGAZE_MUTUAL_GAZE_DESC")
    elseIf (option == oid_debugRays)
        SetInfoText("$TRUEGAZE_DEBUG_RAYS_DESC")
    endIf
endEvent
