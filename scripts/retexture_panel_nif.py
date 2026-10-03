"""Retexture a vanilla SSE NIF for the HCEP panel (Kirk directive 2026-10-01).

Takes Bethesda's proven flat glow quad (fxglowflatrndmid.nif — a 512x512-unit
XZ-plane quad, normal +Y, BSEffectShaderProperty + NiAlphaProperty) and swaps
its source texture to Kirk's chroma-keyed HCEP-02 diagram DDS.

The texture is stored as a NIF SizedString: u32 length + ASCII bytes.
Both paths must keep the block size consistent — we patch the length prefix
and the string, then fix the BSEffectShaderProperty block's size entry and
the header's block-size table.

Usage: python retexture_panel_nif.py <in.nif> <out.nif>
"""
import struct
import sys

OLD_TEX = b'textures\\effects\\GlowSoft01.dds'
NEW_TEX = b'textures\\TrueGaze\\GazeRegionPanel.dds'


def retexture(src, dst):
    data = bytearray(open(src, 'rb').read())

    idx = data.find(OLD_TEX)
    assert idx != -1, "old texture string not found"
    slen, = struct.unpack_from('<I', data, idx - 4)
    assert slen == len(OLD_TEX), f"length prefix mismatch: {slen} vs {len(OLD_TEX)}"

    delta = len(NEW_TEX) - len(OLD_TEX)

    # Patch the SizedString: new length + new bytes
    struct.pack_into('<I', data, idx - 4, len(NEW_TEX))
    data[idx:idx + len(OLD_TEX)] = NEW_TEX

    if delta == 0:
        open(dst, 'wb').write(bytes(data))
        print(f"wrote {dst} (same size, texture swapped)")
        return

    # The string lives inside a BSEffectShaderProperty block. We must find which
    # block contains offset idx and grow its size in the header's size table.
    he = data.index(b'\n') + 1
    nb = struct.unpack_from('<I', data, he + 4 + 1 + 4)[0]
    pos = he + 4 + 1 + 4 + 4 + 4

    def read_ss(p):
        l = data[p]; p += 1
        return bytes(data[p:p + l]), p + l
    _, pos = read_ss(pos); _, pos = read_ss(pos); _, pos = read_ss(pos)
    nbt, = struct.unpack_from('<H', data, pos); pos += 2
    for i in range(nbt):
        sl, = struct.unpack_from('<I', data, pos); pos += 4 + sl
    pos += 2 * nb
    sizes_off = pos
    sizes = [struct.unpack_from('<I', data, sizes_off + 4 * i)[0] for i in range(nb)]
    pos += 4 * nb
    nstr, = struct.unpack_from('<I', data, pos); pos += 8
    for i in range(nstr):
        sl, = struct.unpack_from('<I', data, pos); pos += 4 + sl
    pos += 4
    bd = pos

    # find the block containing idx
    off = bd
    target = None
    for bi in range(nb):
        if off <= idx < off + sizes[bi]:
            target = bi
            break
        off += sizes[bi]
    assert target is not None, "texture string not inside any block?!"

    # grow that block's size
    struct.pack_into('<I', data, sizes_off + 4 * target, sizes[target] + delta)
    print(f"block {target} size: {sizes[target]} -> {sizes[target] + delta}")

    open(dst, 'wb').write(bytes(data))
    print(f"wrote {dst} ({len(data)} bytes, texture -> {NEW_TEX.decode()}")


if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else \
        r"scratch\panel_extract\meshes\effects\ambient\fxglowflatrndmid.nif"
    dst = sys.argv[2] if len(sys.argv) > 2 else \
        r"skyrim\meshes\TrueGaze\GazeRegionPanel.nif"
    retexture(src, dst)
