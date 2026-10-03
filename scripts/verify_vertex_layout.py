"""Independent byte-level verifier for the TrueGaze beam & panel NIFs.

Decodes the BSTriShape vertex buffer by hand and asserts:
  - VertexDesc flags/size/offsets match the canonical SSE layout
  - Positions are 16 bytes (xyzw) with w == 0
  - UV/normal/colour attributes sit at their declared offsets
  - Alpha property blend modes are the intended ones
"""
import struct
import sys


def parse(path, expect_desc, expect_alpha_flags, expect_flags2, expect_tex=None):
    data = open(path, 'rb').read()
    he = data.index(b'\n') + 1
    pos = he + 4 + 1 + 4 + 4 + 4 + 3
    n_types = struct.unpack_from('<H', data, pos)[0]; pos += 2
    for _ in range(n_types):
        sl = struct.unpack_from('<I', data, pos)[0]; pos += 4 + sl
    n_blocks = struct.unpack_from('<I', data, he + 4 + 1 + 4)[0]
    pos += n_blocks * 2
    sizes = [struct.unpack_from('<I', data, pos + 4 * i)[0] for i in range(n_blocks)]
    pos += 4 * n_blocks
    n_str = struct.unpack_from('<I', data, pos)[0]; pos += 4 + 4
    for _ in range(n_str):
        sl = struct.unpack_from('<I', data, pos)[0]; pos += 4 + sl
    pos += 4
    b1 = data[pos + sizes[0]: pos + sizes[0] + sizes[1]]

    off = 4 + 4 + 4 + 4 + 36 + 12 + 4 + 4 + 12 + 4 + 4 + 4 + 4
    desc = struct.unpack_from('<Q', b1, off)[0]
    ntris, nverts = struct.unpack_from('<2H', b1, off + 8)
    dsize = struct.unpack_from('<I', b1, off + 12)[0]
    vsize = (desc & 0xF) * 4
    flags = desc >> 44

    ok = True
    def check(name, cond, detail=""):
        nonlocal ok
        status = "PASS" if cond else "FAIL"
        if not cond:
            ok = False
        print(f"  [{status}] {name} {detail}")

    print(f"{path}:")
    check("vertexDesc", desc == expect_desc, f"got 0x{desc:016X} want 0x{expect_desc:016X}")
    check("vertexSize", vsize in (28, 32), f"{vsize}B")
    check("dataSize", dsize == 6 * ntris + vsize * nverts,
          f"{dsize} == {6 * ntris}+{vsize * nverts}")

    tri_end = 6 * ntris
    uv_off = (desc >> (4 * 1 + 2)) & 0x3C
    nrm_off = (desc >> (4 * 3 + 2)) & 0x3C
    col_off = (desc >> (4 * 5 + 2)) & 0x3C
    tang_off = (desc >> (4 * 4 + 2)) & 0x3C

    for vi in range(nverts):
        vo = off + 16 + tri_end + vi * vsize
        p = struct.unpack_from('<4f', b1, vo)
        if abs(p[3]) > 1e-6:
            check(f"v{vi}.w==0", False, f"w={p[3]}")
            break
    else:
        check("positions 16B xyzw (w=0)", True)

    # sample first vertex attributes
    vo = off + 16 + tri_end
    uv = struct.unpack_from('<2H', b1, vo + uv_off)
    uvf = (struct.unpack('<e', struct.pack('<H', uv[0]))[0],
           struct.unpack('<e', struct.pack('<H', uv[1]))[0])
    check("uv at offset", 0.0 <= uvf[0] <= 1.0 and 0.0 <= uvf[1] <= 1.0, f"uv=({uvf[0]:.2f},{uvf[1]:.2f})")
    col = b1[vo + col_off: vo + col_off + 4]
    check("colour alpha=255", col[3] == 255, f"BGRA={list(col)}")
    if vsize == 32:
        tang = b1[vo + tang_off: vo + tang_off + 4]
        check("tangent present", tang[0] != 0, f"{list(tang)}")

    # shader property block (block 2)
    b2 = data[pos + sizes[0] + sizes[1]: pos + sizes[0] + sizes[1] + sizes[2]]
    sf1 = struct.unpack_from('<I', b2, 12)[0]
    sf2 = struct.unpack_from('<I', b2, 16)[0]
    check("flags2", sf2 == expect_flags2, f"0x{sf2:X} want 0x{expect_flags2:X}")
    check("flags1 ZTest", sf1 & 0x80000000, f"0x{sf1:X}")

    # alpha property (block 3)
    b3 = data[pos + sizes[0] + sizes[1] + sizes[2]: pos + sizes[0] + sizes[1] + sizes[2] + sizes[3]]
    alpha_flags = struct.unpack_from('<H', b3, 12)[0]
    check("alphaFlags", alpha_flags == expect_alpha_flags,
          f"0x{alpha_flags:04X} want 0x{expect_alpha_flags:04X}")

    if expect_tex:
        # NiObjectNET(12) + flags1(4) + flags2(4) + uvOffset(8) + uvScale(8) = 36
        tex_len = struct.unpack_from('<I', b2, 36)[0]
        tex = b2[40:40 + tex_len].decode('ascii')
        check("texture path", tex == expect_tex, tex)

    return ok


if __name__ == "__main__":
    beam_ok = parse(r"skyrim\meshes\TrueGaze\GazeBeam.nif",
                    expect_desc=0x002B000006050407,
                    expect_alpha_flags=0x0001,
                    expect_flags2=0x00000030)
    panel_ok = parse(r"skyrim\meshes\TrueGaze\GazeRegionPanel.nif",
                     expect_desc=0x0003B00007650408,
                     expect_alpha_flags=0x00A9,
                     expect_flags2=0x00000030,
                     expect_tex=r"textures\TrueGaze\GazeRegionPanel.dds")
    print()
    print("ALL PASS" if (beam_ok and panel_ok) else "FAILURES PRESENT")
    sys.exit(0 if (beam_ok and panel_ok) else 1)
