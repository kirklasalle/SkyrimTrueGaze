"""Decode SSE BSA v105 (post-1.6.629) and extract marker_arrow.nif.

Layout discovered by probing Skyrim - Meshes0.bsa:
  Header (36B): magic, version=105, offsetVersion, folderCount, fileCount,
                totalFolderNameLen, totalFileNameLen, fileFlags
  Folder records (24B each at 36): hash(u64), fileCount(u32),
                offsetToFolderData(u32, RELATIVE to end-of-names base),
                unk(u32), unk(u32)
  ... region with folder names + file names (null-terminated) ...
  Folder data blocks: [namelen(u8)][name][fileCount * 16B file records]
  File names block (null-terminated, in folder order)
  Raw file data (zlib if size & 0x40000000)

The v105 twist: the folder record's offset field is relative to a base
located AFTER the header+folder records. We locate the folder blocks by
scanning for the target folder name with its length prefix.
"""
import struct
import sys
import zlib


def extract(bsa_path, want_path, out_path):
    want = want_path.lower().replace('\\', '/')
    want_folder, _, want_file = want.rpartition('/')
    want_folder = want_folder.strip('/')

    with open(bsa_path, 'rb') as f:
        data = f.read()

    magic, version, _, folder_count, file_count, _, _, _ = \
        struct.unpack_from('<4s7I', data, 0)
    assert magic == b'BSA\x00', f"not a BSA: {magic}"

    # Read folder records
    recs = []
    for i in range(folder_count):
        h, cnt, off, u1, u2 = struct.unpack_from('<QIIII', data, 36 + i * 24)
        recs.append((h, cnt, off, u1, u2))

    # The folder-data region: find the base by locating the first folder block.
    # Folder blocks are [namelen][name][cnt*16 file records]. The names region
    # (folder names + file names) sits between the folder records and the
    # folder blocks in v105? We probe: find our target folder block by scanning
    # for [len(prefix)][foldername] followed by plausible file records.
    prefix = bytes([len(want_folder)]) + want_folder.encode('ascii')
    candidates = []
    start = 36 + folder_count * 24
    pos = data.find(prefix, start)
    while pos != -1 and len(candidates) < 50:
        candidates.append(pos)
        pos = data.find(prefix, pos + 1)

    for c in candidates:
        # find the folder record whose count matches the records that follow
        p = c + 1 + len(want_folder)
        # try each possible file count: read records and check plausibility
        # (size < 100MB, offset < filesize, offset+size <= filesize+pad)
        for h, cnt, off, u1, u2 in recs:
            if cnt == 0 or cnt > 5000:
                continue
            ok = True
            file_recs = []
            for j in range(min(cnt, 3)):
                try:
                    fh, fsize, foff = struct.unpack_from('<QII', data, p + j * 16)
                except struct.error:
                    ok = False
                    break
                real_size = fsize & 0x3FFFFFFF
                if foff >= len(data) or real_size > 200 * 1024 * 1024:
                    ok = False
                    break
                file_recs.append((fh, fsize, foff))
            if ok and file_recs:
                # verify ALL records
                all_ok = True
                for j in range(cnt):
                    fh, fsize, foff = struct.unpack_from('<QII', data, p + j * 16)
                    real_size = fsize & 0x3FFFFFFF
                    if foff >= len(data) or real_size > 200 * 1024 * 1024:
                        all_ok = False
                        break
                if all_ok:
                    # This looks like the folder block. Now find the file names.
                    # File names block: after ALL folder blocks. We need the
                    # file NAME for each record to match want_file.
                    # v105: file names are null-terminated, in folder order.
                    # Find the names block: it should follow the last folder block.
                    # Heuristic: scan forward from the end of this folder's
                    # records for want_file + '\0'.
                    nend = p + cnt * 16
                    idx = data.find(want_file.encode('ascii') + b'\x00', nend)
                    if idx == -1:
                        continue
                    # The names block starts somewhere in [nend, idx]. Names are
                    # sequential null-terminated. We can't easily know which
                    # record maps to which name without the block start.
                    # BUT: BSA file records within a folder are sorted by hash,
                    # and names are stored in the SAME ORDER as records.
                    # So: find the names block start, then read cnt names.
                    # The block start: the total file-name length is known from
                    # the header. The block ends right before the raw data of
                    # the first file. Estimate: scan back from idx for a run of
                    # plausible names. Simpler: the names block likely starts
                    # right after the LAST folder block. The last folder block's
                    # u1 is the max u1. Its end = u1 + 1 + len(name) + cnt*16.
                    max_u1 = max(r[3] for r in recs)
                    # find the folder record with max u1 to get its count/name
                    for h2, cnt2, off2, u12, u22 in recs:
                        if u12 == max_u1:
                            q = u12
                            nl2 = data[q]
                            fname2 = data[q + 1:q + 1 + nl2]
                            names_start = q + 1 + nl2 + cnt2 * 16
                            break
                    # read all file_count names from names_start
                    names = []
                    np_ = names_start
                    for _ in range(file_count):
                        end = data.index(b'\x00', np_)
                        names.append(data[np_:end].decode('ascii').lower())
                        np_ = end + 1
                    if len(names) != file_count:
                        continue
                    # map: entries in folder order. We must walk all folder
                    # blocks in the same order the names were stored.
                    # Walk folder blocks by sorted u1? Names are in the order
                    # the folders appear in the folder-record table.
                    # Reconstruct: for each folder record (in table order),
                    # its block is at u1 (absolute). Verify by name.
                    entries = []
                    valid = True
                    for h3, cnt3, off3, u13, u23 in recs:
                        q = u13
                        nl3 = data[q]
                        fblock_name = data[q + 1:q + 1 + nl3].decode('ascii').lower()
                        for j in range(cnt3):
                            fh, fsize, foff = struct.unpack_from('<QII', data, q + 1 + nl3 + j * 16)
                            entries.append((fblock_name, fh, fsize, foff))
                    if len(entries) != file_count:
                        continue
                    for (fblock_name, fh, fsize, foff), fname in zip(entries, names):
                        if fblock_name == want_folder and fname == want_file:
                            raw = data[foff:foff + (fsize & 0x3FFFFFFF)]
                            if fsize & 0x40000000:
                                raw = zlib.decompress(raw)
                            with open(out_path, 'wb') as o:
                                o.write(raw)
                            print(f"extracted {fblock_name}/{fname} -> {out_path} ({len(raw)} bytes)")
                            return True
    print(f"NOT FOUND: {want}")
    return False


if __name__ == "__main__":
    bsa, path, out = sys.argv[1], sys.argv[2], sys.argv[3]
    sys.exit(0 if extract(bsa, path, out) else 1)
