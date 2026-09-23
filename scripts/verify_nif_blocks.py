"""Verify GazeBeam.nif block content integrity."""
import struct

path = r"d:\Projects\SkyrimTrueGaze\skyrim\meshes\TrueGaze\GazeBeam.nif"
with open(path, 'rb') as f:
    data = f.read()

# Parse header to get block sizes dynamically
header_end = data.index(b'\n') + 1
pos = header_end
pos += 4  # version
pos += 1  # endian
pos += 4  # user version
num_blocks = struct.unpack_from('<I', data, pos)[0]
pos += 4
pos += 4  # user version 2
pos += 3  # author/process/export lengths (all 0)
num_block_types = struct.unpack_from('<H', data, pos)[0]
pos += 2
for i in range(num_block_types):
    slen = struct.unpack_from('<I', data, pos)[0]
    pos += 4 + slen
pos += num_blocks * 2  # type indices
block_sizes = []
for i in range(num_blocks):
    block_sizes.append(struct.unpack_from('<I', data, pos)[0])
    pos += 4
# strings
num_strings = struct.unpack_from('<I', data, pos)[0]
pos += 4
pos += 4  # max string len
for i in range(num_strings):
    slen = struct.unpack_from('<I', data, pos)[0]
    pos += 4 + slen
pos += 4  # num groups
block_start = pos

print(f"Blocks: {num_blocks}, sizes: {block_sizes}, data starts at {block_start}")
print(f"Total file: {len(data)} bytes (expected {block_start + sum(block_sizes) + 8})")

# Block 0: NiNode
b0 = data[block_start:block_start + block_sizes[0]]
pos = 0
name_idx = struct.unpack_from('<i', b0, pos)[0]; pos += 4
print(f"\nBlock 0 (NiNode): name_idx={name_idx}")
num_extra = struct.unpack_from('<I', b0, pos)[0]; pos += 4
ctrl_ref = struct.unpack_from('<i', b0, pos)[0]; pos += 4
flags = struct.unpack_from('<I', b0, pos)[0]; pos += 4
rot = struct.unpack_from('<9f', b0, pos); pos += 36
trans = struct.unpack_from('<3f', b0, pos); pos += 12
scale = struct.unpack_from('<f', b0, pos)[0]; pos += 4
coll_ref = struct.unpack_from('<i', b0, pos)[0]; pos += 4
num_children = struct.unpack_from('<I', b0, pos)[0]; pos += 4
children = [struct.unpack_from('<i', b0, pos + i*4)[0] for i in range(num_children)]
pos += num_children * 4
num_effects = struct.unpack_from('<I', b0, pos)[0]; pos += 4
print(f"  extra={num_extra} ctrl={ctrl_ref} flags=0x{flags:08X} scale={scale} children={children} effects={num_effects}")
print(f"  Block 0 consumed {pos}/{block_sizes[0]} bytes")

# Block 2: BSEffectShaderProperty (new SSE format)
b2_start = block_start + block_sizes[0] + block_sizes[1]
b2 = data[b2_start:b2_start + block_sizes[2]]
pos = 0
print(f"\nBlock 2 (BSEffectShaderProperty, {block_sizes[2]} bytes):")
name_idx = struct.unpack_from('<i', b2, pos)[0]; pos += 4
print(f"  name_idx={name_idx}")
num_extra = struct.unpack_from('<I', b2, pos)[0]; pos += 4
print(f"  num_extra_data={num_extra}")
ctrl = struct.unpack_from('<i', b2, pos)[0]; pos += 4
print(f"  controller_ref={ctrl}")
sf1 = struct.unpack_from('<I', b2, pos)[0]; pos += 4
print(f"  shader_flags_1=0x{sf1:08X}")
sf2 = struct.unpack_from('<I', b2, pos)[0]; pos += 4
print(f"  shader_flags_2=0x{sf2:08X}")
uv_offset = struct.unpack_from('<2f', b2, pos); pos += 8
print(f"  uv_offset={uv_offset}")
uv_scale = struct.unpack_from('<2f', b2, pos); pos += 8
print(f"  uv_scale={uv_scale}")
src_tex_len = struct.unpack_from('<I', b2, pos)[0]; pos += 4
print(f"  source_texture_len={src_tex_len}")
pos += src_tex_len
clamp_mode = b2[pos]; pos += 1
lighting_influence = b2[pos]; pos += 1
env_map_lod = b2[pos]; pos += 1
unused = b2[pos]; pos += 1
print(f"  clamp={clamp_mode} lighting_influence={lighting_influence} env_lod={env_map_lod} unused={unused}")
falloff_start_angle = struct.unpack_from('<f', b2, pos)[0]; pos += 4
falloff_stop_angle = struct.unpack_from('<f', b2, pos)[0]; pos += 4
falloff_start_op = struct.unpack_from('<f', b2, pos)[0]; pos += 4
falloff_stop_op = struct.unpack_from('<f', b2, pos)[0]; pos += 4
print(f"  falloff: angles=({falloff_start_angle},{falloff_stop_angle}) opacity=({falloff_start_op},{falloff_stop_op})")
base_color = struct.unpack_from('<4f', b2, pos); pos += 16
print(f"  base_color_rgba={base_color}")
base_scale = struct.unpack_from('<f', b2, pos)[0]; pos += 4
print(f"  base_color_scale={base_scale}")
soft_depth = struct.unpack_from('<f', b2, pos)[0]; pos += 4
print(f"  soft_falloff_depth={soft_depth}")
grey_len = struct.unpack_from('<I', b2, pos)[0]; pos += 4
print(f"  greyscale_texture_len={grey_len}")
print(f"  Block 2 consumed {pos}/{block_sizes[2]} bytes")

# Block 3: NiAlphaProperty
b3_start = b2_start + block_sizes[2]
b3 = data[b3_start:b3_start + block_sizes[3]]
pos = 0
print(f"\nBlock 3 (NiAlphaProperty, {block_sizes[3]} bytes):")
name_idx = struct.unpack_from('<i', b3, pos)[0]; pos += 4
num_extra = struct.unpack_from('<I', b3, pos)[0]; pos += 4
ctrl = struct.unpack_from('<i', b3, pos)[0]; pos += 4
alpha_flags = struct.unpack_from('<H', b3, pos)[0]; pos += 2
threshold = b3[pos]; pos += 1
print(f"  name_idx={name_idx} extra={num_extra} ctrl={ctrl} alpha_flags=0x{alpha_flags:04X} threshold={threshold}")
print(f"  Block 3 consumed {pos}/{block_sizes[3]} bytes")

print("\nDone.")
