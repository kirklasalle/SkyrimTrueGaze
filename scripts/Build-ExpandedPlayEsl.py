#!/usr/bin/env python3
"""
TrueGaze Expanded Play — Optional Light Plugin Builder (WP-9D)

Generates `skyrim/TrueGaze_ExpandedPlay.esp`:
  - ESL-flagged (0x00000200) SSE Form Version 44 light plugin (0 load-order slots)
  - Master: Skyrim.esm ONLY (zero Dawnguard / Hearthfire / Dragonborn dependencies)
  - Zero Papyrus scripts (zero VMAD subrecords; all events handled natively in C++ by TrueGaze.dll)
  - All new FormIDs strictly inside ESL-compact range 0x01000801..0x01000808
  - Reuses verified vanilla Skyrim.esm meshes, ARMA armor-addons, keywords, and inventory statics

Run: python scripts/Build-ExpandedPlayEsl.py
"""

import os
import struct
import sys
from pathlib import Path


ESL_FLAG = 0x00000200
SSE_FORM_VERSION = 44

# Verified local FormIDs inside TrueGaze_ExpandedPlay.esp (master index 0x01 -> 0x010008xx)
FORM_MGEF_CANTICLE_SIGHT = 0x01000801
FORM_SPEL_CANTICLE_POWER = 0x01000802
FORM_BOOK_FIRST_CANTICLE = 0x01000803
FORM_ARMO_AMULET_MOTH    = 0x01000804
FORM_ARMO_BROWBAND_MOTH  = 0x01000805
FORM_ARMO_RING_SIGHT     = 0x01000806
FORM_ARMO_MEDALLION_MOTH = 0x01000807
FORM_MISC_MOTH_RIBBON    = 0x01000808
NEXT_OBJECT_ID           = 0x00000810


def subrec(tag: bytes, payload: bytes) -> bytes:
    assert len(tag) == 4
    if len(payload) <= 0xFFFF:
        return tag + struct.pack("<H", len(payload)) + payload
    # Large subrecord via XXXX
    return b"XXXX" + struct.pack("<HI", 4, len(payload)) + tag + struct.pack("<H", 0) + payload


def zstr(text: str) -> bytes:
    return text.encode("windows-1252") + b"\x00"


def u32(val: int) -> bytes:
    return struct.pack("<I", val)


def record(rtype: bytes, form_id: int, subrecords: list[bytes], flags: int = 0) -> bytes:
    assert len(rtype) == 4
    body = b"".join(subrecords)
    # SSE 24-byte record header:
    # type(4), dataSize(4), flags(4), formID(4), timestamp(2), vcInfo(2), version(2), unk(2)
    header = struct.pack("<4sIIIHHHH", rtype, len(body), flags, form_id, 0, 0, SSE_FORM_VERSION, 0)
    return header + body


def grup(label: bytes, records: list[bytes]) -> bytes:
    assert len(label) == 4
    body = b"".join(records)
    total_size = 24 + len(body)
    # Top-level GRUP (groupType = 0)
    header = struct.pack("<4sI4sIHH", b"GRUP", total_size, label, 0, 0, 0) + b"\x00\x00\x00\x00"
    return header + body


