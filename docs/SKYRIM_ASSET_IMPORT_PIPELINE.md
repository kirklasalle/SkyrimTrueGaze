# Skyrim Special Edition (SSE) 3D Asset Creation & Import Pipeline Guide
**Author:** TrueGaze Engineering & Development  
**Target Architecture:** Skyrim Special Edition (1.6+ AE / SE / VR)  
**Reference Assets:** `GazeBeam.nif`, `GazeRegionPanel.nif`, `GazeRegionPanel.dds`

---

## 1. Executive Summary & Root Cause Analysis

When implementing in-game developer visualizations (such as Superman laser gaze rays and the HCEP-02 floating diagram panel), two catastrophic failure modes occurred in earlier builds:
1. **The "Scene Flood / Washed-Out Light" Failure**:
   Attaching multiple `NiPointLight` emitters (11 panel lights + 2 pupil lights + 1 terminus light = 14 lights per actor) to the scene graph flooded the cell with forward lighting passes. When multiple actors (player, Ralof, Ulfric, Lokir, Imperial guards) were present in enclosed scenes (such as the Helgen cart intro), 70–100 dynamic point lights overwhelmed the engine's lighting buffer, driving ambient and diffuse values to pure white and washing out all textures and geometry. Crucially, **`NiPointLight` possesses no visible 3D geometry or texture**—it only casts invisible photons onto surrounding surfaces.
2. **The `0xC0000005` Access Violation Crash**:
   Previous attempts to generate binary `.nif` files from scratch with Python failed because Skyrim Special Edition (SSE) transitioned from NetImmerse `NiTriShape` to Bethesda's proprietary `BSTriShape`. The custom binary files omitted the trailing 4-byte `Particle Data Size` field in `BSTriShape`, mispacked texture paths as integer indices instead of `SizedString` structures, and used malformed byte offsets. While `BSModelDB::Demand()` parsed the header without throwing, Skyrim's Direct3D 11 renderer dereferenced invalid memory addresses during its vertex buffer binding pass, immediately crashing the game process.

### The Solution
- **Visuals MUST Be True 3D Geometry**: Visible laser beams and floating diagram panels must be authored as valid SSE meshes utilizing `BSEffectShaderProperty` (for unshaded emissive self-illumination) and `NiAlphaProperty` (for transparent/chroma-keyed blending).
- **Point Lights Must Be Minimal**: `NiPointLight` emitters should either be disabled (`rayRenderMode = 2` / GeometryOnly) or kept to micro-radius specular accents ($r \le 1.0$, diffuse $\le 0.15$, ambient $= 0.0$).
- **Mesh Validation Before Scene Graph Insertion**: Every cloned mesh node must be checked via `HasRendererSafeGeometry()` before `AttachChild()` to ensure valid vertex buffers and non-null shader properties.

---

## 2. Skyrim Special Edition NIF Binary Architecture

Skyrim SE uses NIF version `20.2.0.7`, User Version `12`, User Version 2 (`BSVER`) `100`.

```
+-----------------------------------------------------------------------+
| NIF File Structure (SSE 20.2.0.7 / BSVER 100)                        |
+-----------------------------------------------------------------------+
| 1. Header (Version, Block Count, Block Types, Block Sizes, Strings)   |
| 2. Block 0: Root Node (NiNode or BSFadeNode)                          |
| 3. Block 1: Geometry (BSTriShape)                                     |
|    - Transform & Bounding Sphere                                      |
|    - Refs: Skin (-1), ShaderProperty (Block 2), AlphaProperty (Block 3)|
|    - Vertex Flags (uint64 bitmask: Pos, UV, Normals, Tangents, Color) |
|    - Vertex Count & Triangle Count                                    |
|    - Triangle Index Buffer (uint16 * 3 * num_tris)                    |
|    - Vertex Data Buffer (32 bytes per vertex)                         |
|    - Particle Data Size (uint32 = 0)  <-- MANDATORY SSE FIELD         |
| 4. Block 2: BSEffectShaderProperty (Emissive & Transparency)          |
|    - Shader Flags 1 & 2 (ZBuffer_Test, Double_Sided, Glow_Map)        |
|    - Texture Path (SizedString: uint32 length + ASCII chars)          |
|    - Clamp, Lighting Influence (0 = unshaded self-illumination)       |
|    - Emissive Base Color RGBA & Base Color Scale                      |
| 5. Block 3: NiAlphaProperty (Blending & Threshold)                    |
|    - Flags (0x100D or 0x10ED for SrcAlpha / InvSrcAlpha)              |
| 6. Footer (Root Count = 1, Root Index = 0)                            |
+-----------------------------------------------------------------------+
```

