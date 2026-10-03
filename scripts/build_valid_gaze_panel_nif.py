import struct
import math
import os
import sys

def float_to_half(val):
    return struct.unpack('<H', struct.pack('<e', val))[0]

def pack_normal(nx, ny, nz):
    def clamp_byte(v):
        return max(0, min(255, int(v * 127 + 128))) & 0xFF
    return bytes([clamp_byte(nx), clamp_byte(ny), clamp_byte(nz), 127])

def generate_valid_panel_nif(output_path):
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)

    # Aspect ratio: 2760 / 1504 ~= 1.8351064
    aspect = 2760.0 / 1504.0
    hw = aspect * 0.5 # ~0.91755
    hh = 0.5

    # 4 Vertices in XZ plane (Y=0, facing +Y)
    # V0: Bottom-Left (-hw, 0, -hh)
    # V1: Bottom-Right (hw, 0, -hh)
    # V2: Top-Right (hw, 0, hh)
    # V3: Top-Left (-hw, 0, hh)
    positions = [
        (-hw, 0.0, -hh),
        ( hw, 0.0, -hh),
        ( hw, 0.0,  hh),
        (-hw, 0.0,  hh)
    ]

    # UV Coordinates
    uvs = [
        (0.0, 1.0),
        (1.0, 1.0),
        (1.0, 0.0),
        (0.0, 0.0)
    ]

    # Normals facing +Y
    norm_bytes = pack_normal(0.0, 1.0, 0.0)
    tang_bytes = pack_normal(1.0, 0.0, 0.0)
    col_white = bytes([255, 255, 255, 255])
    extra_bytes = bytes([0, 0, 0, 0])

    # Vertex struct: 32 bytes
    # pos (3 floats = 12B)
    # uv (2 half-floats = 4B)
    # normal (4B)
    # tangent (4B)
    # color (4B)
    # extra (4B)
    vert_bytes = bytearray()
    for i in range(4):
        x, y, z = positions[i]
        vert_bytes += struct.pack('<3f', x, y, z)
        u_h = float_to_half(uvs[i][0])
        v_h = float_to_half(uvs[i][1])
        vert_bytes += struct.pack('<2H', u_h, v_h)
        vert_bytes += norm_bytes
        vert_bytes += tang_bytes
        vert_bytes += col_white
        vert_bytes += extra_bytes

    # 4 triangles: 2 front-facing, 2 back-facing
    triangles = [
        (0, 1, 2), # Front 1
        (0, 2, 3), # Front 2
        (0, 2, 1), # Back 1
        (0, 3, 2)  # Back 2
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
    b0 += struct.pack('<i', 0)          # Name string idx 0: 'GazeRegionPanel'
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
    b1 += struct.pack('<i', 1)          # Name string idx 1: 'GazeRegionPanel:0'
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
    b1 += struct.pack('<Q', 0x0003b00007650408) # SSE vertex_flags: Pos+UV+Norm+Tang+Color
    b1 += struct.pack('<H', num_tris)   # num_triangles
    b1 += struct.pack('<H', num_verts)  # num_vertices
    b1 += struct.pack('<I', data_size)  # data_size
    b1 += tri_bytes
    b1 += vert_bytes
    b1 += struct.pack('<I', 0)          # particle_data_size = 0 (MANDATORY SSE FIELD!)

    # Block 2: BSEffectShaderProperty
    tex_path = r"textures\TrueGaze\GazeRegionPanel.dds"
    tex_bytes = tex_path.encode('ascii')
    b2 = bytearray()
    b2 += struct.pack('<i', -1)         # name: -1
    b2 += struct.pack('<I', 0)          # num_extra_data: 0
    b2 += struct.pack('<i', -1)         # controller: -1
    b2 += struct.pack('<I', 0x80000008) # shader flags 1: ZBuffer_Test
    b2 += struct.pack('<I', 0x00000021) # shader flags 2: Double_Sided | Glow_Map
    b2 += struct.pack('<2f', 0.0, 0.0)  # UV offset
    b2 += struct.pack('<2f', 1.0, 1.0)  # UV scale
    b2 += struct.pack('<I', len(tex_bytes)) # SizedString length (uint32)
    b2 += tex_bytes                     # SizedString ASCII bytes
    b2 += struct.pack('<B', 3)          # clamp mode: WRAP_S_WRAP_T
    b2 += struct.pack('<B', 0)          # lighting influence: 0 (unshaded/emissive)
    b2 += struct.pack('<B', 0)          # env map min LOD
    b2 += struct.pack('<B', 0)          # unused byte
    b2 += struct.pack('<4f', 1.0, 1.0, 0.0, 0.0) # falloff angles & opacity
    b2 += struct.pack('<4f', 1.0, 1.0, 1.0, 1.0) # base color RGBA
    b2 += struct.pack('<f', 1.0)        # base color scale
    b2 += struct.pack('<f', 100.0)      # soft falloff depth
    b2 += struct.pack('<I', 0)          # greyscale texture length: 0 (empty SizedString)

    # Block 3: NiAlphaProperty
    b3 = bytearray()
    b3 += struct.pack('<i', -1)         # name: -1
    b3 += struct.pack('<I', 0)          # num_extra_data: 0
    b3 += struct.pack('<i', -1)         # controller: -1
    b3 += struct.pack('<H', 0x100D)     # flags: SrcAlpha / InvSrcAlpha + test
    b3 += struct.pack('<B', 2)          # threshold: 2 (smooth alpha blend)

    blocks = [b0, b1, b2, b3]
    block_sizes = [len(b) for b in blocks]
    block_types = ['NiNode', 'BSTriShape', 'BSEffectShaderProperty', 'NiAlphaProperty']
    type_indices = [0, 1, 2, 3]
    strings = [
        'GazeRegionPanel',
        'GazeRegionPanel:0'
    ]

    # Header
    hdr = bytearray()
    hdr += b'Gamebryo File Format, Version 20.2.0.7\n'
    hdr += struct.pack('<I', 0x14020007) # version
    hdr += struct.pack('<B', 1)          # endian (little)
    hdr += struct.pack('<I', 12)         # user version
    hdr += struct.pack('<I', len(blocks))# num_blocks
    hdr += struct.pack('<I', 100)        # user version 2 (BSVER)
    hdr += struct.pack('<3B', 0, 0, 0)   # author, process, export lengths
    hdr += struct.pack('<H', len(block_types))
    for bt in block_types:
        hdr += struct.pack('<I', len(bt))
        hdr += bt.encode('ascii')
    for ti in type_indices:
        hdr += struct.pack('<H', ti)
    for bs in block_sizes:
        hdr += struct.pack('<I', bs)
    hdr += struct.pack('<I', len(strings))
    hdr += struct.pack('<I', max(len(s) for s in strings) if strings else 0)
    for s in strings:
        hdr += struct.pack('<I', len(s))
        hdr += s.encode('ascii')
    hdr += struct.pack('<I', 0)          # num_groups: 0

    # Footer
    footer = bytearray()
    footer += struct.pack('<I', 1)       # num_roots = 1
    footer += struct.pack('<i', 0)       # root 0 = Block 0

    full_nif = hdr + b''.join(blocks) + footer
    with open(output_path, 'wb') as f:
        f.write(full_nif)
    print(f"Generated clean SSE NIF: {output_path} ({len(full_nif)} bytes)")

if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else r"skyrim\meshes\TrueGaze\GazeRegionPanel.nif"
    generate_valid_panel_nif(dest)
