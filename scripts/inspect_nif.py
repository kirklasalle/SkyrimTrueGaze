"""Quick NIF header inspector for GazeBeam.nif"""
import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else r"d:\Projects\SkyrimTrueGaze\skyrim\meshes\TrueGaze\GazeBeam.nif"

with open(path, 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")

# Header line
header_end = data.index(b'\n') + 1
print(f"Header line: {data[:header_end-1].decode('ascii')}")

pos = header_end
version = struct.unpack_from('<I', data, pos)[0]
print(f"Version at offset {pos}: 0x{version:08X}")
pos += 4

endian = data[pos]
print(f"Endian byte at offset {pos}: {endian} (1=LE, 0=BE)")
pos += 1

user_ver = struct.unpack_from('<I', data, pos)[0]
print(f"User version: {user_ver}")
pos += 4

num_blocks = struct.unpack_from('<I', data, pos)[0]
print(f"Num blocks: {num_blocks}")
pos += 4

user_ver2 = struct.unpack_from('<I', data, pos)[0]
print(f"User version 2 (BS version): {user_ver2}")
pos += 4

# Author, process script, export script lengths
a_len = data[pos]; pos += 1
p_len = data[pos]; pos += 1
e_len = data[pos]; pos += 1
print(f"Author/Process/Export string lens: {a_len}/{p_len}/{e_len}")
pos += a_len + p_len + e_len

num_block_types = struct.unpack_from('<H', data, pos)[0]
print(f"Num block types: {num_block_types}")
pos += 2

for i in range(num_block_types):
    slen = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    name = data[pos:pos+slen].decode('ascii')
    pos += slen
    print(f"  Block type {i}: '{name}'")

# Type indices
for i in range(num_blocks):
    ti = struct.unpack_from('<H', data, pos)[0]
    pos += 2
    print(f"  Block {i} -> type index {ti}")

# Block sizes
for i in range(num_blocks):
    bs = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    print(f"  Block {i} size: {bs} bytes")

# Strings
num_strings = struct.unpack_from('<I', data, pos)[0]
pos += 4
max_string_len = struct.unpack_from('<I', data, pos)[0]
pos += 4
print(f"Num strings: {num_strings}, max len: {max_string_len}")
for i in range(num_strings):
    slen = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    s = data[pos:pos+slen].decode('ascii')
    pos += slen
    print(f"  String {i}: '{s}'")

num_groups = struct.unpack_from('<I', data, pos)[0]
pos += 4
print(f"Num groups: {num_groups}")
print(f"Block data starts at offset: {pos}")

# Check the BSEffectShaderProperty (Block 2)
# Need to skip past Block 0 and Block 1 first
# Read block sizes again to compute offsets
header_rewind = header_end + 4 + 1 + 4 + 4 + 4 + 3
header_rewind += sum(4 + len(t) for t in ['NiNode', 'BSTriShape', 'BSEffectShaderProperty', 'NiAlphaProperty'])
header_rewind += num_blocks * 2  # type indices
block_sizes_offset = header_rewind
block_sizes_list = []
for i in range(num_blocks):
    bs = struct.unpack_from('<I', data, block_sizes_offset + i * 4)[0]
    block_sizes_list.append(bs)

block_data_start = pos
for i in range(num_blocks):
    block_offset = block_data_start + sum(block_sizes_list[:i])
    print(f"\n--- Block {i} at offset {block_offset}, size {block_sizes_list[i]} ---")
    block_data = data[block_offset:block_offset + block_sizes_list[i]]
    print(f"  Hex dump (first 64 bytes): {block_data[:64].hex()}")

print("\nDone.")
