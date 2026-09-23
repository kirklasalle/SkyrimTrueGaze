import os
import sys
import numpy as np
from PIL import Image

def generate_hcep_texture():
    src_path = r"docs\images\hcep-02_enhanced-diagram_keyed-01.jfif"
    if not os.path.exists(src_path):
        print(f"Error: Source image not found at {src_path}")
        return False

    print(f"Loading source image: {src_path}")
    im = Image.open(src_path).convert("RGBA")
    arr = np.array(im, dtype=np.float32)

    # Calculate distance from pure white (255, 255, 255)
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    dist = np.sqrt((255.0 - r)**2 + (255.0 - g)**2 + (255.0 - b)**2)

    # Chroma-key thresholds:
    # Low threshold: pixels with dist <= 12.0 are 100% transparent canvas
    # High threshold: pixels with dist >= 35.0 are 100% opaque content
    # Smoothstep interpolation between 12.0 and 35.0
    alpha_factor = np.clip((dist - 12.0) / 23.0, 0.0, 1.0)
    alpha_smooth = alpha_factor * alpha_factor * (3.0 - 2.0 * alpha_factor)

    # Anti-fringe: For transitional edge pixels that blended with the white canvas,
    # compensate RGB so white doesn't halo around dark text/lines
    # Where alpha is low (< 0.9), suppress the white component towards the edge color
    edge_mask = (alpha_smooth > 0.0) & (alpha_smooth < 0.95)
    for c in range(3):
        # Desaturate white bias on soft edges
        arr[edge_mask, c] = np.clip(arr[edge_mask, c] * alpha_smooth[edge_mask] + 
                                    (1.0 - alpha_smooth[edge_mask]) * 20.0, 0.0, 255.0)

    arr[:, :, 3] = alpha_smooth * 255.0

    keyed_im = Image.fromarray(arr.astype(np.uint8))

    # Save PNG reference in docs/images
    png_out = r"docs\images\hcep-02_enhanced-diagram_keyed-01.png"
    keyed_im.save(png_out)
    print(f"Saved PNG reference: {png_out} ({keyed_im.size[0]}x{keyed_im.size[1]})")

    # Target texture destinations
    dest_dirs = [
        r"skyrim\textures\TrueGaze",
        r"build\TrueGaze\textures\TrueGaze"
    ]

    for d in dest_dirs:
        os.makedirs(d, exist_ok=True)
        dds_path = os.path.join(d, "GazeRegionPanel.dds")
        # Save as uncompressed 32-bit RGBA8 DDS (Skyrim SE native)
        try:
            keyed_im.save(dds_path, format="DDS")
            print(f"Saved DDS texture: {dds_path} (size: {os.path.getsize(dds_path)} bytes)")
        except Exception as e:
            print(f"Failed to save {dds_path}: {e}")
            return False

    return True

if __name__ == "__main__":
    success = generate_hcep_texture()
    sys.exit(0 if success else 1)
