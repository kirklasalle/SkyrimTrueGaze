#!/usr/bin/env python3
"""
TrueGaze Forensic .ess Save Inspector (CRIT-04 Verification Tool)

Pure-Python (zero external dependencies) Skyrim SE/AE/VR (.ess v12) binary
ChangeForm inspector for PlayerCharacter (ACHR RefID 0x00000014).

Capabilities:
  1. Decompresses SSE/AE/VR LZ4-compressed .ess save bodies (pure Python LZ4 block decoder).
  2. Locates PlayerCharacter ACHR ChangeForm (RefID 0x000014, formType 1).
  3. Reports exact ACHR changeFlags (including CHANGE_ACTOR_PERM_ACTOR_VALUE_MODIFIERS
     and CHANGE_ACTOR_TEMP_ACTOR_VALUE_MODIFIERS) and compares raw/decompressed
     Player ACHR payloads and ActorValue modifier blocks between two saves.

Usage:
  python scripts/inspect_ess_actor_values.py <save.ess>
  python scripts/inspect_ess_actor_values.py <baseline.ess> <after_buffs.ess>
"""

from __future__ import annotations

import hashlib
import struct
import sys
import zlib
from pathlib import Path
from typing import Any


# Expanded Play ActorValue IDs tracked by ExpandedPlayModifiers.cpp
EXPANDED_PLAY_AVS: dict[int, str] = {
    18: "Speechcraft",
    20: "Illusion",
    28: "MagickaRateMult",
    29: "StaminaRateMult",
    39: "DamageResist",
    42: "ResistMagic",
    91: "OneHandedModifier",
    92: "TwoHandedModifier",
    93: "MarksmanModifier",
    94: "BlockModifier",
    102: "SneakingModifier",
    103: "PickpocketModifier",
    106: "SpeechcraftModifier",
    109: "IllusionModifier",
}

# UESP TES5 ACHR ChangeFlags bits (Actor / Character)
ACHR_FLAGS: dict[int, str] = {
    0x00000001: "CHANGE_FORM_FLAGS",
    0x00000002: "CHANGE_REFR_MOVE",
    0x00000004: "CHANGE_REFR_HAVOK_MOVE",
    0x00000008: "CHANGE_REFR_CELL_CHANGED",
    0x00000010: "CHANGE_REFR_SCALE",
    0x00000020: "CHANGE_REFR_INVENTORY",
    0x00000040: "CHANGE_REFR_EXTRA_OWNERSHIP",
    0x00000080: "CHANGE_ACTOR_LIFESTATE_OR_PERM_AV",
    0x00000100: "CHANGE_ACTOR_EXTRA_PACKAGE_DATA",
    0x00000200: "CHANGE_ACTOR_EXTRA_MERCHANT_CONTAINER",
    0x00000400: "CHANGE_ACTOR_EXTRA_DISPOSITION_MODIFIERS",
    0x00000800: "CHANGE_ACTOR_PERM_ACTOR_VALUE_MODIFIERS",
    0x00001000: "CHANGE_ACTOR_TEMP_ACTOR_VALUE_MODIFIERS",
    0x00002000: "CHANGE_ACTOR_DAMAGE_ACTOR_VALUE_MODIFIERS",
    0x00004000: "CHANGE_ACTOR_OVERRIDE_MODIFIERS",
    0x02000000: "CHANGE_ACTOR_BASE_DATA",
}


def lz4_decompress_block(src: bytes, uncompressed_size: int) -> bytes:
    """Pure-Python LZ4 raw block decompressor (LZ4 block format spec)."""
    out = bytearray()
    i = 0
    n = len(src)
    while i < n:
        token = src[i]
        i += 1
        lit_len = token >> 4
        if lit_len == 15:
            while i < n:
                b = src[i]
                i += 1
                lit_len += b
                if b != 255:
                    break
        if lit_len:
            out.extend(src[i : i + lit_len])
            i += lit_len
        if i >= n:
            break
        offset = src[i] | (src[i + 1] << 8)
        i += 2
        if offset == 0:
            raise ValueError("Invalid LZ4 offset 0")
        match_len = (token & 0x0F) + 4
        if (token & 0x0F) == 15:
            while i < n:
                b = src[i]
                i += 1
                match_len += b
                if b != 255:
                    break
        start = len(out) - offset
        if start < 0:
            raise ValueError(f"Invalid LZ4 back-reference offset {offset} at out len {len(out)}")
        if offset >= match_len:
            out.extend(out[start : start + match_len])
        else:
            for k in range(match_len):
                out.append(out[start + k])
    if uncompressed_size and len(out) != uncompressed_size:
        raise ValueError(
            f"LZ4 size mismatch: expected {uncompressed_size}, got {len(out)}"
        )
    return bytes(out)


def read_wstring(buf: bytes, pos: int) -> tuple[str, int]:
    length = struct.unpack_from("<H", buf, pos)[0]
    pos += 2
    text = buf[pos : pos + length].decode("windows-1252", errors="replace")
    return text, pos + length


