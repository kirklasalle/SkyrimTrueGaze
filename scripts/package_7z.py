"""
package_7z.py — Creates a clean 7z release archive matching the Skyrim mod directory structure.
"""
import sys
import os
import py7zr

def main():
    if len(sys.argv) < 3:
        print("Usage: package_7z.py <stage_dir> <output_7z>")
        sys.exit(1)
    stage = sys.argv[1]
    out = sys.argv[2]
    with py7zr.SevenZipFile(out, 'w') as archive:
        for root, dirs, files in os.walk(stage):
            rel_root = os.path.relpath(root, stage)
            for f in files:
                full_path = os.path.join(root, f)
                arcname = f if rel_root == '.' else os.path.normpath(os.path.join(rel_root, f)).replace('\\', '/')
                archive.write(full_path, arcname)

if __name__ == '__main__':
    main()