def build_mgef() -> bytes:
    # Verified Skyrim.esm MGEF DATA (152 bytes) blending LightFFSelf (0x0001EA6C) warm golden-white
    # hand-cast visuals & sounds with harmless kScript archetype (1), 0 cost, 0 XP skillUsageMult.
    data_152 = struct.pack(
        "<IfIiiIIfIIIIffffIiIIIIiIIIfIfIIIIIIIff",
        0x00400E00,  # flags: NoDuration | NoMagnitude | NoArea | Painless
        0.0,         # baseCost
        0,           # assocItem
        0x00000015,  # magicSkill (Illusion - Ancestor Moth sight discipline, 0 XP via skillUsageMult=0)
        -1,          # resistVar
        0,           # counterCnt
        0x00041F09,  # light (Candlelight warm golden-white hand glow)
        0.0,         # taperWeight
        0,           # hitShader
        0,           # enchantShader
        0,           # minSkill (Novice)
        0,           # spellmakingArea
        0.5,         # castingTime (0.5s glowing charge-up on hold & release)
        0.0,         # taperCurve
        0.0,         # taperDur
        0.0,         # secondAVWeight
        1,           # archetype (kScript - neutral no-op; logic handled natively in C++)
        -1,          # primaryAV (None)
        0,           # projectile
        0,           # explosion
        1,           # castType (FireAndForget)
        0,           # delivery (Self)
        -1,          # secondAV (None)
        0x00041F08,  # castingArt (visible glowing hand spell art in 1st & 3rd person)
        0x00041F06,  # hitEffectArt (warm golden burst on release)
        0,           # impactDataSet
        0.0,         # skillUsageMult (zero XP exploit)
        0,           # dualCastArt
        1.0,         # dualCastScale
        0,           # enchantArt
        0,           # unk78
        0,           # unk7C
        0,           # equipAbility
        0,           # imageSpaceMod
        0,           # perk
        0,           # soundVol
        0.0,         # scriptAIDataScore
        0.0,         # scriptAIDataDelay
    )
    assert len(data_152) == 152
    # Verified 32-byte SNDD from Skyrim.esm LightFFSelf (0x0001EA6C): Draw, Charge, Release, Loop
    sndd_32 = bytes.fromhex("0000000018a00500010000007bf30300020000007df30300030000007cf30300")
    subs = [
        subrec(b"EDID", zstr("TGZ_Effect_CanticleSight")),
        subrec(b"FULL", zstr("Sight of the First Canticle")),
        subrec(b"MDOB", u32(0x000435A5)),  # Skyrim.esm vanilla magic inventory art
        subrec(b"KSIZ", u32(1)),
        subrec(b"KWDA", u32(0x000AD7C6)),  # Neutral self keyword
        subrec(b"DATA", data_152),
        subrec(b"SNDD", sndd_32),
        subrec(b"DNAM", zstr("Channel Moth light in your right hand while holding the First Canticle open in your left. Cast anytime to inspect the four keys, awaken TrueGaze at a Sanctuary, or veil and unveil the Sight.")),
    ]
    return record(b"MGEF", FORM_MGEF_CANTICLE_SIGHT, subs)


def build_spel() -> bytes:
    # Hand-cast Right-Hand Spell (0 Magicka, ManualCostCalc, 0.5s charge, FireAndForget Self).
    # While equipped with hands raised, TrueGaze.dll visibly holds The First Canticle open in
    # the player's left hand in both 1st and 3rd person while the right hand glows with Moth light.
    spit = struct.pack("<IIIfIIffI", 0, 1, 0, 0.5, 1, 0, 0.0, 0.0, 0)
    assert len(spit) == 36
    efit = struct.pack("<fII", 1.0, 0, 0)  # magnitude=1.0, area=0, duration=0
    subs = [
        subrec(b"EDID", zstr("TGZ_Power_CanticleAwakening")),
        subrec(b"OBND", bytes(12)),
        subrec(b"FULL", zstr("Sight of the First Canticle")),
        subrec(b"MDOB", u32(0x000435A5)),
        subrec(b"ETYP", u32(0x00013F42)),  # RightHand equip slot (left hand holds open Canticle)
        subrec(b"DESC", zstr("Right-hand spell (0 Magicka). Hold the First Canticle open in your left hand and channel Moth light to inspect the four keys, complete the Awakening at a Sanctuary, or toggle TrueGaze Expanded.")),
        subrec(b"SPIT", spit),
        subrec(b"EFID", u32(FORM_MGEF_CANTICLE_SIGHT)),
        subrec(b"EFIT", efit),
    ]
    return record(b"SPEL", FORM_SPEL_CANTICLE_POWER, subs)


