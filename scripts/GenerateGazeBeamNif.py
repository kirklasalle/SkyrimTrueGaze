"""
TrueGaze GazeBeam NIF Generator — SSE-correct rewrite (2026-10-01).

Generates a valid Skyrim Special Edition NIF (v20.2.0.7, user 12, BSVER 100)
for TrueGaze "Superman laser eye" gaze rays.

WHY THE PREVIOUS VERSION WAS INVISIBLE (see /memories/repo/skyrim-asset-pipeline.md):
  1. Authored radius=1.0 / length=1.0 Skyrim units = a 1.4 cm pebble; the
     runtime applies scale=1.0 for the custom mesh -> nothing to see.
  2. VertexDesc 0x0002900005040006 had NO VF_UV flag and ALL attribute offsets
     were zero -> the engine read attributes from the wrong bytes.
  3. Positions were written as 3 floats (12 bytes). SSE (BSVER 100) positions
     are ALWAYS 16 bytes (xyzw) per NifSkope ResetAttributeOffsets:
     `if (vf & VF_FULLPREC || stream == 100) attributeSizes[VA_POSITION] = 4;`
  4. NiAlphaProperty 0x10ED = DEST_ALPHA/INV_DEST_ALPHA blending -> invisible
     against opaque scene geometry.

THIS VERSION:
  - Cylinder authored along +Y (Gamebryo forward) at REAL Skyrim scale:
    length = 70 units (1 metre), radius = 0.35 units (5 mm) — pencil-thin.
    The runtime scales uniformly by fGazeRayLengthMeters (70 units per metre),
    so cross-section and length grow together, exactly like a laser beam.
  - VertexDesc 0x002B000006050407: VF_VERTEX|VF_UV|VF_NORMAL|VF_COLORS,
    vertex size 28 bytes, offsets UV=16, Normal=20, Colors=24 (canonical SSE).
  - 16-byte positions (xyzw), half-float UVs, byte normals, BGRA vertex colour.
  - BSEffectShaderProperty emissive gold + NiAlphaProperty ADDITIVE blending
    (One/One = flags 0x0001) for the glowing laser look.
  - Root is NiNode (NOT BSFadeNode) so actor fading never alpha-kills the beam.
"""

import struct
import math
import os


def _float_to_half(val):
    """Convert a float to IEEE 754 half-precision, returned as uint16."""
    return struct.unpack('<H', struct.pack('<e', val))[0]


def _pack_normal(nx, ny, nz):
    """Pack a unit normal into 4 bytes (x, y, z, bitangent_sign)."""
    def _clamp_byte(v):
        return max(0, min(255, int(v * 127 + 128))) & 0xFF
    return bytes([_clamp_byte(nx), _clamp_byte(ny), _clamp_byte(nz), 127])


