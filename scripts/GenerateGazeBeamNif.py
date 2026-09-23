import struct
import math
import os


def generate_gaze_beam_nif(output_path, radius=1.0, length=1.0, sides=6):
    """
    Generates a valid Skyrim Special Edition NIF (v20.2.0.7, user 12/100)
    for TrueGaze in-game "Superman laser eye" gaze rays.

    - Unit cylinder geometry along +Y (from Y=0 to Y=1.0, radius=1.0).
    - Runtime non-uniform scaling via BeamTransform() handles the thin cross-section
      radius (~0.28 Skyrim units = ~4mm radius) and target reach length (~700 units = 10m).
    - Root is NiNode (NOT BSFadeNode).
    - Uses BSEffectShaderProperty with emissive self-illumination and
      NiAlphaProperty with additive blending for the glowing laser look.
    - Double-sided so the beam is visible from all angles.
    - Embedded vertex colours: TrueGaze gold (201, 168, 106, 255).
    """
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)

    color_rgba = bytes([201, 168, 106, 255])

    # Vertices: 2 rings of 'sides' vertices + 2 cap center vertices
    verts = []
    normals = []
    uvs = []

    # Ring 0 at Y=0 (beam origin / pupil)
    for i in range(sides):
        angle = 2.0 * math.pi * i / sides
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)
        verts.append((x, 0.0, z))
        # Normal points radially outwards
        nx = math.cos(angle)
        nz = math.sin(angle)
        normals.append(_pack_normal(nx, 0.0, nz))
        u_half = _float_to_half(i / sides)
        v_half = _float_to_half(0.0)
        uvs.append((u_half, v_half))

    # Ring 1 at Y=length (beam terminus)
    for i in range(sides):
        angle = 2.0 * math.pi * i / sides
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)
        verts.append((x, length, z))
        nx = math.cos(angle)
        nz = math.sin(angle)
        normals.append(_pack_normal(nx, 0.0, nz))
        u_half = _float_to_half(i / sides)
        v_half = _float_to_half(1.0)
        uvs.append((u_half, v_half))

    num_verts = len(verts)

    # Triangles: 2 triangles per quad (cylinder sides), double-sided
    triangles = []
    for i in range(sides):
        next_i = (i + 1) % sides
        v0 = i
        v1 = next_i
        v2 = i + sides
        v3 = next_i + sides
        # Front face
        triangles.append((v0, v1, v2))
        triangles.append((v1, v3, v2))
        # Back face (double-sided visibility)
        triangles.append((v0, v2, v1))
        triangles.append((v1, v2, v3))

    num_tris = len(triangles)

    # Bounding sphere — kept small to prevent BSPortalGraph room culling
    bound_center = (0.0, length * 0.5, 0.0)
    bound_radius = math.sqrt((length * 0.5) ** 2 + radius ** 2) * 1.05

    # Vertex data: 24 bytes per vertex (3f pos + 2H uv + 4B normal + 4B color)
    vert_bytes = bytearray()
    for i in range(num_verts):
        x, y, z = verts[i]
        vert_bytes += struct.pack('<3f', x, y, z)
        u, v = uvs[i]
        vert_bytes += struct.pack('<2H', u, v)
        vert_bytes += normals[i]
        vert_bytes += color_rgba

    # Triangle data: 6 bytes per triangle (3 x uint16)
    tri_bytes = bytearray()
    for t in triangles:
        tri_bytes += struct.pack('<3H', t[0], t[1], t[2])

    data_size = len(tri_bytes) + len(vert_bytes)

    # -----------------------------------------------------------------------
    # Block 0: NiNode ("GazeBeam") — root node
    # -----------------------------------------------------------------------
    b0 = bytearray()
    b0 += struct.pack('<i', 0)   # Name string index 0: 'GazeBeam'
    b0 += struct.pack('<I', 0)   # num_extra_data
    b0 += struct.pack('<i', -1)  # controller ref
    b0 += struct.pack('<I', 0x0008000E)  # flags (visible, no skinning)
    b0 += struct.pack('<9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation identity
    b0 += struct.pack('<3f', 0, 0, 0)    # translation
    b0 += struct.pack('<f', 1.0)         # scale
    b0 += struct.pack('<i', -1)          # collision ref
    b0 += struct.pack('<I', 1)           # num_children = 1
    b0 += struct.pack('<i', 1)           # child 0 = Block 1 (BSTriShape)
    b0 += struct.pack('<I', 0)           # num_effects = 0

    # -----------------------------------------------------------------------
    # Block 1: BSTriShape ("GazeBeam:0") — the beam cylinder
    # -----------------------------------------------------------------------
    b1 = bytearray()
    b1 += struct.pack('<i', 1)   # Name string index 1: 'GazeBeam:0'
    b1 += struct.pack('<I', 0)   # num_extra_data
    b1 += struct.pack('<i', -1)  # controller ref
    b1 += struct.pack('<I', 0x0008000E)  # flags
    b1 += struct.pack('<9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation identity
    b1 += struct.pack('<3f', 0, 0, 0)    # translation
    b1 += struct.pack('<f', 1.0)         # scale
    b1 += struct.pack('<i', -1)          # collision ref
    b1 += struct.pack('<3f', *bound_center)  # bounding sphere center
    b1 += struct.pack('<f', bound_radius)    # bounding sphere radius
    b1 += struct.pack('<i', -1)          # skin ref
    b1 += struct.pack('<i', 2)           # shader property = Block 2
    b1 += struct.pack('<i', 3)           # alpha property = Block 3
    b1 += struct.pack('<Q', 0x0002900005040006)  # vertex_flags: Pos+UV+Norm+Color
    b1 += struct.pack('<H', num_tris)    # Num Triangles (ushort for SSE)
    b1 += struct.pack('<H', num_verts)   # Num Vertices (ushort)
    b1 += struct.pack('<I', data_size)   # Data Size (tri_bytes + vert_bytes)
    b1 += tri_bytes
    b1 += vert_bytes
    # SSE: Particle Data Size (uint32). 0 = no particle collision mesh.
    b1 += struct.pack('<I', 0)

    # -----------------------------------------------------------------------
    # Block 2: BSEffectShaderProperty — emissive self-illuminated glow
    #
    # SSE format (BSVER 100) per nif.xml. Inherits NiObjectNET fields:
    #   - Name (NiFixedString = uint32 string index)
    #   - Num Extra Data (uint32)
    #   - Extra Data List (Ref[] - absent when count=0)
    #   - Controller (Ref = int32)
    # Then BSShaderProperty fields:
    #   - Shader Flags (uint32) + Shader Flags 2 (uint32) [for FO3 only]
    #   - Env Map Scale (float) [for FO3 only]
    # Then BSEffectShaderProperty own fields.
    #
    # For SSE, BSShaderProperty fields after NiObjectNET are EMPTY because
    # the FO3-specific fields have vercond="#NI_BS_LTE_FO3#".
    # -----------------------------------------------------------------------
    b2 = bytearray()
    # --- NiObjectNET fields ---
    # Name: NiFixedString (string index). -1 = no name.
    b2 += struct.pack('<i', -1)
    # Num Extra Data List
    b2 += struct.pack('<I', 0)
    # Controller ref
    b2 += struct.pack('<i', -1)
    # --- BSEffectShaderProperty own fields (SSE, BSVER=100) ---
    # Shader Flags 1 (SkyrimShaderPropertyFlags1): ZBuffer_Test (bit 31)
    b2 += struct.pack('<I', 0x80000000)
    # Shader Flags 2 (SkyrimShaderPropertyFlags2): Double_Sided (bit 4) | Vertex_Colors (bit 5)
    b2 += struct.pack('<I', 0x00000030)
    # UV Offset (TexCoord: 2 half-floats as 2 floats in SSE)
    b2 += struct.pack('<2f', 0.0, 0.0)
    # UV Scale (TexCoord)
    b2 += struct.pack('<2f', 1.0, 1.0)
    # Source Texture: SizedString (uint32 length + chars). Length 0 = empty.
    b2 += struct.pack('<I', 0)
    # Texture Clamp Mode (byte): 3 = WRAP_S_WRAP_T
    b2 += struct.pack('<B', 3)
    # Lighting Influence (byte): 0 = no external lighting influence
    b2 += struct.pack('<B', 0)
    # Env Map Min LOD (byte)
    b2 += struct.pack('<B', 0)
    # Unused Byte
    b2 += struct.pack('<B', 0)
    # Falloff Start Angle (float)
    b2 += struct.pack('<f', 1.0)
    # Falloff Stop Angle (float)
    b2 += struct.pack('<f', 1.0)
    # Falloff Start Opacity (float)
    b2 += struct.pack('<f', 0.0)
    # Falloff Stop Opacity (float)
    b2 += struct.pack('<f', 0.0)
    # Base colour: TrueGaze gold with high alpha
    b2 += struct.pack('<4f', 201.0/255.0, 168.0/255.0, 106.0/255.0, 0.95)
    # Base Color Scale (float): bright laser glow
    b2 += struct.pack('<f', 2.5)
    # Soft Falloff Depth (float)
    b2 += struct.pack('<f', 100.0)
    # Greyscale Texture: SizedString. Length 0 = empty.
    b2 += struct.pack('<I', 0)

    # -----------------------------------------------------------------------
    # Block 3: NiAlphaProperty — additive blending for laser glow
    #
    # NiAlphaProperty inherits NiProperty → NiObjectNET.
    # NiObjectNET fields: Name (NiFixedString), Num Extra Data (uint),
    #   Extra Data List (Ref[]), Controller (Ref).
    # NiAlphaProperty own fields: Flags (AlphaFlags, ushort), Threshold (byte).
    # -----------------------------------------------------------------------
    b3 = bytearray()
    # --- NiObjectNET fields ---
    b3 += struct.pack('<i', -1)  # Name: no name
    b3 += struct.pack('<I', 0)   # Num Extra Data List
    b3 += struct.pack('<i', -1)  # Controller ref
    # --- NiAlphaProperty own fields ---
    # Alpha flags: enable blending, SrcAlpha / InvSrcAlpha, alpha test enabled
    # Bits: 0=blend enable, 1-4=src blend (SRC_ALPHA=6), 5-8=dst blend (INV_SRC_ALPHA=7)
    #        9=alpha test, 10-12=test func (GREATER=4), 13=no sorter
    # 0x10ED = 0001 0000 1110 1101 → blend=1, src=6(SrcAlpha), dst=7(InvSrcAlpha), test=1, func=4(Greater)
    b3 += struct.pack('<H', 0x10ED)
    b3 += struct.pack('<B', 0)   # threshold 0 (show all alpha values)

    # -----------------------------------------------------------------------
    # Assemble the NIF
    # -----------------------------------------------------------------------
    blocks = [b0, b1, b2, b3]
    block_sizes = [len(b) for b in blocks]
    # NiNode root (NOT BSFadeNode!) — critical for correct rendering
    block_types = ['NiNode', 'BSTriShape', 'BSEffectShaderProperty', 'NiAlphaProperty']
    type_indices = [0, 1, 2, 3]
    strings = ['GazeBeam', 'GazeBeam:0']

    # NIF header
    hdr = bytearray()
    hdr += b'Gamebryo File Format, Version 20.2.0.7\n'
    hdr += struct.pack('<I', 0x14020007)  # version
    hdr += struct.pack('<B', 1)           # little endian
    hdr += struct.pack('<I', 12)          # user version
    hdr += struct.pack('<I', len(blocks)) # num blocks
    hdr += struct.pack('<I', 100)         # user version 2
    hdr += struct.pack('<3B', 0, 0, 0)   # author, process script, export script lengths
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
    hdr += struct.pack('<I', 0)  # num_groups

    # NIF footer
    footer = bytearray()
    footer += struct.pack('<I', 1)   # num_roots
    footer += struct.pack('<i', 0)   # root = Block 0

    full_nif = hdr + b''.join(blocks) + footer
    with open(output_path, 'wb') as f:
        f.write(full_nif)

    print(f"Generated: {output_path}")
    print(f"  Size: {len(full_nif)} bytes")
    print(f"  Vertices: {num_verts}, Triangles: {num_tris}")
    print(f"  Radius: {radius} units ({radius/70.0*100:.1f} cm)")
    print(f"  Length: {length} units ({length/70.0:.1f} m)")
    print(f"  Root: NiNode (NOT BSFadeNode)")


def _float_to_half(val):
    """Convert a float to IEEE 754 half-precision, returned as uint16."""
    return struct.unpack('<H', struct.pack('<e', val))[0]


def _pack_normal(nx, ny, nz):
    """Pack a unit normal into 4 bytes (x, y, z, bitangent_sign)."""
    def _clamp_byte(v):
        return max(0, min(255, int(v * 127 + 128))) & 0xFF
    return bytes([_clamp_byte(nx), _clamp_byte(ny), _clamp_byte(nz), 127])


if __name__ == "__main__":
    import sys
    dest = sys.argv[1] if len(sys.argv) > 1 else r"skyrim\meshes\TrueGaze\GazeBeam.nif"
    generate_gaze_beam_nif(dest)
    build_dest = r"build\TrueGaze\meshes\TrueGaze\GazeBeam.nif"
    generate_gaze_beam_nif(build_dest)