def build_book() -> bytes:
    # Authentic Elder Scrolls Ancestor Moth lore treatise.
    # Uses exact vanilla Skyrim.esm BookMenu markup (matching The Lunar Lorkhan):
    #   - Title page on right of Opening 1, followed by [pagebreak]\r\n[pagebreak]
    #   - Single continuous <p align="left"> body block separated by \r\n\r\n
    #     so Scaleform book.swf auto-paginates cleanly with ZERO overlapping text
    #   - Pure ASCII punctuation only (no Windows-1252 em-dashes that render as [] boxes)
    book_text = (
        "[pagebreak]\r\n"
        "<p align=\"center\">\r\n\r\n\r\n"
        "The First Canticle of the Eye\r\n\r\n"
        "Primer of the Cult of the Ancestor Moth\r\n\r\n"
        "by Brother Cassian\r\n"
        "</p>\r\n"
        "[pagebreak]\r\n"
        "[pagebreak]\r\n"
        "<p align=\"left\">\r\n"
        "Before an acolyte looks upon an Elder Scroll, they must learn to look upon a living "
        "soul without tremor. Where Skyrim's Tongues shape the world through the Voice, the "
        "Moth Prelates anchor stillness through the eye -- the TrueGaze.\r\n\r\n"
        "I. Word & Vessel -- Study these pages without haste. Bind a Moth relic or Divine "
        "amulet and hold another's gaze for three quiet breaths.\r\n\r\n"
        "II. Shared Sight & Pilgrimage -- Trade or offer a Ring of Quiet Sight, Silver Moth "
        "Medallion, or Moth-Silk Ribbon to a friendly companion and meet their eyes. Hold "
        "stillness within two Skyrim Sanctuaries (Temple of Kynareth, Sleeping Giant Inn, "
        "Eldergleam, Temple of Mara or the Divines, High Hrothgar, or the Arcanaeum).\r\n\r\n"
        "III. Final Turning -- With all four keys lit, stand in any Sanctuary wearing your "
        "Vessel with this tome in hand and cast Sight of the First Canticle.\r\n"
        "</p>"
    )
    assert all(ord(ch) < 128 for ch in book_text), "BOOK text must remain pure ASCII for Skyrim fonts"
    # Verified Skyrim.esm BOOK 0x000ED03A (Book4RareAccountingOfTheElderScrolls) template
    obnd = bytes.fromhex("f5fff1fffdff0b000f000300")
    # BOOK DATA (16 bytes): flags(1), type(1), pad(2), teaches(4)=-1 (0xFFFFFFFF), value(4)=150, weight(4)=1.0f
    data = struct.pack("<BBHIIf", 0, 0, 0, 0xFFFFFFFF, 150, 1.0)
    subs = [
        subrec(b"EDID", zstr("TGZ_Book_FirstCanticle")),
        subrec(b"OBND", obnd),
        subrec(b"FULL", zstr("The First Canticle of the Eye")),
        subrec(b"MODL", zstr("Clutter\\Books\\Book02LowPoly02.nif")),
        subrec(b"DESC", zstr(book_text)),
        subrec(b"KSIZ", u32(1)),
        subrec(b"KWDA", u32(0x000937A2)),  # VendorItemBook
        subrec(b"DATA", data),
        subrec(b"INAM", u32(0x0008ADAF)),  # Verified inventory static from 0x000ED03A
        subrec(b"CNAM", zstr("A Moth Priest primer describing the four keys of the TrueGaze.")),
    ]
    return record(b"BOOK", FORM_BOOK_FIRST_CANTICLE, subs)


def build_armo(
    form_id: int,
    edid: str,
    full_name: str,
    desc: str,
    obnd_hex: str,
    world_nif: str,
    bod2_hex: str,
    rnam: int,
    kwda_ids: list[int],
    arma_ids: list[int],
    value: int,
    weight: float,
) -> bytes:
    subs = [
        subrec(b"EDID", zstr(edid)),
        subrec(b"OBND", bytes.fromhex(obnd_hex)),
        subrec(b"FULL", zstr(full_name)),
        subrec(b"MOD2", zstr(world_nif)),
        subrec(b"BOD2", bytes.fromhex(bod2_hex)),
        subrec(b"RNAM", u32(rnam)),
        subrec(b"KSIZ", u32(len(kwda_ids))),
        subrec(b"KWDA", b"".join(u32(k) for k in kwda_ids)),
        subrec(b"DESC", zstr(desc)),
    ]
    for arma in arma_ids:
        subs.append(subrec(b"MODL", u32(arma)))
    subs.append(subrec(b"DATA", struct.pack("<If", value, weight)))
    subs.append(subrec(b"DNAM", u32(0)))  # 0 armor rating (clothing/jewelry)
    return record(b"ARMO", form_id, subs)


def build_misc_ribbon() -> bytes:
    # Cloned from verified Skyrim.esm MISC 0x00034CD6 (RuinsLinenPile01)
    subs = [
        subrec(b"EDID", zstr("TGZ_Misc_MothRibbon")),
        subrec(b"OBND", bytes.fromhex("faffe2fff9ff06001f000500")),
        subrec(b"FULL", zstr("Moth-Silk Ribbon")),
        subrec(b"MODL", zstr("Clutter\\Ruins\\RuinsLinenWrap01.nif")),
        subrec(b"KSIZ", u32(1)),
        subrec(b"KWDA", u32(0x000914EE)),  # VendorItemClutter
        subrec(b"DATA", struct.pack("<If", 25, 0.1)),
    ]
    return record(b"MISC", FORM_MISC_MOTH_RIBBON, subs)


