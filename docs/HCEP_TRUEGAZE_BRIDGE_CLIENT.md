# HCEP TrueGaze Bridge Client

**Status:** Implemented and published September 19, 2026
**Project:** HCEP Desktop (`D:\Projects\HCEP`)
**Consumer:** TrueGaze Skyrim SKSE plugin

## Purpose

The HCEP Desktop application previously produced live Kinect/HCEP readings but had no client for the TrueGaze Skyrim named pipe. The plugin created `\\.\pipe\TrueGazeBridge` and waited indefinitely, while HCEP only connected to its internal `HCEP_KINECT_BRIDGE` raw-sensor pipe.

This client closes that gap.

## Data Flow

```text
Xbox 360 Kinect
    -> HCEP Kinect/Vision pipeline
    -> HCEPPipelineOrchestrator.SnapshotReady
    -> TrueGazeBridgeClient
    -> \\.\pipe\TrueGazeBridge
    -> TrueGaze.dll PlayerGazeResolver
    -> NPC target/attention behavior
```

The HCEP Desktop does not need an LLM for this path. The bridge uses the local sensor and vision pipeline. LLM configuration is separate from telemetry production.

## Wire Contract

### Outbound to Skyrim: 64 bytes

The client sends the existing TrueGaze telemetry contract:

- magic `0x48434550` (`HCEP`)
- protocol `0x0100`
- sequence ID
- monotonic timestamp
- gaze pitch/yaw
- convergence/distance
- confidence
- HCEP mode
- blink mask
- social-triangle vertex
- head pitch/yaw/roll
- tracked person ID
- CRC32

### Inbound from Skyrim: 32 bytes

The client receives:

- target FormID
- relationship rank
- combat state
- dialogue state
- target distance
- mutual gaze angle
- game frame number
- CRC32

## Implementation

Files:

- `src/HCEP.App/TrueGazeBridgeClient.cs`
- `src/HCEP.App/TrueGazeBridgeHostedService.cs`
- `src/HCEP.App/App.xaml.cs` DI registration

Behavior:

- Starts automatically with the HCEP app.
- Starts the HCEP pipeline if necessary.
- Retries the Skyrim pipe every three seconds.
- Connects whether HCEP starts before or after Skyrim.
- Streams the primary tracked person's latest HCEP reading.
- Uses calibrated gaze direction when available, otherwise raw HCEP direction.
- Converts camera-space direction into yaw/pitch.
- Maps HCEP mode and social-triangle enums to the TrueGaze wire values.
- Sends face-tracking head pose.
- Maps the blink action unit to the wire blink mask.
- Receives and logs Skyrim feedback.
- Does not require an LLM.

## Build and Publish

```powershell
Set-Location D:\Projects\HCEP
dotnet build src\HCEP.App\HCEP.App.csproj -c Release
dotnet publish src\HCEP.App\HCEP.App.csproj -c Release -r win-x64 --self-contained false -o D:\Projects\HCEP\publish\app
```

The published assembly was verified to contain `TrueGazeBridgeClient`.

## Live Acceptance Test

1. Start the HCEP Desktop from `D:\Projects\HCEP\publish\app`.
2. Confirm the Kinect is detected and the HCEP pipeline reports tracked face/skeleton data.
3. Start Skyrim through SKSE.
4. Load a save near an NPC and enter third person.
5. Wait for TrueGaze to create `\\.\pipe\TrueGazeBridge`.
6. HCEP should log a successful TrueGaze connection.
7. Run `tgstatus` in Skyrim.
8. Confirm the HCEP lines show a sequence, confidence, age, and head/convergence data.
9. Move the head/gaze and confirm the sequence/confidence updates.
10. Confirm stale, low-confidence, and blink states fall back safely.

## Troubleshooting

### HCEP reports Kinect data but Skyrim says `Bridge idle`

- Skyrim may not be running or TrueGaze may not have loaded.
- Confirm the live `TrueGaze.log` contains `Gaze engine ready`.
- Confirm the pipe exists after Skyrim reaches the main menu.
- Restart the published HCEP app after updating it.

### Skyrim says `hcep intent inactive`

- The old HCEP executable may still be running.
- Ensure the published `HCEP.App.dll` timestamp is current.
- Confirm HCEP has a tracked face/person and non-zero gaze confidence.
- Search HCEP logs for `TrueGaze connected`.

### HCEP confidence is zero

The Kinect/Vision pipeline may have a tracked face but no valid gaze estimate. This is an upstream sensor/calibration condition, not a pipe failure. TrueGaze correctly falls back rather than using an invalid vector.

### No LLM is connected

That is acceptable. The TrueGaze telemetry client consumes local Kinect/HCEP sensor output and does not depend on the LLM provider.

## Privacy

The bridge carries local biometric-derived values: gaze direction, head pose, blink state, confidence, cognitive mode, and tracked person ID. It is local machine IPC only. Use it only with informed consent from people in front of the Kinect, and keep the HCEP and TrueGaze privacy/governance notices aligned.