def decode_ref_id(b0: int, b1: int, b2: int) -> tuple[int, int]:
    ref_type = (b0 >> 6) & 0x03
    value = ((b0 & 0x3F) << 16) | (b1 << 8) | b2
    return ref_type, value


def parse_ess(path: Path) -> dict[str, Any]:
    raw = path.read_bytes()
    if raw[:13] != b"TESV_SAVEGAME":
        raise ValueError(f"{path}: Not a TESV_SAVEGAME file (magic={raw[:13]!r})")

    pos = 13
    header_size = struct.unpack_from("<I", raw, pos)[0]
    pos += 4
    header_end = pos + header_size

    version, save_number = struct.unpack_from("<II", raw, pos)
    pos += 8
    player_name, pos = read_wstring(raw, pos)
    player_level = struct.unpack_from("<I", raw, pos)[0]
    pos += 4
    player_location, pos = read_wstring(raw, pos)
    game_date, pos = read_wstring(raw, pos)
    player_race, pos = read_wstring(raw, pos)
    _sex, _cur_exp, _lvl_exp, _filetime, shot_w, shot_h = struct.unpack_from(
        "<HffQII", raw, pos
    )
    pos += struct.calcsize("<HffQII")
    compression_type = 0
    if version >= 12 and pos + 2 <= header_end:
        compression_type = struct.unpack_from("<H", raw, pos)[0]

    pos = header_end
    bpp = 4 if version >= 12 else 3
    pos += shot_w * shot_h * bpp

    if version >= 12 and compression_type == 2:
        uncompressed_len, compressed_len = struct.unpack_from("<II", raw, pos)
        pos += 8
        body = lz4_decompress_block(raw[pos : pos + compressed_len], uncompressed_len)
    elif version >= 12 and compression_type == 1:
        uncompressed_len, compressed_len = struct.unpack_from("<II", raw, pos)
        pos += 8
        body = zlib.decompress(raw[pos : pos + compressed_len])
    else:
        body = raw[pos:]

    bpos = 0
    form_version = body[bpos]
    bpos += 1
    plugin_info_size = struct.unpack_from("<I", body, bpos)[0]
    bpos += 4
    plugin_blob = body[bpos : bpos + plugin_info_size]
    bpos += plugin_info_size

    plugins: list[str] = []
    light_plugins: list[str] = []
    ppos = 0
    reg_count = plugin_blob[ppos]
    ppos += 1
    for _ in range(reg_count):
        name, ppos = read_wstring(plugin_blob, ppos)
        plugins.append(name)
    if form_version >= 78 and ppos + 2 <= len(plugin_blob):
        light_count = struct.unpack_from("<H", plugin_blob, ppos)[0]
        ppos += 2
        for _ in range(light_count):
            name, ppos = read_wstring(plugin_blob, ppos)
            light_plugins.append(name)

    flt = struct.unpack_from("<10I", body, bpos)
    (
        _form_id_array_off,
        _unk3_off,
        gd1_off,
        _gd2_off,
        change_forms_off,
        _gd3_off,
        _gd1_cnt,
        _gd2_cnt,
        _gd3_cnt,
        change_form_cnt,
    ) = flt
    bpos += 25 * 4

    # Global Data Table 1 starts immediately at bpos in the decompressed body,
    # so gd1_off - bpos adjusts FileLocationTable file-space offsets to body-space.
    offset_bias = gd1_off - bpos
    cf_start = change_forms_off - offset_bias

    player_cf: dict[str, Any] | None = None
    cpos = cf_start
    for idx in range(change_form_cnt):
        ref_type, ref_val = decode_ref_id(body[cpos], body[cpos + 1], body[cpos + 2])
        cpos += 3
        change_flags = struct.unpack_from("<I", body, cpos)[0]
        cpos += 4
        type_byte = body[cpos]
        cpos += 1
        form_type = type_byte & 0x3F
        len_class = (type_byte >> 6) & 0x03
        cf_version = body[cpos]
        cpos += 1
        if len_class == 0:
            len1, len2 = struct.unpack_from("<BB", body, cpos)
            cpos += 2
        elif len_class == 1:
            len1, len2 = struct.unpack_from("<HH", body, cpos)
            cpos += 4
        else:
            len1, len2 = struct.unpack_from("<II", body, cpos)
            cpos += 8
        cf_raw = body[cpos : cpos + len1]
        cpos += len1

        # PlayerCharacter is RefID type=1 (default/Skyrim.esm), value=0x000014, formType=1 (ACHR)
        if form_type == 1 and ref_type == 1 and ref_val == 0x000014:
            if len2 != 0:
                if compression_type == 2:
                    cf_data = lz4_decompress_block(cf_raw, len2)
                else:
                    cf_data = zlib.decompress(cf_raw)
            else:
                cf_data = cf_raw
            player_cf = {
                "index": idx,
                "change_flags": change_flags,
                "version": cf_version,
                "compressed_len": len1,
                "uncompressed_len": len(cf_data),
                "sha256": hashlib.sha256(cf_data).hexdigest(),
                "data": cf_data,
            }
            break

    return {
        "path": str(path),
        "version": version,
        "form_version": form_version,
        "compression_type": compression_type,
        "save_number": save_number,
        "player_name": player_name,
        "player_level": player_level,
        "player_location": player_location,
        "game_date": game_date,
        "has_truegaze_esl": any(
            p.lower() == "truegaze_expandedplay.esp" for p in (plugins + light_plugins)
        ),
        "change_form_count": change_form_cnt,
        "player_achr": player_cf,
    }