def build_plugin(out_path: Path) -> bytes:
    # Build top-level GRUPs in canonical Skyrim.esm order: MGEF, SPEL, ARMO, BOOK, MISC
    mgef_recs = [build_mgef()]
    spel_recs = [build_spel()]
    armo_recs = [
        # 1. Amulet of the Ancestor Moth (Player Toggle Relic — cloned from JewelryNecklaceGoldGems 0x00087835)
        build_armo(
            form_id=FORM_ARMO_AMULET_MOTH,
            edid="TGZ_Amulet_AncestorMoth",
            full_name="Amulet of the Ancestor Moth",
            desc="A Cyrodiilic Moth Prelate's talisman. Equip to attune and toggle TrueGaze Expanded Play.",
            obnd_hex="fdfff7ff0000030009000100",
            world_nif="Armor\\AmuletsandRings\\GoldAmuletGemsGO.nif",
            bod2_hex="2000000002000000",  # Amulet slot (0x20), Clothing (2)
            rnam=0x00013749,
            kwda_ids=[0x0006BBE9, 0x0008F95A, 0x000A8664, 0x0010CD0A],
            arma_ids=[0x0009171F],
            value=250,
            weight=0.5,
        ),
        # 2. Moth-Silk Browband (Player Toggle Relic — cloned from ClothesCirclet01 0x000166D6)
        build_armo(
            form_id=FORM_ARMO_BROWBAND_MOTH,
            edid="TGZ_Circlet_MothBrowband",
            full_name="Moth-Silk Browband",
            desc="Gold-threaded browband of the First Canticle. Equip to attune and toggle TrueGaze Expanded Play.",
            obnd_hex="fbfff7fff7ff07000900fbff",
            world_nif="Armor\\Circlets\\Circlet1_GO.nif",
            bod2_hex="0010000002000000",  # Circlet slot (0x1000), Clothing (2)
            rnam=0x00000019,
            kwda_ids=[0x0006BBE9, 0x0008F95A, 0x000A8664, 0x0010CD08],
            arma_ids=[0x000166D5, 0x00016760, 0x00016757],
            value=250,
            weight=0.5,
        ),
        # 3. Ring of Quiet Sight (Giftable NPC/Follower Relic — cloned from JewelryRingSilver 0x0003B97C)
        build_armo(
            form_id=FORM_ARMO_RING_SIGHT,
            edid="TGZ_Ring_QuietSight",
            full_name="Ring of Quiet Sight",
            desc="A silver ring inscribed with a Moth glyph. Share with a follower or companion to awaken Kindred Sight.",
            obnd_hex="ffffffff0000010001000000",
            world_nif="Armor\\AmuletsandRings\\SilverRingGO.nif",
            bod2_hex="4000000002000000",  # Ring slot (0x40), Clothing (2)
            rnam=0x00013749,
            kwda_ids=[0x0006BBE9, 0x0008F95A, 0x0010CD09],
            arma_ids=[0x0008E83F],
            value=120,
            weight=0.25,
        ),
        # 4. Silver Moth Medallion (Giftable NPC/Follower Relic — cloned from JewelryNecklaceSilverGems 0x00087834)
        build_armo(
            form_id=FORM_ARMO_MEDALLION_MOTH,
            edid="TGZ_Amulet_MothMedallion",
            full_name="Silver Moth Medallion",
            desc="A silver Moth Prelate medallion. Gift to followers or allies to grant Kindred Sight and watch-warnings.",
            obnd_hex="fdfff7ff0000030009000100",
            world_nif="Armor\\AmuletsandRings\\SilverAmuletGemsGO.nif",
            bod2_hex="2000000002000000",  # Amulet slot (0x20), Clothing (2)
            rnam=0x00013749,
            kwda_ids=[0x0006BBE9, 0x0008F95A, 0x000A8664, 0x0010CD0A],
            arma_ids=[0x0009171C],
            value=180,
            weight=0.5,
        ),
    ]
    book_recs = [build_book()]
    misc_recs = [build_misc_ribbon()]

    groups = [
        grup(b"MGEF", mgef_recs),
        grup(b"SPEL", spel_recs),
        grup(b"ARMO", armo_recs),
        grup(b"BOOK", book_recs),
        grup(b"MISC", misc_recs),
    ]

    # HEDR.numRecords = total records + GRUP headers (standard TES4 convention)
    num_records = len(groups) + len(mgef_recs) + len(spel_recs) + len(armo_recs) + len(book_recs) + len(misc_recs)
    tes4_subs = [
        subrec(b"HEDR", struct.pack("<fII", 1.70, num_records, NEXT_OBJECT_ID)),
        subrec(b"CNAM", zstr("Kirk LaSalle (TrueGaze)")),
        subrec(b"SNAM", zstr("TrueGaze Expanded Play optional light plugin (ESL-flagged, Skyrim.esm only, zero Papyrus scripts).")),
        subrec(b"MAST", zstr("Skyrim.esm")),
        subrec(b"DATA", struct.pack("<Q", 0)),
    ]
    tes4 = record(b"TES4", 0, tes4_subs, flags=ESL_FLAG)
    blob = tes4 + b"".join(groups)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(blob)
    return blob


