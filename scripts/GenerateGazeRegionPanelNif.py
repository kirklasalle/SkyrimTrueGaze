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
    """
    Generates a valid Skyrim Special Edition NIF (v20.2.0.7, user 12/100)
    for the HCEP floating gaze diagram panel.

    - Root node: NiNode ("GazeRegionPanel")
    - Quad mesh: BSTriShape ("GazeRegionPanel:0") in the XZ plane, facing +Y (forward)
    - Aspect ratio: 2760 / 1504 ~= 1.8351064
    - Texture: textures\\TrueGaze\\GazeRegionPanel.dds
    - Shader: BSEffectShaderProperty with emissive illumination + NiAlphaProperty
    """
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)

    # Unit dimensions with exact 2760 / 1504 aspect ratio
    aspect = 2760.0 / 1504.0
    hw = aspect * 0.5  # half-width ~ 0.91755
    hh = 0.5           # half-height = 0.5

    # 4 vertices: Quad in XZ plane at Y=0, normal facing +Y
    # Vertex 0: Bottom-Left
    # Vertex 1: Bottom-Right
    # Vertex 2: Top-Right
    # Vertex 3: Top-Left
    positions = [
        (-hw, 0.0, -hh),
        ( hw, 0.0, -hh),
        ( hw, 0.0,  hh),
        (-hw, 0.0,  hh)
    ]

    # UV coordinates (0,0 is top-left in texture space for BSEffectShader)
    # V=1.0 at bottom (-hh), V=0.0 at top (+hh)
    uv_coords = [
        (0.0, 1.0),
        (1.0, 1.0),
        (1.0, 0.0),
        (0.0, 0.0)
    ]

    normal_bytes = _pack_normal(0.0, 1.0, 0.0)
    color_white = bytes([255, 255, 255, 255])

    vert_bytes = bytearray()
    for i in range(4):
        x, y, z = positions[i]
        vert_bytes += struct.pack('<3f', x, y, z)
        u_h = _float_to_half(uv_coords[i][0])
        v_h = _float_to_half(uv_coords[i][1])
        vert_bytes += struct.pack('<2H', u_h, v_h)
        vert_bytes += normal_bytes
        vert_bytes += color_white

    # 4 triangles: 2 front-facing, 2 back-facing (double-sided)
    triangles = [
        (0, 1, 2),  # Front 1
        (0, 2, 3),  # Front 2
        (0, 2, 1),  # Back 1
        (0, 3, 2)   # Back 2
    ]

    tri_bytes = bytearray()
    for t in triangles:
        tri_bytes += struct.pack('<3H', t[0], t[1], t[2])

    num_verts = len(positions)
    num_tris = len(triangles)
    data_size = len(tri_bytes) + len(vert_bytes)
    bound_radius = math.sqrt(hw*hw + hh*hh) * 1.05

    # Block 0: NiNode ("GazeRegionPanel")
    b0 = bytearray()
    b0 += struct.pack('<i', 0)          # name string index 0: 'GazeRegionPanel'
    b0 += struct.pack('<I', 0)          # num_extra_data
    b0 += struct.pack('<i', -1)         # controller
    b0 += struct.pack('<I', 0x0008000E) # flags
    b0 += struct.pack('<9f', 1,0,0, 0,1,0, 0,0,1) # rotation identity
    b0 += struct.pack('<3f', 0, 0, 0)   # translation
    b0 += struct.pack('<f', 1.0)        # scale
    b0 += struct.pack('<i', -1)         # collision
    b0 += struct.pack('<I', 1)          # num_children = 1
    b0 += struct.pack('<i', 1)          # child 0 = Block 1
    b0 += struct.pack('<I', 0)          # num_effects = 0

    # Block 1: BSTriShape ("GazeRegionPanel:0")
    b1 = bytearray()
    b1 += struct.pack('<i', 1)          # name string index 1: 'GazeRegionPanel:0'
    b1 += struct.pack('<I', 0)          # num_extra_data
    b1 += struct.pack('<i', -1)         # controller
    b1 += struct.pack('<I', 0x0008000E) # flags
    b1 += struct.pack('<9f', 1,0,0, 0,1,0, 0,0,1) # rotation identity
    b1 += struct.pack('<3f', 0, 0, 0)   # translation
    b1 += struct.pack('<f', 1.0)        # scale
    b1 += struct.pack('<i', -1)         # collision
    b1 += struct.pack('<3f', 0.0, 0.0, 0.0) # bound center
    b1 += struct.pack('<f', bound_radius)   # bound radius
    b1 += struct.pack('<i', -1)         # skin ref
    b1 += struct.pack('<i', 2)          # shader property = Block 2
    b1 += struct.pack('<i', 3)          # alpha property = Block 3
    b1 += struct.pack('<Q', 0x0002900005040006) # vertex_flags: Pos+UV+Norm+Color
    b1 += struct.pack('<H', num_tris)
    b1 += struct.pack('<H', num_verts)
    b1 += struct.pack('<I', data_size)
    b1 += tri_bytes
    b1 += vert_bytes

    # Block 2: BSEffectShaderProperty
    b2 = bytearray()
    b2 += struct.pack('<i', -1)         # controller 1
    b2 += struct.pack('<I', 0)          # flags
    b2 += struct.pack('<i', -1)         # controller 2
    b2 += struct.pack('<I', 0x80000008) # shader flags 1: ZBuffer_Test
    b2 += struct.pack('<I', 0x00000021) # shader flags 2: Double_Sided | Glow_Map
    b2 += struct.pack('<2f', 0.0, 0.0)  # UV offset
    b2 += struct.pack('<2f', 1.0, 1.0)  # UV scale
    b2 += struct.pack('<i', 2)          # source texture string index 2
    b2 += struct.pack('<I', 0x0000FF03) # texture clamp mode
    b2 += struct.pack('<4f', 1.0, 1.0, 0.0, 0.0) # falloff
    b2 += struct.pack('<4f', 1.0, 1.0, 1.0, 1.0) # base color RGBA
    b2 += struct.pack('<f', 1.0)        # base color scale
    b2 += struct.pack('<f', 100.0)      # soft falloff depth
    b2 += struct.pack('<i', 0)          # greyscale texture string index (none)

    # Block 3: NiAlphaProperty (SrcAlpha / InvSrcAlpha)
    b3 = bytearray()
    b3 += struct.pack('<i', -1)         # controller 1
    b3 += struct.pack('<I', 0)          # num_extra
    b3 += struct.pack('<i', -1)         # controller 2
    b3 += struct.pack('<H', 0x10ED)     # flags: SrcAlpha / InvSrcAlpha + test
    b3 += struct.pack('<B', 0)          # threshold = 0 (smooth alpha blending)

    blocks = [b0, b1, b2, b3]
    block_sizes = [len(b) for b in blocks]
    block_types = ['NiNode', 'BSTriShape', 'BSEffectShaderProperty', 'NiAlphaProperty']
    type_indices = [0, 1, 2, 3]
    strings = [
        'GazeRegionPanel',
        'GazeRegionPanel:0',
        r'textures\TrueGaze\GazeRegionPanel.dds'
    ]

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
    return True


if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else r"skyrim\meshes\TrueGaze\GazeRegionPanel.nif"
    generate_gaze_region_panel_nif(dest)
    # Also write to build/TrueGaze/meshes/TrueGaze if build exists
    build_dest = r"build\TrueGaze\meshes\TrueGaze\GazeRegionPanel.nif"
    generate_gaze_region_panel_nif(build_dest)
