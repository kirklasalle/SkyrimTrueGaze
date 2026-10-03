"""Generate a tiny white radial-glow DDS for the GazeBeam effect shader.

An SSE BSEffectShaderProperty with NO source texture renders black under
additive (One/One) blending — black added to the scene = invisible. Every
vanilla effect NIF carries a texture. This generates a small white radial
gradient (64x64, BC1/DXT1 with mipmaps) that the shader tints with its
base colour (TrueGaze gold), producing a visible additive laser glow.
"""

import struct
import math
import os


def _dxt1_block(color0, color1, bitmap_4x4):
    """Encode one 4x4 block of 2-bit indices into an 8-byte DXT1 block."""
    # bitmap_4x4: 4 rows of 4 values (0..3)
    c0 = (color0[0] >> 3) << 11 | (color0[1] >> 2) << 5 | (color0[2] >> 3)
    c1 = (color1[0] >> 3) << 11 | (color1[1] >> 2) << 5 | (color1[2] >> 3)
    if c0 < c1:
        c0, c1 = c1, c0  # DXT1: color0 must be >= color1 for 4-color mode
    indices = 0
    shift = 0
    for row in range(4):
        for col in range(4):
            indices |= (bitmap_4x4[row][col] & 3) << shift
            shift += 2
    return struct.pack('<HHI', c0, c1, indices)


def generate_beam_glow_dds(output_path, size=64):
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)

    # White radial gradient: bright center, dark edge.
    # The effect shader multiplies this by its gold base colour.
    pixels = [[0] * size for _ in range(size)]
    center = (size - 1) / 2.0
    max_dist = math.hypot(center, center)
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - center, y - center) / max_dist
            # smooth falloff: 255 at center -> 0 at edge
            v = int(max(0.0, 1.0 - d) ** 2 * 255.0)
            pixels[y][x] = v

    # Encode as DXT1 (BC1) with mipmaps.
    # For a radial gradient, each 4x4 block: two endpoint greys + 2-bit indices.
    def block_at(bx, by):
        vals = []
        for row in range(4):
            for col in range(4):
                vals.append(pixels[by * 4 + row][bx * 4 + col])
        lo, hi = min(vals), max(vals)
        if hi - lo < 8:
            # flat block: single colour
            c = (hi, hi, hi)
            bm = [[0] * 4 for _ in range(4)]
            return _dxt1_block(c, c, bm)
        c0 = (hi, hi, hi)
        c1 = (lo, lo, lo)
        bm = []
        for row in range(4):
            bmrow = []
            for col in range(4):
                v = pixels[by * 4 + row][bx * 4 + col]
                # nearest of c0, c0c1avg, c1, black
                d0 = abs(v - hi)
                d1 = abs(v - lo)
                mid = (hi + lo) // 2
                dm = abs(v - mid)
                best = min(d0, dm, d1)
                if best == d0:
                    bmrow.append(0)
                elif best == dm:
                    bmrow.append(2)
                else:
                    bmrow.append(1)
            bm.append(bmrow)
        return _dxt1_block(c0, c1, bm)

    # Main mip + 6 smaller mips (64,32,16,8,4,1... we do 64->4)
    mips = []
    mip_size = size
    while mip_size >= 4:
        blocks = bytearray()
        for by in range(mip_size // 4):
            for bx in range(mip_size // 4):
                # sample from the full-res gradient, scaled
                scale = size // mip_size
                sub = [[pixels[min(size - 1, (by * 4 + r) * scale)][min(size - 1, (bx * 4 + c) * scale)]
                        for c in range(4)] for r in range(4)]
                vals = [v for row in sub for v in row]
                lo, hi = min(vals), max(vals)
                if hi - lo < 8:
                    c = (hi, hi, hi)
                    bm = [[0] * 4 for _ in range(4)]
                    blocks += _dxt1_block(c, c, bm)
                else:
                    c0 = (hi, hi, hi)
                    c1 = (lo, lo, lo)
                    bm = []
                    for r in range(4):
                        bmrow = []
                        for c in range(4):
                            v = sub[r][c]
                            d0 = abs(v - hi); d1 = abs(v - lo); dm = abs(v - (hi + lo) // 2)
                            if min(d0, dm, d1) == d0: bmrow.append(0)
                            elif min(d0, dm, d1) == dm: bmrow.append(2)
                            else: bmrow.append(1)
                        bm.append(bmrow)
                    blocks += _dxt1_block(c0, c1, bm)
        mips.append((mip_size, bytes(blocks)))
        mip_size //= 2

    # DDS file: header (124B) + DX10 header (20B) + mips
    # Using plain DXT1 (no DX10 header needed): fourcc 'DXT1'
    hdr = b'DDS '
    hdr += struct.pack('<7I', 124, 0x1007, size, size, 0, 0, 0)  # size, flags(w|mips), h, w, pitch?, depth, mips
    # pitch = max(1, width//4) * 8
    hdr = b'DDS '
    flags = 0x1007  # CAPS | HEIGHT | WIDTH | PIXELFORMAT
    pitch = max(1, size // 4) * 8
    hdr += struct.pack('<I', 124)          # size of struct
    hdr += struct.pack('<I', flags)
    hdr += struct.pack('<I', size)         # height
    hdr += struct.pack('<I', size)         # width
    hdr += struct.pack('<I', pitch)        # pitch
    hdr += struct.pack('<I', 0)            # depth
    hdr += struct.pack('<I', len(mips))    # mip count
    hdr += b'\x00' * 44                    # reserved
    # pixel format: size 32, flags DDPF_FOURCC, fourcc DXT1
    hdr += struct.pack('<I', 32)
    hdr += struct.pack('<I', 0x4)          # DDPF_FOURCC
    hdr += b'DXT1'
    hdr += struct.pack('<5I', 0, 0, 0, 0, 0)
    # caps: DDSCAPS_TEXTURE | DDSCAPS_MIPMAP
    hdr += struct.pack('<2I', 0x401000, 0)
    hdr += b'\x00' * 12

    with open(output_path, 'wb') as f:
        f.write(hdr)
        for _, data in mips:
            f.write(data)

    print(f"Generated {output_path} ({os.path.getsize(output_path)} bytes, "
          f"{size}x{size} DXT1, {len(mips)} mips)")


if __name__ == "__main__":
    import sys
    dest = sys.argv[1] if len(sys.argv) > 1 else r"skyrim\textures\TrueGaze\GazeBeamGlow.dds"
    generate_beam_glow_dds(dest)
    build_dest = r"build\TrueGaze\textures\TrueGaze\GazeBeamGlow.dds"
    generate_beam_glow_dds(build_dest)
