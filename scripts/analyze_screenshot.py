"""Analyze TrueGaze screenshots for beam pixels.

Scans for TrueGaze gold (201,168,106) and region-colour pixels, clusters them,
and reports cluster bounding boxes / centroids / orientation — reconstructing
what the beams actually rendered as, without needing eyes.
"""
import sys
import numpy as np
from PIL import Image


def analyze(path):
    im = Image.open(path).convert("RGB")
    arr = np.array(im, dtype=np.int16)
    h, w, _ = arr.shape
    print(f"{path}")
    print(f"  size: {w}x{h}")

    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]

    # TrueGaze gold: (201,168,106) — allow tolerance for lighting/additive blend
    gold = (np.abs(r - 201) < 45) & (np.abs(g - 168) < 45) & (np.abs(b - 106) < 45)
    # Additive-blended gold on bright bg goes toward white-yellow; on dark bg stays gold
    goldish = gold & (r > g) & (g > b) & (r > 140) & (b < 180)
    count = int(goldish.sum())
    print(f"  gold-ish pixels: {count} ({100.0 * count / (w * h):.3f}% of frame)")

    if count < 20:
        print("  -> NO VISIBLE BEAM (no gold clusters)")
        return

    ys, xs = np.nonzero(goldish)

    # Simple grid clustering: divide into 8x8 cells, report occupied cells
    gy = (ys * 8 // h)
    gx = (xs * 8 // w)
    cells = {}
    for cy, cx in zip(gy, gx):
        cells.setdefault((cy, cx), 0)
        cells[(cy, cx)] += 1

    print(f"  occupied 8x8 cells: {len(cells)}")
    for (cy, cx), n in sorted(cells.items(), key=lambda kv: -kv[1])[:12]:
        print(f"    cell row={cy} col={cx}: {n} px")

    # Overall bounding box of gold pixels
    print(f"  gold bbox: x[{xs.min()}..{xs.max()}] y[{ys.min()}..{ys.max()}]")
    print(f"  centroid: ({xs.mean():.0f}, {ys.mean():.0f})")

    # Largest connected-ish cluster via flood fill on a downscaled mask
    small = goldish[::4, ::4]
    sh, sw = small.shape
    visited = np.zeros_like(small, dtype=bool)
    best = []
    import collections
    for sy in range(sh):
        for sx in range(sw):
            if small[sy, sx] and not visited[sy, sx]:
                q = collections.deque([(sy, sx)])
                visited[sy, sx] = True
                pts = []
                while q:
                    y, x = q.popleft()
                    pts.append((y, x))
                    for dy, dx in ((1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)):
                        ny, nx = y+dy, x+dx
                        if 0 <= ny < sh and 0 <= nx < sw and small[ny, nx] and not visited[ny, nx]:
                            visited[ny, nx] = True
                            q.append((ny, nx))
                best.append(pts)
    best.sort(key=len, reverse=True)
    print(f"  clusters found: {len(best)} (downscaled 4x)")
    for i, pts in enumerate(best[:6]):
        ys2 = np.array([p[0] for p in pts]) * 4
        xs2 = np.array([p[1] for p in pts]) * 4
        # orientation via PCA on the cluster
        pts_f = np.stack([xs2 - xs2.mean(), ys2 - ys2.mean()])
        cov = np.cov(pts_f)
        evals, evecs = np.linalg.eigh(cov)
        major = evecs[:, -1]
        elongation = evals[-1] / max(evals[0], 1e-6)
        angle = np.degrees(np.arctan2(major[1], major[0]))
        print(f"    cluster {i}: {len(pts)*16} px  bbox x[{xs2.min()}..{xs2.max()}] "
              f"y[{ys2.min()}..{ys2.max()}]  elongation={elongation:.1f} angle={angle:.0f}deg")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        analyze(p)
        print()