### The Mandatory 32-Byte Vertex Struct
For `vertex_flags = 0x0003b00007650408` (Position + Half-Float UV + Packed Normals + Packed Tangents + RGBA Color + Extra):
| Offset | Type | Field | Description |
|---|---|---|---|
| `0x00` | `float[3]` | `pos` | 3D Local Position (X, Y, Z) |
| `0x0C` | `uint16[2]` | `uv` | Half-precision IEEE 754 (U, V) |
| `0x10` | `uint8[4]` | `normal` | Packed normal vector: `int(n * 127 + 128)` |
| `0x14` | `uint8[4]` | `tangent` | Packed tangent vector: `int(t * 127 + 128)` |
| `0x18` | `uint8[4]` | `color` | Vertex Color (R, G, B, A) |
| `0x1C` | `uint8[4]` | `extra` | Secondary lighting / padding (`0, 0, 0, 0`) |

---

## 3. Creating & Importing Assets for Players, NPCs, and Creatures

### Toolchain Overview
1. **3D Modeling & Animation**:
   - **Blender (3.6 LTS or 4.x)** with the **PyNifly** or **Blender Niftools Addon**.
   - Scale: 1 Skyrim unit $\approx 1.428\text{ cm}$ ($70\text{ units} = 1.0\text{ m}$).
2. **Mesh Optimization & Rigging**:
   - **Outfit Studio / BodySlide**: The industry standard tool for converting meshes to SSE, generating slider morphs, and copying bone weights from base game skeletons (`skeleton.nif`).
   - **NifSkope 2.0 Dev 8+**: Used to verify block linkages, inspect shader flags, assign material textures, and sanitize block names.
3. **Texture Processing**:
   - **Paint.NET**, **Photoshop** (with Intel Texture Works plugin), or **GIMP**.
   - Target format: **BC7** (for fine color details) or **BC3 / DXT5** (for transparent alpha channels).

---

### Step-by-Step: Creating a 3D Chroma-Keyed Floating Panel
To create a billboard or diagnostic diagram floating in 3D space:

1. **Prepare the Source Image**:
   - Save your diagram/panel as high-resolution image (`.png` or `.jfif`).
   - Run the chroma-keying script (`GenerateHcepPanelTexture.py`) to map background pixels (e.g. solid white `RGB > 240`) to `Alpha = 0` while keeping content pixels opaque (`Alpha = 255`).
   - Save the result as a 32-bit RGBA DDS (`textures/TrueGaze/GazeRegionPanel.dds`).

2. **Generate the Planar Quad Mesh**:
   - Dimensions: Match the diagram aspect ratio (e.g. $2760 \times 1504 \approx 1.8351$).
   - Coordinates: Construct a rectangle in the XZ plane with normals facing $+Y$ (toward the camera/viewer).
   - Use `scripts/GenerateGazeRegionPanelNif.py` to compile the byte-perfect SSE NIF.

3. **Deploy to Skyrim Data Directory**:
   - Mesh: `Data/meshes/TrueGaze/GazeRegionPanel.nif`
   - Texture: `Data/textures/TrueGaze/GazeRegionPanel.dds`

---

### Step-by-Step: Creating Laser Eye Beam Geometry
1. **Geometry Design**:
   - Model a unit cylinder along the $+Y$ forward axis from $Y=0.0$ (pupil) to $Y=1.0$ (unit reach).
   - Author double-sided faces so the laser is visible when viewed from any angle.