def scan_actor_value_modifier_vectors(achr_data: bytes) -> list[dict[str, Any]]:
    """
    Scan decompressed Player ACHR ChangeForm bytes for ActorValueModifier arrays:
    count-prefixed runs of (uint8 avIndex, float32 delta) matching tracked AVs.
    """
    hits: list[dict[str, Any]] = []
    n = len(achr_data)
    i = 0
    while i + 10 < n:
        cnt = achr_data[i]
        if 1 <= cnt <= 24 and i + 1 + cnt * 5 <= n:
            pairs: list[tuple[int, float]] = []
            seen: set[int] = set()
            ok = True
            for k in range(cnt):
                av = achr_data[i + 1 + k * 5]
                val = struct.unpack_from("<f", achr_data, i + 2 + k * 5)[0]
                if (
                    av > 163
                    or av in seen
                    or val != val
                    or abs(val) < 1e-4
                    or abs(val) > 500.0
                    or (abs(val) < 0.01 and abs(val) > 0.0)
                ):
                    ok = False
                    break
                seen.add(av)
                pairs.append((av, val))
            if ok and any(av in EXPANDED_PLAY_AVS for av, _ in pairs) and (
                cnt >= 2 or any(av in (28, 106, 109) for av, _ in pairs)
            ):
                if all(abs(v * 20.0 - round(v * 20.0)) < 1e-3 for _, v in pairs):
                    hits.append(
                        {
                            "offset": i,
                            "count": cnt,
                            "entries": [
                                (av, EXPANDED_PLAY_AVS.get(av, f"AV_{av}"), round(v, 3))
                                for av, v in pairs
                            ],
                        }
                    )
                    i += 1 + cnt * 5
                    continue
        i += 1
    return hits


def print_report(info: dict[str, Any]) -> None:
    print(f"=== Save: {info['path']} ===")
    print(
        f"  Version={info['version']} FormVer={info['form_version']} "
        f"Compression={'LZ4' if info['compression_type']==2 else info['compression_type']} "
        f"Save#{info['save_number']}"
    )
    print(
        f"  Player='{info['player_name']}' Lv={info['player_level']} "
        f"Loc='{info['player_location']}' Date='{info['game_date']}'"
    )
    print(f"  TrueGaze_ExpandedPlay.esp in load order: {info['has_truegaze_esl']}")
    cf = info["player_achr"]
    if not cf:
        print("  [ERROR] PlayerCharacter ACHR (0x000014) ChangeForm not found!")
        return
    flags = cf["change_flags"]
    print(
        f"  Player ACHR (index={cf['index']}): changeFlags=0x{flags:08X} "
        f"size={cf['uncompressed_len']} B (compressed={cf['compressed_len']} B) "
        f"sha256={cf['sha256'][:16]}"
    )
    active_flags = [name for bit, name in ACHR_FLAGS.items() if flags & bit]
    print(f"  Active ACHR flags: {', '.join(active_flags)}")
    candidates = scan_actor_value_modifier_vectors(cf["data"])
    if candidates:
        print(f"  Candidate ActorValue modifier blocks ({len(candidates)}):")
        for c in candidates:
            print(f"    @0x{c['offset']:04X} (n={c['count']}): {c['entries']}")
    else:
        print("  Candidate ExpandedPlay ActorValue modifier blocks: NONE detected")


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: python scripts/inspect_ess_actor_values.py <save1.ess> [save2.ess]")
        return 2

    infos = [parse_ess(Path(arg)) for arg in sys.argv[1:]]
    for info in infos:
        print_report(info)

    if len(infos) == 2 and infos[0]["player_achr"] and infos[1]["player_achr"]:
        c0 = infos[0]["player_achr"]
        c1 = infos[1]["player_achr"]
        print("\n=== Forensic Comparison (Save 1 vs Save 2) ===")
        print(
            f"  ACHR changeFlags: 0x{c0['change_flags']:08X} -> 0x{c1['change_flags']:08X} "
            f"(XOR=0x{c0['change_flags'] ^ c1['change_flags']:08X})"
        )
        print(f"  ACHR size:        {c0['uncompressed_len']} B -> {c1['uncompressed_len']} B")
        v0 = {tuple(x["entries"]) for x in scan_actor_value_modifier_vectors(c0["data"])}
        v1 = {tuple(x["entries"]) for x in scan_actor_value_modifier_vectors(c1["data"])}
        added = v1 - v0
        removed = v0 - v1
        print(f"  New AV modifier blocks in Save 2:     {len(added)}")
        for blk in sorted(added):
            print(f"    + {blk}")
        print(f"  Removed AV modifier blocks in Save 2: {len(removed)}")
        for blk in sorted(removed):
            print(f"    - {blk}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
