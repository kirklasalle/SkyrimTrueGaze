"""
TrueGaze GazeRegionPanel NIF Generator — SSE-correct rewrite (2026-10-01).

Generates a valid Skyrim Special Edition NIF (v20.2.0.7, user 12, BSVER 100)
for the HCEP floating gaze-region diagram panel (Kirk's chroma-keyed texture).

WHY THE PREVIOUS VERSION WAS INVISIBLE (see /memories/repo/skyrim-asset-pipeline.md):
  1. Positions were written as 3 floats (12 bytes) but the VertexDesc declared
     the canonical SSE layout where positions are 16 bytes (xyzw). Every
     attribute after position shifted 4 bytes early: the colour attribute read
     the trailing zero bytes -> RGBA(0,0,0,0) -> alpha 0 -> fully transparent.
  2. Shader Flags2 = 0x21 = ZBuffer_Write|Vertex_Colors — NOT TwoSided
     (bit 4). The quad's only front face pointed AWAY from a viewer looking
     at the NPC, so it was back-face culled.
  3. NiAlphaProperty 0x100D = DEST_ALPHA/ONE blending — wrong mode, invisible
     against opaque geometry.
  4. Authored 1 unit tall with runtime scale 5 -> a 7 cm postage stamp.

THIS VERSION:
  - 16-byte positions (xyzw), VertexDesc 0x0003B00007650408 (32 B vertices:
    pos16 + uv4 + normal4 + tangent4 + color4) — matches the declared desc.
  - Flags2 0x30 = TwoSided|Vertex_Colors (visible from both sides).
  - NiAlphaProperty 0x00A9 = SrcAlpha/InvSrcAlpha standard alpha blend.
  - Quad authored 1 unit tall in the XZ plane; runtime scale (default 25)
    makes it ~36 cm tall — a readable floating diagram.
  - Texture: textures\\TrueGaze\\GazeRegionPanel.dds (Kirk's chroma-keyed
    HCEP-02 diagram; alpha channel keys out the white canvas).
"""

import struct
import math
import os
import sys


def _float_to_half(val):
    """Convert float to IEEE 754 half-precision (uint16)."""
    return struct.unpack('<H', struct.pack('<e', val))[0]


def _pack_normal(nx, ny, nz):
    """Pack unit normal into 4 bytes (x, y, z, bitangent_sign)."""
    def _clamp_byte(v):
        return max(0, min(255, int(v * 127 + 128))) & 0xFF
    return bytes([_clamp_byte(nx), _clamp_byte(ny), _clamp_byte(nz), 127])