def verify_plugin(path: Path) -> None:
    data = path.read_bytes()
    pos = 0
    rtype, rsize, rflags, rform, _, _, rver, _ = struct.unpack_from("<4sIIIHHHH", data, pos)
    assert rtype == b"TES4", f"Expected TES4, got {rtype}"
    assert rflags == ESL_FLAG, f"Expected ESL flag 0x{ESL_FLAG:X}, got 0x{rflags:X}"
    assert rver == SSE_FORM_VERSION, f"Expected form version {SSE_FORM_VERSION}, got {rver}"
    tes4_body = data[24 : 24 + rsize]
    masters = []
    p = 0
    hedr_count = 0
    while p < len(tes4_body):
        st = tes4_body[p : p + 4]
        sl = struct.unpack_from("<H", tes4_body, p + 4)[0]
        sd = tes4_body[p + 6 : p + 6 + sl]
        p += 6 + sl
        if st == b"MAST":
            masters.append(sd.rstrip(b"\x00").decode("latin1"))
        elif st == b"HEDR":
            ver, hedr_count, next_id = struct.unpack_from("<fII", sd, 0)
            assert abs(ver - 1.70) < 0.01
    assert masters == ["Skyrim.esm"], f"Expected Skyrim.esm only, got {masters}"

    pos = 24 + rsize
    seen_records = []
    grup_count = 0
    while pos < len(data):
        gt, gs, gl, gk = struct.unpack_from("<4sI4sI", data, pos)
        assert gt == b"GRUP", f"Expected GRUP at {pos}, got {gt}"
        grup_count += 1
        end = pos + gs
        pos += 24
        while pos < end:
            rt, rs, rf, rform, _, _, rv, _ = struct.unpack_from("<4sIIIHHHH", data, pos)
            assert rv == SSE_FORM_VERSION
            assert 0x01000800 <= rform <= 0x01000FFF, f"FormID 0x{rform:08X} outside ESL compact range!"
            rbody = data[pos + 24 : pos + 24 + rs]
            pos += 24 + rs
            sp = 0
            edid = ""
            full = ""
            while sp < len(rbody):
                st = rbody[sp : sp + 4]
                sl = struct.unpack_from("<H", rbody, sp + 4)[0]
                sp += 6
                if st == b"XXXX":
                    sl = struct.unpack_from("<I", rbody, sp)[0]
                    sp += 4
                    st = rbody[sp : sp + 4]
                    sp += 6
                sd = rbody[sp : sp + sl]
                sp += sl
                assert st != b"VMAD", f"Record {edid} must not contain Papyrus VMAD!"
                if st == b"EDID":
                    edid = sd.rstrip(b"\x00").decode("latin1")
                elif st == b"FULL":
                    full = sd.rstrip(b"\x00").decode("windows-1252")
            seen_records.append((rt.decode(), rform, edid, full))
        assert pos == end, f"GRUP size mismatch at {pos} vs {end}"
    assert pos == len(data), f"Trailing bytes: {len(data) - pos}"
    assert hedr_count == grup_count + len(seen_records), f"HEDR count mismatch: {hedr_count}"

    print(f"Verified {path} ({len(data)} bytes) | ESL=YES | FormVer=44 | Masters={masters} | Zero Papyrus")
    for rt, fid, edid, full in seen_records:
        print(f"  {rt}  0x{fid:08X}  {edid:28s}  '{full}'")


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    out_path = repo_root / "skyrim" / "TrueGaze_ExpandedPlay.esp"
    build_plugin(out_path)
    verify_plugin(out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
