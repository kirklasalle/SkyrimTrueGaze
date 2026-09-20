TrueGaze - Custom Diagnostic Gaze Beam Mesh Directory
======================================================
Modders may place an optional standalone custom mesh named 'GazeBeam.nif' in this folder:
  Data/meshes/TrueGaze/GazeBeam.nif

When in-game diagnostic visuals are enabled (via 'tgvisuals' in console or TrueGaze.ini),
TrueGaze probes this directory first. If 'GazeBeam.nif' is not present, TrueGaze seamlessly
defaults to the verified engine NiPointLight emitter fallback (pure lighting attached to the
pupils and gaze terminus, requiring zero meshes or external assets).
