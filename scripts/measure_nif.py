"""Measure a vanilla SSE NIF's BSTriShape geometry: bounds, aspect, flatness.

Uses the EMPIRICAL SSE layout verified against marker_arrow.nif:
  NiObjectNET: name(i32) nextra(u32) [extraRefs] ctrl(i32)
  NiAVObject:  flags(u32) pad(12) rot(9f)@+28 trans(3f)@+64 scale(f)@+76
               coll(i32)@+80 boundC(3f)@+84 boundR(f)@+96
  BSTriShape:  skin(i32)@+100 shader(i32)@+104 alpha(i32)@+108
               desc(u64)@+112 ntris(u16)@+120 nverts(u16)@+122 dsize(u32)@+124
               [triData]@+128 [vertData] particleSize(u32)

Vertex layout comes from the desc: flags bits (>>44), size (&0xF)*4,
attribute offsets ((desc >> (4*attr+2)) & 0x3C). Positions are 3 floats
at offset 0 in vanilla SSE meshes (verified: marker_arrow desc 0x0002900005040006
has 12B positions + half-UV + normal + color = 24B).
"""
import struct
import sys


def parse_header(data):
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
    idxs = [struct.unpack_from('<H', data, pos + 2 * i)[0] for i in range(nb)]
    pos += 2 * nb
    sizes = [struct.unpack_from('<I', data, pos + 4 * i)[0] for i in range(nb)]
    pos += 4 * nb
    nstr, = struct.unpack_from('<I', data, pos); pos += 8
    strings = []
    for i in range(nstr):
        sl, = struct.unpack_from('<I', data, pos); pos += 4
        strings.append(data[pos:pos + sl].decode('ascii')); pos += sl
    pos += 4
    return nb, types, idxs, sizes, strings, pos


def measure(path):
    data = open(path, 'rb').read()
    nb, types, idxs, sizes, strings, bd = parse_header(data)
    print(f"{path.split(chr(92))[-1]}  ({len(data)}B, {nb} blocks)")

    off = bd
    best = None
    for bi in range(nb):
        t = types[idxs[bi]]
        sz = sizes[bi]
        b = data[off:off + sz]
        off += sz
        if t != 'BSTriShape' or sz < 140:
            continue
        desc, = struct.unpack_from('<Q', b, 100)
        ntris, nverts = struct.unpack_from('<2H', b, 108)
        dsize, = struct.unpack_from('<I', b, 112)
        vsize = (desc & 0xF) * 4
        vflags = desc >> 44
        if nverts == 0 or ntris == 0 or vsize == 0:
            continue
        if 116 + dsize > sz + 4:
            continue
        # VERIFIED on marker_arrow.nif: vertices come FIRST (116 + vsize*nverts),
        # then triangles, then particle u32. Positions are 12B floats at offset 0.
        vert_bytes = vsize * nverts
        if 116 + vert_bytes + 6 * ntris > sz:
            continue
        xs, ys, zs = [], [], []
        good = True
        for vi in range(nverts):
            vo = 116 + vi * vsize
            x, y, z = struct.unpack_from('<3f', b, vo)
            if abs(x) > 1e6 or abs(y) > 1e6 or abs(z) > 1e6:
                good = False
                break
            xs.append(x); ys.append(y); zs.append(z)
        # validate triangle indices
        tris_ok = True
        for t in range(ntris):
            i0, i1, i2 = struct.unpack_from('<3H', b, 116 + vert_bytes + 6 * t)
            if max(i0, i1, i2) >= nverts:
                tris_ok = False
                break
        if not good or not xs or not tris_ok:
            continue
        xr = (min(xs), max(xs)); yr = (min(ys), max(ys)); zr = (min(zs), max(zs))
        dx, dy, dz = xr[1]-xr[0], yr[1]-yr[0], zr[1]-zr[0]
        # flatness: the thinnest axis
        dims = sorted([('X', dx), ('Y', dy), ('Z', dz)], key=lambda d: d[1])
        flat_axis = dims[0][0]
        # aspect of the two thick axes
        a, bb = dims[1][1], dims[2][1]
        aspect = max(a, bb) / max(min(a, bb), 1e-9)
        print(f"  BSTriShape#{bi}: {nverts}v {ntris}t vsize={vsize} flags=0x{vflags:X}")
        print(f"    X {xr[0]:.1f}..{xr[1]:.1f} ({dx:.1f})  Y {yr[0]:.1f}..{yr[1]:.1f} ({dy:.1f})  Z {zr[0]:.1f}..{zr[1]:.1f} ({dz:.1f})")
        print(f"    flat along {flat_axis}; face dims {min(a,bb):.1f} x {max(a,bb):.1f}; aspect {aspect:.2f}")
        if best is None or nverts > best[0]:
            best = (nverts, flat_axis, aspect, bi)
    if best:
        print(f"  -> best: block#{best[3]}, flat along {best[1]}, aspect {best[2]:.2f}")
    else:
        print("  -> no decodable BSTriShape")
    print()


if __name__ == "__main__":
    for p in sys.argv[1:]:
        try:
            measure(p)
        except Exception as e:
            print(f"{p}: ERROR {e}")
