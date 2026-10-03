"""Extract a file from a Skyrim SSE BSA (v3 format) by path.

BSA v3 layout (SSE):
  Header: magic 'BSA\\', version=0x68 (104), offsetVersion=0x68,
          folderCount, fileCount, totalFolderNameLength, totalFileNameLength,
          fileFlags (bit 0 = meshes, 1 = textures, ...)
  Folder records: folderNameHash(u64), fileCount(u32), offsetToFolderRecordData(u32),
                  unknown(u32), unknown(u32)  -- 24 bytes each
  Folder data blocks (at folderRecordDataOffset): nameLength(u8), name (lowercase,
          no trailing backslash), then fileCount file records:
          fileHash(u64), fileSize(u32), offsetToRawData(u32)  -- 16 bytes each
  File names block, then raw data (zlib-compressed if size & 0x40000000).

Usage: python extract_bsa.py <bsa> <virtual-path> <out>
       virtual-path like: meshes/marker_arrow.nif
"""
import struct
import sys
import zlib


def bsa_hash(s):
    """Bethesda's BSA name hash (for verification only; we scan linearly)."""
    s = s.lower()
    v = 0
    for c in s[:3]:
        v = v * 0x1003 + ord(c)
    v &= 0xFFFFFFFF
    if len(s) > 2:
        v2 = ord(s[-2]) | (ord(s[-1]) << 8)
    else:
        v2 = 0
    v3 = 0
    for c in s[3:-2]:
        if c in '\\/:>':
            continue
        v3 = v3 * 0x1003 + ord(c)
    v3 &= 0xFFFFFFFF
    return v | (v2 << 32) | (v3 << 48)


def extract(bsa_path, want_path, out_path):
    want = want_path.lower().replace('\\', '/')
    want_folder, _, want_file = want.rpartition('/')
    want_folder = want_folder.strip('/')
    want_file = want_file

    with open(bsa_path, 'rb') as f:
        data = f.read()

    magic, version, offset_version, folder_count, file_count, \
        total_folder_name_len, total_file_name_len, file_flags = \
        struct.unpack_from('<4s7I', data, 0)
    assert magic == b'BSA\x00', f"not a BSA: {magic}"
    assert version in (0x68, 0x69), f"unsupported BSA version: {version}"

    # Folder records start at 0x18, 24 bytes each
    folder_recs_off = 0x18
    # Folder data blocks start after all folder records
    folder_data_off = folder_recs_off + folder_count * 24

    pos = folder_data_off
    for i in range(folder_count):
        # folder record i
        _, fcount, fdata_off, _, _ = struct.unpack_from('<QIIII', data, folder_recs_off + i * 24)
        # folder data block at fdata_off (absolute)
        p = fdata_off
        name_len = data[p]
        folder_name = data[p + 1:p + 1 + name_len].decode('ascii').lower()
        p += 1 + name_len

        if folder_name != want_folder:
            continue

        # file records
        for j in range(fcount):
            fhash, fsize, foff = struct.unpack_from('<QII', data, p + j * 16)
            # We need the file NAME — read from the file-names block.
            # The names block starts after ALL folder data blocks; each folder
            # data block's file records are followed by names in the same order.
            # Per spec: names are stored in a contiguous block after all folder
            # records' data. We locate it via the LAST folder's data end.
            pass
        # fall through to linear name scan below
        break

    # Simpler robust approach: walk every folder block, collecting
    # (folder, file record) pairs, then read the names block once.
    entries = []  # (folder_name, fhash, fsize, foff)
    p = folder_data_off
    for i in range(folder_count):
        _, fcount, fdata_off, _, _ = struct.unpack_from('<QIIII', data, folder_recs_off + i * 24)
        q = fdata_off
        name_len = data[q]
        folder_name = data[q + 1:q + 1 + name_len].decode('ascii').lower()
        q += 1 + name_len
        recs_off = q
        q += fcount * 16
        for j in range(fcount):
            fhash, fsize, foff = struct.unpack_from('<QII', data, recs_off + j * 16)
            entries.append((folder_name, fhash, fsize, foff))
        p = q

    # Names block: after the last folder data block
    names_off = p
    names = []
    np_ = names_off
    for _ in range(file_count):
        name_len = data[np_]
        names.append(data[np_ + 1:np_ + 1 + name_len].decode('ascii').lower())
        np_ += 1 + name_len

    assert len(names) == file_count, f"name count mismatch {len(names)} vs {file_count}"

    # Match entries to names: entries were collected in folder order, and the
    # names block lists files in the same order (folder by folder).
    for (folder_name, fhash, fsize, foff), fname in zip(entries, names):
        full = f"{folder_name}/{fname}"
        if full == want:
            raw = data[foff:foff + (fsize & 0x3FFFFFFF)]
            if fsize & 0x40000000:
                raw = zlib.decompress(raw)
            with open(out_path, 'wb') as o:
                o.write(raw)
            print(f"extracted {full} -> {out_path} ({len(raw)} bytes)")
            return True

    print(f"NOT FOUND: {want}")
    return False


if __name__ == "__main__":
    bsa, path, out = sys.argv[1], sys.argv[2], sys.argv[3]
    sys.exit(0 if extract(bsa, path, out) else 1)