def generate_gaze_region_panel_nif(output_path):
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)

    # Unit quad in the XZ plane, normal +Y (head-local forward).
    # Aspect ratio matches the source diagram 2760 x 1504.
    aspect = 2760.0 / 1504.0
    hw = aspect * 0.5  # half-width ~ 0.91755
    hh = 0.5           # half-height

    positions = [
        (-hw, 0.0, -hh),  # 0: Bottom-Left
        ( hw, 0.0, -hh),  # 1: Bottom-Right
        ( hw, 0.0,  hh),  # 2: Top-Right
        (-hw, 0.0,  hh),  # 3: Top-Left
    ]

    uvs = [
        (0.0, 1.0),
        (1.0, 1.0),
        (1.0, 0.0),
        (0.0, 0.0),
    ]

    norm_bytes = _pack_normal(0.0, 1.0, 0.0)   # +Y normal
    tang_bytes = _pack_normal(1.0, 0.0, 0.0)   # +X tangent
    col_white = bytes([255, 255, 255, 255])    # BGRA white, opaque

    # 32-byte vertex struct matching VertexDesc 0x0003B00007650408:
    # pos xyzw (16) + uv half2 (4) + normal (4) + tangent (4) + color (4)
    vert_bytes = bytearray()
    for i in range(4):
        x, y, z = positions[i]
        vert_bytes += struct.pack('<4f', x, y, z, 0.0)  # 16 B position
        u_h = _float_to_half(uvs[i][0])
        v_h = _float_to_half(uvs[i][1])
        vert_bytes += struct.pack('<2H', u_h, v_h)       # 4 B uv
        vert_bytes += norm_bytes                        # 4 B normal
        vert_bytes += tang_bytes                        # 4 B tangent
        vert_bytes += col_white                         # 4 B color

    # Two triangles (TwoSided flag renders both faces; no need to duplicate).
    triangles = [
        (0, 1, 2),
        (0, 2, 3),
    ]

    tri_bytes = bytearray()
    for t in triangles:
        tri_bytes += struct.pack('<3H', t[0], t[1], t[2])

    num_verts = len(positions)
    num_tris = len(triangles)
    data_size = len(tri_bytes) + len(vert_bytes)
    bound_radius = math.sqrt(hw * hw + hh * hh) * 1.05

    # Block 0: NiNode ("GazeRegionPanel")
    b0 = bytearray()
    b0 += struct.pack('<i', 0)           # name string index 0
    b0 += struct.pack('<I', 0)           # num_extra_data
    b0 += struct.pack('<i', -1)          # controller
    b0 += struct.pack('<I', 0x0008000E)  # flags
    b0 += struct.pack('<9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation identity
    b0 += struct.pack('<3f', 0, 0, 0)    # translation
    b0 += struct.pack('<f', 1.0)         # scale
    b0 += struct.pack('<i', -1)          # collision
    b0 += struct.pack('<I', 1)           # num_children = 1
    b0 += struct.pack('<i', 1)           # child 0 = Block 1
    b0 += struct.pack('<I', 0)           # num_effects = 0

    # Block 1: BSTriShape ("GazeRegionPanel:0")
    b1 = bytearray()
    b1 += struct.pack('<i', 1)           # name string index 1
    b1 += struct.pack('<I', 0)           # num_extra_data
    b1 += struct.pack('<i', -1)          # controller
    b1 += struct.pack('<I', 0x0008000E)  # flags
    b1 += struct.pack('<9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation identity
    b1 += struct.pack('<3f', 0, 0, 0)    # translation
    b1 += struct.pack('<f', 1.0)         # scale
    b1 += struct.pack('<i', -1)          # collision
    b1 += struct.pack('<3f', 0.0, 0.0, 0.0)  # bound center
    b1 += struct.pack('<f', bound_radius)    # bound radius
    b1 += struct.pack('<i', -1)          # skin ref
    b1 += struct.pack('<i', 2)           # shader property = Block 2
    b1 += struct.pack('<i', 3)           # alpha property = Block 3
    # VertexDesc: flags 0x3B (VERTEX|UV|NORMAL|TANGENT|COLORS) << 44,
    # size 32 B (8 x 4B), offsets: UV=16, Normal=20, Tangent=24, Colors=28.
    b1 += struct.pack('<Q', 0x0003B00007650408)
    b1 += struct.pack('<H', num_tris)
    b1 += struct.pack('<H', num_verts)
    b1 += struct.pack('<I', data_size)
    b1 += tri_bytes
    b1 += vert_bytes
    b1 += struct.pack('<I', 0)           # particle data size (mandatory SSE)

    # Block 2: BSEffectShaderProperty
    tex_path = r"textures\TrueGaze\GazeRegionPanel.dds"
    tex_bytes = tex_path.encode('ascii')
    b2 = bytearray()
    b2 += struct.pack('<i', -1)           # name
    b2 += struct.pack('<I', 0)            # num extra data
    b2 += struct.pack('<i', -1)           # controller
    # Flags1: ZBuffer_Test (bit 31)
    b2 += struct.pack('<I', 0x80000000)
    # Flags2: TwoSided (bit 4) | Vertex_Colors (bit 5)
    b2 += struct.pack('<I', 0x00000030)
    b2 += struct.pack('<2f', 0.0, 0.0)    # UV offset
    b2 += struct.pack('<2f', 1.0, 1.0)    # UV scale
    b2 += struct.pack('<I', len(tex_bytes))  # SizedString length
    b2 += tex_bytes                          # relative texture path
    b2 += struct.pack('<B', 3)            # clamp mode: WRAP_S_WRAP_T
    b2 += struct.pack('<B', 0)            # lighting influence: unshaded
    b2 += struct.pack('<B', 0)            # env map min LOD
    b2 += struct.pack('<B', 0)            # unused
    b2 += struct.pack('<f', 1.0)          # falloff start angle
    b2 += struct.pack('<f', 1.0)          # falloff stop angle
    b2 += struct.pack('<f', 0.0)          # falloff start opacity
    b2 += struct.pack('<f', 0.0)          # falloff stop opacity
    b2 += struct.pack('<4f', 1.0, 1.0, 1.0, 1.0)  # base colour: white (texture supplies colour)
    b2 += struct.pack('<f', 1.0)          # base colour scale
    b2 += struct.pack('<f', 100.0)        # soft falloff depth
    b2 += struct.pack('<I', 0)            # greyscale texture: empty

    # Block 3: NiAlphaProperty — standard alpha blend (SrcAlpha/InvSrcAlpha)
    # Bits: 0=blend enable, 1-4=src, 5-8=dst, 9=test enable.
    # SrcAlpha=4, InvSrcAlpha=5 -> (4 << 1) | (5 << 5) | 1 = 0x00A9.
    b3 = bytearray()
    b3 += struct.pack('<i', -1)           # name
    b3 += struct.pack('<I', 0)            # num extra
    b3 += struct.pack('<i', -1)           # controller
    b3 += struct.pack('<H', 0x00A9)       # blend: SrcAlpha / InvSrcAlpha
    b3 += struct.pack('<B', 0)            # threshold

    blocks = [b0, b1, b2, b3]
    block_sizes = [len(b) for b in blocks]
    block_types = ['NiNode', 'BSTriShape', 'BSEffectShaderProperty', 'NiAlphaProperty']
    type_indices = [0, 1, 2, 3]
    strings = ['GazeRegionPanel', 'GazeRegionPanel:0']

    hdr = bytearray()
    hdr += b'Gamebryo File Format, Version 20.2.0.7\n'
    hdr += struct.pack('<I', 0x14020007)
    hdr += struct.pack('<B', 1)
    hdr += struct.pack('<I', 12)
    hdr += struct.pack('<I', len(blocks))
    hdr += struct.pack('<I', 100)
    hdr += struct.pack('<3B', 0, 0, 0)
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
    hdr += struct.pack('<I', 0)

    footer = bytearray()
    footer += struct.pack('<I', 1)
    footer += struct.pack('<i', 0)

    full_nif = hdr + b''.join(blocks) + footer
    with open(output_path, 'wb') as f:
        f.write(full_nif)

    print(f"Generated GazeRegionPanel NIF: {output_path} ({len(full_nif)} bytes)")
    print(f"  VertexDesc: 0x0003B00007650408 (32B: pos16+uv4+nrm4+tang4+col4)")
    print(f"  Flags2: 0x30 (TwoSided|VertexColors), alpha 0x00A9 (SrcAlpha/InvSrcAlpha)")
    return True


if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else r"skyrim\meshes\TrueGaze\GazeRegionPanel.nif"
    generate_gaze_region_panel_nif(dest)
    build_dest = r"build\TrueGaze\meshes\TrueGaze\GazeRegionPanel.nif"
    generate_gaze_region_panel_nif(build_dest)
