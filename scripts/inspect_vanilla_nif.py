"""Inspect a vanilla SSE NIF: block walk + BSTriShape vertex bounds.

Uses the EMPIRICAL SSE NiAVObject layout discovered from marker_arrow.nif:
  name(i32) nextra(u32) ctrl(i32) flags(u32) pad(12) rot(9f) trans(3f) scale(f)
  coll(i32) bound(3f+f) skin(i32) shader(i32) alpha(i32) desc(u64)
  ntris(u16) nverts(u16) dsize(u32) [tris][verts] particleSize(u32)
"""
import struct
import sys


def inspect(path):
    data = open(path, 'rb').read()
    he = data.index(b'\n') + 1
    nb = struct.unpack_from('<I', data, he + 4 + 1 + 4)[0]
    pos = he + 4 + 1 + 4 + 4 + 4

    def read_ss(p):
        l = data[p]; p += 1
        return data[p:p + l], p + l
    a, pos = read_ss(pos); pr, pos = read_ss(pos); ex, pos = read_ss(pos)
    nbt, = struct.unpack_from('<H', data, pos); pos += 2
    types = []
    for i in range(nbt):
        sl, = struct.unpack_from('<I', data, pos); pos += 4
        types.append(data[pos:pos + sl].decode('ascii')); pos += sl
    idxs = [struct.unpack_from('<H', data, pos + 2 * i)[0] for i in range(nb)]; pos += 2 * nb
    sizes = [struct.unpack_from('<I', data, pos + 4 * i)[0] for i in range(nb)]; pos += 4 * nb
    nstr, = struct.unpack_from('<I', data, pos); pos += 4 + 4
    strings = []
    for i in range(nstr):
        sl, = struct.unpack_from('<I', data, pos); pos += 4
        strings.append(data[pos:pos + sl].decode('ascii')); pos += sl
    pos += 4
    bd = pos

    print(f"{path}")
    print(f"  blocks={nb} types={types} strings={strings}")

    off = bd
    for bi in range(nb):
        sz = sizes[bi]
        tname = types[idxs[bi]]
        b = data[off:off + sz]
        if tname == 'BSTriShape' and sz > 140:
            # empirical layout
            trans = struct.unpack_from('<3f', b, 52)
            scale, = struct.unpack_from('<f', b, 64)
            bc = struct.unpack_from('<3f', b, 84)
            br, = struct.unpack_from('<f', b, 96)
            desc, = struct.unpack_from('<Q', b, 112)
            ntris, nverts = struct.unpack_from('<2H', b, 120)
            dsize, = struct.unpack_from('<I', b, 124)
            vsize = (desc & 0xF) * 4
            vflags = desc >> 44
            print(f"  BSTriShape block{bi}: trans={trans} scale={scale}")
            print(f"    boundC={bc} boundR={br:.1f}")
            print(f"    desc=0x{desc:016X} vsize={vsize} vflags=0x{vflags:X} "
                  f"ntris={ntris} nverts={nverts} dsize={dsize}")
            # decode vertices: data at 128 (after dsize), tris first
            tri_end = 128 + 6 * ntris
            xs, ys, zs = [], [], []
            ok = True
            for vi in range(nverts):
                vo = tri_end + vi * vsize
                if vo + vsize > sz:
                    ok = False
                    break
                # try 12-byte positions first; validate plausibility vs bound
                x, y, z = struct.unpack_from('<3f', b, vo)
                if abs(x) > br * 1.5 or abs(y) > br * 1.5 or abs(z) > br * 1.5:
                    ok = False
                    break
                xs.append(x); ys.append(y); zs.append(z)
            if ok and xs:
                print(f"    verts (12B pos): X {min(xs):.1f}..{max(xs):.1f}  "
                      f"Y {min(ys):.1f}..{max(ys):.1f}  Z {min(zs):.1f}..{max(zs):.1f}")
            else:
                print("    12B-position decode failed bounds check; trying 16B")
                xs, ys, zs = [], [], []
                ok = True
                for vi in range(nverts):
                    vo = tri_end + vi * vsize
                    if vo + vsize > sz:
                        ok = False
                        break
                    x, y, z, w = struct.unpack_from('<4f', b, vo)
                    if abs(x) > br * 1.5 or abs(y) > br * 1.5 or abs(z) > br * 1.5:
                        ok = False
                        break
                    xs.append(x); ys.append(y); zs.append(z)
                if ok and xs:
                    print(f"    verts (16B pos): X {min(xs):.1f}..{max(xs):.1f}  "
                          f"Y {min(ys):.1f}..{max(ys):.1f}  Z {min(zs):.1f}..{max(zs):.1f}")
                else:
                    print("    both decodes failed — layout still unknown")
        off += sz
    print()


if __name__ == "__main__":
    for p in sys.argv[1:]:
        inspect(p)