def generate_gaze_beam_nif(output_path, radius=0.35, length=70.0, sides=6):
    """
    radius/length are in SKYRIM UNITS (1 unit = 1.4 cm; 70 units = 1 m).
    Default: 1 metre beam, 5 mm radius — a pencil-thin laser.
    The runtime scales this mesh uniformly by fGazeRayLengthMeters.
    """
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)

    # TrueGaze gold (201, 168, 106) in BGRA vertex-colour order.
    color_bgra = bytes([106, 168, 201, 255])

    # --- Geometry: 2 rings of 'sides' vertices, cylinder along +Y -----------
    verts = []   # (x, y, z) positions
    uvs = []     # (u, v)
    normals = [] # packed 4-byte normals

    for ring, y in enumerate((0.0, length)):
        v = 0.0 if ring == 0 else 1.0
        for i in range(sides):
            angle = 2.0 * math.pi * i / sides
            x = radius * math.cos(angle)
            z = radius * math.sin(angle)
            verts.append((x, y, z))
            normals.append(_pack_normal(math.cos(angle), 0.0, math.sin(angle)))
            uvs.append((_float_to_half(i / sides), _float_to_half(v)))

    num_verts = len(verts)

    # Triangles: 2 per side-quad. The shader flag TwoSided handles back-face
    # visibility, so single-sided winding keeps the buffer minimal.
    triangles = []
    for i in range(sides):
        v0 = i
        v1 = (i + 1) % sides
        v2 = i + sides
        v3 = (i + 1) % sides + sides
        triangles.append((v0, v1, v2))
        triangles.append((v1, v3, v2))

    num_tris = len(triangles)

    # Bounding sphere — small enough to avoid BSPortalGraph room culling.
    bound_center = (0.0, length * 0.5, 0.0)
    bound_radius = math.sqrt((length * 0.5) ** 2 + radius ** 2) * 1.05

    # --- Vertex buffer: 28 bytes per vertex (SSE canonical) -----------------
    # pos xyzw (16) + uv half2 (4) + normal (4) + color BGRA (4)
    vert_bytes = bytearray()
    for i in range(num_verts):
        x, y, z = verts[i]
        vert_bytes += struct.pack('<4f', x, y, z, 0.0)   # 16 B position
        u, v = uvs[i]
        vert_bytes += struct.pack('<2H', u, v)            # 4 B uv
        vert_bytes += normals[i]                           # 4 B normal
        vert_bytes += color_bgra                           # 4 B color

    tri_bytes = bytearray()
    for t in triangles:
        tri_bytes += struct.pack('<3H', t[0], t[1], t[2])

    data_size = len(tri_bytes) + len(vert_bytes)

    # -----------------------------------------------------------------------
    # Block 0: NiNode ("GazeBeam") — root node (NOT BSFadeNode!)
    # -----------------------------------------------------------------------
    b0 = bytearray()
    b0 += struct.pack('<i', 0)   # name string index 0: 'GazeBeam'
    b0 += struct.pack('<I', 0)   # num_extra_data
    b0 += struct.pack('<i', -1)  # controller ref
    b0 += struct.pack('<I', 0x0008000E)  # flags (visible)
    b0 += struct.pack('<9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation identity
    b0 += struct.pack('<3f', 0, 0, 0)    # translation
    b0 += struct.pack('<f', 1.0)        # scale
    b0 += struct.pack('<i', -1)          # collision ref
    b0 += struct.pack('<I', 1)           # num_children = 1
    b0 += struct.pack('<i', 1)           # child 0 = Block 1 (BSTriShape)
    b0 += struct.pack('<I', 0)           # num_effects = 0

    # -----------------------------------------------------------------------
    # Block 1: BSTriShape ("GazeBeam:0") — the beam cylinder
    # -----------------------------------------------------------------------
    b1 = bytearray()
    b1 += struct.pack('<i', 1)   # name string index 1: 'GazeBeam:0'
    b1 += struct.pack('<I', 0)   # num_extra_data
    b1 += struct.pack('<i', -1)  # controller ref
    b1 += struct.pack('<I', 0x0008000E)  # flags
    b1 += struct.pack('<9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation identity
    b1 += struct.pack('<3f', 0, 0, 0)    # translation
    b1 += struct.pack('<f', 1.0)         # scale
    b1 += struct.pack('<i', -1)          # collision ref
    b1 += struct.pack('<3f', *bound_center)
    b1 += struct.pack('<f', bound_radius)
    b1 += struct.pack('<i', -1)          # skin ref
    b1 += struct.pack('<i', 2)           # shader property = Block 2
    b1 += struct.pack('<i', 3)           # alpha property = Block 3
    # VertexDesc: flags 0x2B (VERTEX|UV|NORMAL|COLORS) << 44,
    # size 28 B (7 x 4B), offsets: UV=16, Normal=20, Colors=24.
    b1 += struct.pack('<Q', 0x002B000006050407)
    b1 += struct.pack('<H', num_tris)
    b1 += struct.pack('<H', num_verts)
    b1 += struct.pack('<I', data_size)
    b1 += tri_bytes
    b1 += vert_bytes
    b1 += struct.pack('<I', 0)  # particle data size (mandatory SSE field)

    # -----------------------------------------------------------------------
    # Block 2: BSEffectShaderProperty — emissive self-illuminated glow
    # SSE (BSVER 100) field order per nif.xml / NifSkope:
    #   NiObjectNET: Name(i32), NumExtraData(u32), Controller(i32)
    #   Flags1(u32), Flags2(u32), UVOffset(2f), UVScale(2f),
    #   SourceTexture(SizedString), ClampMode(B), LightingInfluence(B),
    #   EnvMapMinLOD(B), unused(B),
    #   FalloffStartAngle(f), FalloffStopAngle(f),
    #   FalloffStartOpacity(f), FalloffStopOpacity(f),
    #   BaseColor(Color4), BaseColorScale(f), SoftFalloffDepth(f),
    #   GreyscaleTexture(SizedString)
    # -----------------------------------------------------------------------
    b2 = bytearray()
    b2 += struct.pack('<i', -1)          # name: none
    b2 += struct.pack('<I', 0)           # num extra data
    b2 += struct.pack('<i', -1)          # controller
    # Flags1: ZBuffer_Test (bit 31)
    b2 += struct.pack('<I', 0x80000000)
    # Flags2: TwoSided (bit 4) | Vertex_Colors (bit 5)
    b2 += struct.pack('<I', 0x00000030)
    b2 += struct.pack('<2f', 0.0, 0.0)   # UV offset
    b2 += struct.pack('<2f', 1.0, 1.0)   # UV scale
    # Source texture: MANDATORY for visibility. An untextured BSEffectShaderProperty
    # renders black under additive (One/One) blending — black added to the scene is
    # invisible. This white radial glow is tinted by the gold base colour.
    tex_path = r"textures\TrueGaze\GazeBeamGlow.dds"
    tex_bytes = tex_path.encode('ascii')
    b2 += struct.pack('<I', len(tex_bytes))
    b2 += tex_bytes
    b2 += struct.pack('<B', 3)            # clamp mode: WRAP_S_WRAP_T
    b2 += struct.pack('<B', 0)            # lighting influence: unshaded
    b2 += struct.pack('<B', 0)            # env map min LOD
    b2 += struct.pack('<B', 0)            # unused
    b2 += struct.pack('<f', 1.0)          # falloff start angle
    b2 += struct.pack('<f', 1.0)          # falloff stop angle
    b2 += struct.pack('<f', 0.0)          # falloff start opacity
    b2 += struct.pack('<f', 0.0)          # falloff stop opacity
    # Base colour: TrueGaze gold, opaque
    b2 += struct.pack('<4f', 201.0 / 255.0, 168.0 / 255.0, 106.0 / 255.0, 1.0)
    b2 += struct.pack('<f', 2.5)          # base colour scale: bright laser
    b2 += struct.pack('<f', 100.0)         # soft falloff depth
    b2 += struct.pack('<I', 0)             # greyscale texture: empty

    # -----------------------------------------------------------------------
    # Block 3: NiAlphaProperty — ADDITIVE blending for the laser glow
    # Bits: 0=blend enable, 1-4=src mode, 5-8=dst mode, 9=test enable,
    #       10-12=test func, 13=no sorter.
    # Modes: ONE=0, ZERO=1, SRC_COLOR=2, INV_SRC_COLOR=3, SRC_ALPHA=4,
    #        INV_SRC_ALPHA=5, DEST_ALPHA=6, INV_DEST_ALPHA=7.
    # Additive (One/One): enable + src ONE + dst ONE = 0x0001.
    # -----------------------------------------------------------------------
    b3 = bytearray()
    b3 += struct.pack('<i', -1)   # name: none
    b3 += struct.pack('<I', 0)    # num extra data
    b3 += struct.pack('<i', -1)   # controller
    b3 += struct.pack('<H', 0x0001)  # blend enable, src=ONE, dst=ONE (additive)
    b3 += struct.pack('<B', 0)       # threshold

    # -----------------------------------------------------------------------
    # Assemble the NIF
    # -----------------------------------------------------------------------
    blocks = [b0, b1, b2, b3]
    block_sizes = [len(b) for b in blocks]
    block_types = ['NiNode', 'BSTriShape', 'BSEffectShaderProperty', 'NiAlphaProperty']
    type_indices = [0, 1, 2, 3]
    strings = ['GazeBeam', 'GazeBeam:0']

    hdr = bytearray()
    hdr += b'Gamebryo File Format, Version 20.2.0.7\n'
    hdr += struct.pack('<I', 0x14020007)  # version
    hdr += struct.pack('<B', 1)           # little endian
    hdr += struct.pack('<I', 12)           # user version
    hdr += struct.pack('<I', len(blocks)) # num blocks
    hdr += struct.pack('<I', 100)         # user version 2 (BSVER)
    hdr += struct.pack('<3B', 0, 0, 0)    # author/process/export lengths
    hdr += struct.pack('<H', len(block_types))
    for ts in block_types:
        hdr += struct.pack('<I', len(ts))
        hdr += ts.encode('ascii')
    for ti in type_indices:
        hdr += struct.pack('<H', ti)
    for bs in block_sizes:
        hdr += struct.pack('<I', bs)
    hdr += struct.pack('<I', len(strings))
    hdr += struct.pack('<I', max(len(s) for s in strings))
    for s in strings:
        hdr += struct.pack('<I', len(s))
        hdr += s.encode('ascii')
    hdr += struct.pack('<I', 0)  # num groups

    footer = bytearray()
    footer += struct.pack('<I', 1)   # num roots
    footer += struct.pack('<i', 0)   # root = Block 0

    full_nif = hdr + b''.join(blocks) + footer
    with open(output_path, 'wb') as f:
        f.write(full_nif)

    print(f"Generated: {output_path}")
    print(f"  Size: {len(full_nif)} bytes")
    print(f"  Vertices: {num_verts}, Triangles: {num_tris}")
    print(f"  Radius: {radius} units ({radius * 1.4:.2f} cm)")
    print(f"  Length: {length} units ({length / 70.0:.2f} m)")
    print(f"  VertexDesc: 0x002B000006050407 (28B: pos16+uv4+nrm4+col4)")
    print(f"  Root: NiNode, additive alpha (One/One)")


if __name__ == "__main__":
    import sys
    dest = sys.argv[1] if len(sys.argv) > 1 else r"skyrim\meshes\TrueGaze\GazeBeam.nif"
    generate_gaze_beam_nif(dest)
    build_dest = r"build\TrueGaze\meshes\TrueGaze\GazeBeam.nif"
    generate_gaze_beam_nif(build_dest)