2. **Shader Configuration (`BSEffectShaderProperty`)**:
   - Flags 1: `0x80000000` (`ZBuffer_Test`)
   - Flags 2: `0x00000030` (`Double_Sided` | `Vertex_Colors`)
   - Lighting Influence: `0` (unaffected by cell lighting; purely emissive).
   - Base Color: `#C9A86A` TrueGaze gold (or dynamically overridden at runtime via `BSEffectShaderMaterial::baseColor`).
3. **Alpha Configuration (`NiAlphaProperty`)**:
   - Flags: `0x10ED` (Additive / Translucent glow).

---

### Step-by-Step: Rigging Assets to Characters and Creatures
1. **Humanoid Skeletons (`NPC Head [Head]`)**:
   - Skyrim humanoid rigs lack dedicated eyeball bones. Eye direction is controlled via FaceGen tri-morphs (`femalehead.nif`).
   - To anchor an asset to an actor's gaze or face, anchor to the `NPC Head [Head]` bone.
   - Socket offsets from the head bone origin:
     - Forward offset $\approx +12.0\text{ cm}$ ($+8.4\text{ Skyrim units}$).
     - Up offset $\approx +6.0\text{ cm}$ ($+4.2\text{ Skyrim units}$).
     - Interpupillary distance $\approx 6.4\text{ cm}$ ($\pm 2.24\text{ units}$ along the local X axis).
2. **Creature Skeletons**:
   - Creatures possess distinct skeleton root names:
     - Canines/Wolves: `NPC Head [Head]` or `Head`
     - Dragons: `NPC Head35`
     - Horses: `Horse Head`
   - Always query the skeleton dynamically via `a_actor->Get3D()` and search for the head node by name; fall back to the root `Get3D()` node if the head bone is absent.

---

## 4. SKSE C++ Runtime Lifecycle: Loading, Cloned Instances & Safety Guards

```cpp
// 1. Demand the model from Skyrim's BSModelDB
RE::NiPointer<RE::NiNode> model;
RE::BSModelDB::DBTraits::ArgsType args;
auto result = RE::BSModelDB::Demand("meshes\\TrueGaze\\GazeRegionPanel.nif", model, args);

if (result != RE::BSResource::ErrorCode::kNone || !model) {
    logger::warn("Failed to load asset from disk/BSA.");
    return;
}

// 2. Clone the node for the specific actor instance
auto clonedObject = model->Clone();
auto* clonedNode = clonedObject ? clonedObject->AsNode() : nullptr;

// 3. MANDATORY SAFETY GUARD: Validate geometry before attaching to scene graph
if (!HasRendererSafeGeometry(clonedNode)) {
    logger::error("Mesh contains invalid BSGeometry or null shader property; rejecting.");
    return;
}

// 4. Attach to live skeleton anchor node
anchorNode->AttachChild(clonedNode, false);

// 5. Update local translation, rotation, scale and downward pass
clonedNode->local.translate = localOffset;
clonedNode->local.scale = scale;
clonedNode->world = anchorNode->world * clonedNode->local;

RE::NiUpdateData updateData;
updateData.time = 0.0f;
updateData.flags = RE::NiUpdateData::Flag::kDirty;
clonedNode->UpdateDownwardPass(updateData, 0);
```

---

## 5. Summary of Automated Verification Commands

| Command | Purpose |
|---|---|
| `python scripts/GenerateGazeRegionPanelNif.py` | Compiles the verified 3D chroma-keyed panel NIF. |
| `python scripts/GenerateGazeBeamNif.py` | Compiles the verified 3D laser eye beam NIF. |
| `python scratch/validate_panel.py` | Validates every block, byte offset, and vertex stream. |
| `python scratch/validate_beam.py` | Validates laser beam geometry and shader parameters. |
| `.\scripts\Deploy-TrueGaze.ps1 -NoLaunch` | Builds C++ DLL and deploys all meshes and textures. |
