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
import wave
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
FORM_BOOK_CASSIAN_NOTE   = 0x01000809
FORM_DIAL_ALL_IN_NOTE    = 0x0100080A
FORM_INFO_ALL_IN_NOTE    = 0x0100080B
FORM_QUST_PILGRIMAGE     = 0x0100080C
FORM_DLBR_INN_RUMOR      = 0x0100080D
FORM_DIAL_INN_RUMOR      = 0x0100080E
FORM_INFO_INN_RUMOR      = 0x0100080F
FORM_DLBR_RUMOR_SHARED   = 0x01000810
FORM_DIAL_RUMOR_SHARED   = 0x01000811
FORM_INFO_RUMOR_SHARED   = 0x01000812
NEXT_OBJECT_ID           = 0x00000820


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


def build_cassian_note() -> bytes:
    # WP-11D living rumor & Awakening checklist note. When opened in-game with TrueGaze.dll
    # loaded, BookStudyTracker dynamically updates its text with the player's live [X]/[ ]
    # Awakening progress; this static text serves as a crisp fallback.
    note_text = (
        "<p align=\"left\">\r\n"
        "Brother Cassian's Ledger of the TrueGaze\r\n\r\n"
        "Before an Ancestor Moth acolyte reads an Elder Scroll, they learn the First Canticle -- "
        "stillness of the eye that reads intention, steadies persuasion, anticipates blows in "
        "battle, and meets beasts and Dragons without flinching.\r\n\r\n"
        "The Four Keys (any order):\r\n"
        "[ ] The Word -- Study 'The First Canticle of the Eye' slowly from start to finish.\r\n"
        "[ ] The Vessel -- Equip a Moth relic (or Divine Amulet/Circlet) and hold an NPC's gaze "
        "for 3 breaths.\r\n"
        "[ ] Shared Sight -- Trade or drop a Ring of Quiet Sight, Silver Moth Medallion, or "
        "Moth-Silk Ribbon beside a friendly NPC and meet their eyes.\r\n"
        "[ ] The Pilgrimage -- Hold stillness in 2 Skyrim Sanctuaries (Sleeping Giant Inn, "
        "Temple of Kynareth, Eldergleam, Temple of Mara, Temple of the Divines, High Hrothgar, "
        "or the Arcanaeum).\r\n\r\n"
        "-- Brother Cassian\r\n"
        "</p>"
    )
    assert all(ord(ch) < 128 for ch in note_text), "Note text must remain pure ASCII"
    # Match vanilla Skyrim.esm note records (WERoad08CourierLetter 0x001065F5 / ccBGSSSE051_BountyLetter):
    #   - OBND = 12 zero bytes
    #   - YNAM = 0x000C7A54 (ITMNoteUpSD parchment rustle sound)
    #   - DATA = value 0, weight 0.0 (zero-value note is never marked as theft!)
    #   - INAM = 0x00097788 (STAT HighPolyNote: Clutter\Books\Note01\Note01.nif) so BookMenu and
    #            SkyUI inventory open it as a single-sheet Parchment Note (isNote=true), NOT a book!
    obnd = bytes(12)
    data = struct.pack("<BBHIIf", 0, 0, 0, 0xFFFFFFFF, 0, 0.0)
    subs = [
        subrec(b"EDID", zstr("TGZ_Note_CassianRumor")),
        subrec(b"OBND", obnd),
        subrec(b"FULL", zstr("Innkeeper's Note: The Moth Priest's Passage")),
        subrec(b"MODL", zstr("Clutter\\Books\\Note01.nif")),
        subrec(b"DESC", zstr(note_text)),
        subrec(b"YNAM", u32(0x000C7A54)),  # ITMNoteUpSD
        subrec(b"KSIZ", u32(1)),
        subrec(b"KWDA", u32(0x000937A2)),  # VendorItemBook
        subrec(b"DATA", data),
        subrec(b"INAM", u32(0x00097788)),  # HighPolyNote (single-page parchment note in BookMenu!)
        subrec(b"CNAM", zstr("A folded rumor note left by Brother Cassian at the Sleeping Giant Inn.")),
    ]
    return record(b"BOOK", FORM_BOOK_CASSIAN_NOTE, subs)


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


def build_pilgrimage_quest() -> bytes:
    # Native Skyrim Journal Quest: "The Moth Priest's Pilgrimage" (Side Quest, priority 25).
    # Priority 25 sits just below DialogueRiverwood_Revised (priority 30) so Orgnar tells his
    # two vanilla Riverwood rumors first, then offers our Moth Priest rumor as his 3rd rumor
    # under "Heard any rumors lately?". Stage 0 stays invisible in the Quest Journal until
    # Stage 10 starts upon receiving Brother Cassian's Note (or discovering a Canticle key).
    # All stage & objective transitions are driven natively in C++ by TrueGaze.dll (zero Papyrus).
    #
    # DNAM (12 bytes, verified against Skyrim.esm MS13 0x00039645):
    #   flags(u16)=0x0111 (kEnabled | kStartsEnabled | kRunOnce), priority(u8)=45 (fires as 3rd
    #   rumor right after DA01Intro=60 and DBEntranceQuest=50), pad(u8)=0,
    #   questDelayTime(f32)=0.0, questType(u32)=8 (kSideQuest)
    dnam = struct.pack("<HBBII", 0x0111, 45, 0, 0, 8)

    log_stage_10 = (
        "Orgnar at the Sleeping Giant Inn passed me a sealed ledger left behind by Brother "
        "Cassian, an Ancestor Moth Priest from Cyrodiil. Before reading an Elder Scroll, Moth "
        "acolytes master the First Canticle of the Eye -- the TrueGaze. To awaken full TrueGaze "
        "mastery, I must light all four keys of the First Canticle and perform the Awakening "
        "Ceremony within a Skyrim Sanctuary."
    )
    log_stage_50 = (
        "All four keys of Brother Cassian's First Canticle now stand lit. To complete the "
        "Awakening, I must stand within any Sanctuary of Skyrim wearing my Vessel, carrying the "
        "studied Canticle, and cast Sight of the First Canticle."
    )
    log_stage_100 = (
        "I have completed all four keys of the First Canticle and performed the Awakening "
        "Ceremony within a Sanctuary of Skyrim. The TrueGaze stands fully awakened."
    )
    objectives = [
        (10, "Key I (The Word): Find and study The First Canticle of the Eye in Winterhold's Arcanaeum (or an Elder Scroll tome)"),
        (20, "Key II (The Vessel): Equip a Moth Relic or Divine Amulet and hold an NPC's gaze for 3 breaths"),
        (30, "Key III (Shared Sight): Share a Ring of Quiet Sight, Silver Moth Medallion, or Moth-Silk Ribbon with a friendly companion and meet their eyes"),
        (40, "Key IV (The Pilgrimage): Hold stillness inside two Sanctuaries of Skyrim"),
        (50, "Perform the Awakening Ceremony in any Sanctuary (cast Sight of the First Canticle while wearing your Vessel)"),
    ]
    for txt in (log_stage_10, log_stage_50, log_stage_100, *(t for _, t in objectives)):
        assert all(ord(ch) < 128 for ch in txt), "Quest text must remain pure ASCII"

    subs = [
        subrec(b"EDID", zstr("TGZ_Pilgr")),  # 9 chars -> unambiguous voice prefix tgz_pilgr_
        subrec(b"FULL", zstr("The Moth Priest's Pilgrimage")),
        subrec(b"DNAM", dnam),
        subrec(b"NEXT", b""),
        # Stage 0: silent startup stage hosting Orgnar's 3rd rumor
        subrec(b"INDX", struct.pack("<HBB", 0, 0, 0)),
        subrec(b"QSDT", b"\x00"),
        # Stage 10: Pilgrimage begun (displays Quest in Journal + 4 Key objectives)
        subrec(b"INDX", struct.pack("<HBB", 10, 0, 0)),
        subrec(b"QSDT", b"\x00"),
        subrec(b"CNAM", zstr(log_stage_10)),
        # Stage 50: All four keys lit -> Final Ceremony ready
        subrec(b"INDX", struct.pack("<HBB", 50, 0, 0)),
        subrec(b"QSDT", b"\x00"),
        subrec(b"CNAM", zstr(log_stage_50)),
        # Stage 100: Quest Completed (QSDT = 0x01 = Complete Quest)
        subrec(b"INDX", struct.pack("<HBB", 100, 0, 0)),
        subrec(b"QSDT", b"\x01"),
        subrec(b"CNAM", zstr(log_stage_100)),
    ]
    for obj_idx, obj_text in objectives:
        subs.append(subrec(b"QOBJ", struct.pack("<H", obj_idx)))
        subs.append(subrec(b"FNAM", u32(0)))
        subs.append(subrec(b"NNAM", zstr(obj_text)))
    subs.append(subrec(b"ANAM", u32(0)))  # 0 aliases (zero alias-fill failure risk)
    return record(b"QUST", FORM_QUST_PILGRIMAGE, subs)


def build_inn_rumor_branches() -> list[bytes]:
    # Two top-level Dialogue Branches owned by TGZ_Pilgr:
    #   1) TGZ_InnRumorBranch (0x0100080D) -> dedicated top-level topic "Have any quiet travelers or Moth Priests passed through?"
    #   2) TGZ_RumorBranch    (0x01000810) -> shared "Heard any rumors lately?" (SNAM=RUMO) 3rd rumor topic
    b1 = record(
        b"DLBR",
        FORM_DLBR_INN_RUMOR,
        [
            subrec(b"EDID", zstr("TGZ_InnRumorBranch")),
            subrec(b"QNAM", u32(FORM_QUST_PILGRIMAGE)),
            subrec(b"TNAM", u32(0)),
            subrec(b"DNAM", u32(1)),
            subrec(b"SNAM", u32(FORM_DIAL_INN_RUMOR)),
        ],
    )
    b2 = record(
        b"DLBR",
        FORM_DLBR_RUMOR_SHARED,
        [
            subrec(b"EDID", zstr("TGZ_RumorBranch")),
            subrec(b"QNAM", u32(FORM_QUST_PILGRIMAGE)),
            subrec(b"TNAM", u32(0)),
            subrec(b"DNAM", u32(1)),
            subrec(b"SNAM", u32(FORM_DIAL_RUMOR_SHARED)),
        ],
    )
    return [b1, b2]


def build_inn_rumor_and_note_dials() -> list[bytes]:
    # Verified CTDAs from Skyrim.esm DA01IntroRumorsTopic (0x00024390) & DBRumorsTopic (0x000153B3):
    #   1) GetStage(TGZ_Pilgr 0x0100080C) < 10.0f  (op=0x80 Less Than, AND, func=0x3A=58)
    #   2) GetIsID(Orgnar 0x00013479) == 1.0f      (op=0x01 Equal | OR, func=0x48=72)
    #   3) GetIsID(Delphine 0x00013478) == 1.0f    (op=0x00 Equal, AND, func=0x48=72)
    ctda_stage_lt_10 = struct.pack(
        "<B3xfHHIIIII", 0x80, 10.0, 0x003A, 0, FORM_QUST_PILGRIMAGE, 0, 0, 0, 0xFFFFFFFF
    )
    assert len(ctda_stage_lt_10) == 32
    ctda_is_orgnar_or = bytes.fromhex(
        "010000000000803f4800000079340100000000000000000000000000ffffffff"
    )
    ctda_is_delphine = bytes.fromhex(
        "000000000000803f4800000078340100000000000000000000000000ffffffff"
    )
    enam_direct = struct.pack("<HH", 0x0200, 0)
    enam_rumor_shared = struct.pack("<HH", 0x0002, 0)  # kSayOnce (matches DA01IntroRumorsTopic 0x00024528)
    enam_note = struct.pack("<HH", 0x0200, 0)

    # 1) Dedicated Top-Level Topic on Orgnar & Delphine (SNAM = CUST, DATA = 00000000)
    #    Timed via clean-room programmatic 44100 Hz 16-bit mono PCM .wav (SEC-01: zero Bethesda BSA assets):
    #    Sound/Voice/TrueGaze_ExpandedPlay.esp/{MaleBrute,FemaleUniqueDelphine}/tgz_pilgr_tgz_innrum_0000080f_1.wav
    rumor_prompt = "Have any quiet travelers or Moth Priests passed through?"
    rumor_dial_subs = [
        subrec(b"EDID", zstr("TGZ_InnRum")),
        subrec(b"FULL", zstr(rumor_prompt)),
        subrec(b"PNAM", struct.pack("<f", 50.0)),
        subrec(b"BNAM", u32(FORM_DLBR_INN_RUMOR)),
        subrec(b"QNAM", u32(FORM_QUST_PILGRIMAGE)),
        subrec(b"DATA", bytes.fromhex("00000000")),  # Category=Topic, Subtype=Custom (top-level!)
        subrec(b"SNAM", b"CUST"),
        subrec(b"TIFC", u32(1)),
    ]
    rumor_dial_rec = record(b"DIAL", FORM_DIAL_INN_RUMOR, rumor_dial_subs)

    trdt_neutral_1 = bytes.fromhex("000000003200000000000000010000000000000001000000")
    rumor_line = (
        "A quiet Moth Priest from Cyrodiil passed through here not long ago. "
        "Left a sealed note behind for anyone asking after the Ancestor Moths."
    )
    assert len(rumor_line) <= 148 and all(ord(ch) < 128 for ch in rumor_line)
    rumor_info_subs = [
        subrec(b"ENAM", enam_direct),
        subrec(b"PNAM", u32(0)),
        subrec(b"CNAM", b"\x00"),
        subrec(b"TCLT", u32(FORM_DIAL_ALL_IN_NOTE)),  # Links to "A Moth Priest? May I see the note?"
        subrec(b"TRDT", trdt_neutral_1),
        subrec(b"NAM1", zstr(rumor_line)),
        subrec(b"NAM2", b"\x00"),
        subrec(b"NAM3", b"\x00"),
        subrec(b"CTDA", ctda_stage_lt_10),
        subrec(b"CTDA", ctda_is_orgnar_or),
        subrec(b"CTDA", ctda_is_delphine),
        subrec(b"RNAM", zstr(rumor_prompt)),
    ]
    rumor_info_rec = record(b"INFO", FORM_INFO_INN_RUMOR, rumor_info_subs)
    rumor_child_grup = (
        struct.pack("<4sIIIHH", b"GRUP", 24 + len(rumor_info_rec), FORM_DIAL_INN_RUMOR, 7, 0, 0)
        + b"\x00\x00\x00\x00"
        + rumor_info_rec
    )

    # 2) Shared "Heard any rumors lately?" 3rd Rumor Topic (SNAM = RUMO, DATA = 00000200)
    #    Matches Skyrim.esm DA01IntroRumorsTopic (0x00024390) byte-for-byte:
    #      - NO FULL subrecord on DIAL (so Creation Engine binds it to shared RUMO subtype!)
    #      - NO RNAM subrecord on INFO (so MenuTopicManager pools it into "Heard any rumors lately?")
    #    Timed via clean-room programmatic 44100 Hz 16-bit mono PCM .wav (SEC-01: zero Bethesda BSA assets):
    #    Sound/Voice/TrueGaze_ExpandedPlay.esp/{MaleBrute,FemaleUniqueDelphine}/tgz_pilgr_tgz_rumtop_00000812_1.wav
    shared_rumor_dial_subs = [
        subrec(b"EDID", zstr("TGZ_RumTop")),
        subrec(b"PNAM", struct.pack("<f", 50.0)),
        subrec(b"BNAM", u32(FORM_DLBR_RUMOR_SHARED)),
        subrec(b"QNAM", u32(FORM_QUST_PILGRIMAGE)),
        subrec(b"DATA", bytes.fromhex("00000200")),  # Category=Topic, Subtype=RUMO (2)
        subrec(b"SNAM", b"RUMO"),
        subrec(b"TIFC", u32(1)),
    ]
    shared_rumor_dial_rec = record(b"DIAL", FORM_DIAL_RUMOR_SHARED, shared_rumor_dial_subs)

    shared_rumor_info_subs = [
        subrec(b"ENAM", enam_rumor_shared),
        subrec(b"PNAM", u32(0)),
        subrec(b"CNAM", b"\x00"),
        subrec(b"TCLT", u32(FORM_DIAL_ALL_IN_NOTE)),  # Links to "A Moth Priest? May I see the note?"
        subrec(b"TRDT", trdt_neutral_1),
        subrec(b"NAM1", zstr(rumor_line)),
        subrec(b"NAM2", b"\x00"),
        subrec(b"NAM3", b"\x00"),
        subrec(b"CTDA", ctda_stage_lt_10),
        subrec(b"CTDA", ctda_is_orgnar_or),
        subrec(b"CTDA", ctda_is_delphine),
    ]
    shared_rumor_info_rec = record(b"INFO", FORM_INFO_RUMOR_SHARED, shared_rumor_info_subs)
    shared_rumor_child_grup = (
        struct.pack(
            "<4sIIIHH",
            b"GRUP",
            24 + len(shared_rumor_info_rec),
            FORM_DIAL_RUMOR_SHARED,
            7,
            0,
            0,
        )
        + b"\x00\x00\x00\x00"
        + shared_rumor_info_rec
    )

    # 3) Linked follow-up player response -> Orgnar speaks vanilla voiced "It's all in this note."
    note_prompt = "A Moth Priest? May I see the note?"
    note_dial_subs = [
        subrec(b"EDID", zstr("TGZ_AllInNote")),
        subrec(b"FULL", zstr(note_prompt)),
        subrec(b"PNAM", struct.pack("<f", 50.0)),
        subrec(b"BNAM", u32(FORM_DLBR_INN_RUMOR)),
        subrec(b"QNAM", u32(FORM_QUST_PILGRIMAGE)),
        subrec(b"DATA", bytes.fromhex("00000000")),  # Category=Topic, Subtype=Custom
        subrec(b"SNAM", b"CUST"),
        subrec(b"TIFC", u32(1)),
    ]
    note_dial_rec = record(b"DIAL", FORM_DIAL_ALL_IN_NOTE, note_dial_subs)

    note_info_subs = [
        subrec(b"ENAM", enam_note),
        subrec(b"PNAM", u32(0)),
        subrec(b"CNAM", b"\x00"),
        subrec(b"DNAM", u32(0x000DBA27)),  # Vanilla Shared Info: "It's all in this note." (MaleBrute .fuz)
        subrec(b"CTDA", ctda_stage_lt_10),
        subrec(b"CTDA", ctda_is_orgnar_or),
        subrec(b"CTDA", ctda_is_delphine),
        subrec(b"RNAM", zstr(note_prompt)),
    ]
    note_info_rec = record(b"INFO", FORM_INFO_ALL_IN_NOTE, note_info_subs)
    note_child_grup = (
        struct.pack("<4sIIIHH", b"GRUP", 24 + len(note_info_rec), FORM_DIAL_ALL_IN_NOTE, 7, 0, 0)
        + b"\x00\x00\x00\x00"
        + note_info_rec
    )

    return [
        rumor_dial_rec + rumor_child_grup,
        shared_rumor_dial_rec + shared_rumor_child_grup,
        note_dial_rec + note_child_grup,
    ]


def build_silent_timing_wav(out_path: Path, duration_sec: float = 4.8) -> None:
    # Clean 44100 Hz, 16-bit mono PCM silent timing WAV so Orgnar's Moth Priest rumor subtitle
    # stays comfortably readable in vanilla Skyrim SE (zero Fuz Ro D-oh needed; skippable on click).
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sample_rate = 44100
    num_frames = int(sample_rate * duration_sec)
    with wave.open(str(out_path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(b"\x00\x00" * num_frames)


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
    book_recs = [build_book(), build_cassian_note()]
    misc_recs = [build_misc_ribbon()]
    qust_recs = [build_pilgrimage_quest()]
    dlbr_recs = build_inn_rumor_branches()
    dial_blobs = build_inn_rumor_and_note_dials()

    # Exact canonical Skyrim.esm top-level GRUP order: MGEF, SPEL, ARMO, BOOK, MISC, DIAL, QUST, DLBR
    groups = [
        grup(b"MGEF", mgef_recs),
        grup(b"SPEL", spel_recs),
        grup(b"ARMO", armo_recs),
        grup(b"BOOK", book_recs),
        grup(b"MISC", misc_recs),
        grup(b"DIAL", dial_blobs),
        grup(b"QUST", qust_recs),
        grup(b"DLBR", dlbr_recs),
    ]

    # HEDR.numRecords = total records + GRUP headers (including nested Topic Children GRUPs + DIALs + INFOs)
    num_records = (
        len(groups)
        + len(mgef_recs)
        + len(spel_recs)
        + len(armo_recs)
        + len(book_recs)
        + len(misc_recs)
        + len(qust_recs)
        + len(dlbr_recs)
        + 9  # 3 DIAL records + 3 Topic Children GRUPs (type 7) + 3 INFO records
    )
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

    def walk_range(start_pos: int, end_pos: int) -> None:
        nonlocal grup_count
        p2 = start_pos
        while p2 < end_pos:
            tag = data[p2 : p2 + 4]
            if tag == b"GRUP":
                gs = struct.unpack_from("<I", data, p2 + 4)[0]
                grup_count += 1
                walk_range(p2 + 24, p2 + gs)
                p2 += gs
            else:
                rt, rs, rf, rform, _, _, rv, _ = struct.unpack_from("<4sIIIHHHH", data, p2)
                assert rv == SSE_FORM_VERSION
                assert 0x01000800 <= rform <= 0x01000FFF, f"FormID 0x{rform:08X} outside ESL compact range!"
                rbody = data[p2 + 24 : p2 + 24 + rs]
                p2 += 24 + rs
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
        assert p2 == end_pos, f"GRUP size mismatch at {p2} vs {end_pos}"

    walk_range(pos, len(data))
    assert hedr_count == grup_count + len(seen_records), f"HEDR count mismatch: {hedr_count} vs {grup_count + len(seen_records)}"

    print(f"Verified {path} ({len(data)} bytes) | ESL=YES | FormVer=44 | Masters={masters} | Zero Papyrus")
    for rt, fid, edid, full in seen_records:
        print(f"  {rt}  0x{fid:08X}  {edid:28s}  '{full}'")


def build_seq_file(out_path: Path, quest_form_ids: list[int]) -> None:
    # Verified directly against SkyrimSE.exe 1.7.104.0 LoadSEQFile (0x1401C7EA0) and Bethesda's
    # ccbgssse005-goldbrand.seq: a flat array of 4-byte little-endian raw disk FormIDs (including
    # the local master index byte, e.g., 0x0100080C). LoadSEQFile remaps each FormID via
    # TESForm::ResolveFileFormID (0x1401E6D00) into the global SEQ hash table [0x142077E78] so
    # TESDataHandler::ConstructObjectList (0x1401C0D77) enters GRUP(type=7) and loads all INFOs!
    out_path.parent.mkdir(parents=True, exist_ok=True)
    blob = b"".join(struct.pack("<I", fid) for fid in quest_form_ids)
    out_path.write_bytes(blob)
    assert out_path.read_bytes() == blob and len(blob) == 4 * len(quest_form_ids)


def compute_reading_duration_sec(subtitle_text: str) -> float:
    # Comfortable reading cadence (~15 chars/sec + 0.8s margin), bounded [3.5s, 9.5s].
    return max(3.5, min(9.5, (len(subtitle_text) / 15.0) + 0.8))


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    out_path = repo_root / "skyrim" / "TrueGaze_ExpandedPlay.esp"
    build_plugin(out_path)
    verify_plugin(out_path)

    seq_path = repo_root / "skyrim" / "SEQ" / "TrueGaze_ExpandedPlay.seq"
    build_seq_file(seq_path, [FORM_QUST_PILGRIMAGE])
    print(f"Generated SEQ file: {seq_path} ({seq_path.stat().st_size} bytes, quest=0x{FORM_QUST_PILGRIMAGE:08X})")

    # SEC-01: Generate clean-room programmatic 44.1 kHz 16-bit mono PCM timing .wav files
    # (zero Bethesda BSA extraction; GPL-3.0 & EULA compliant; exact subtitle reading duration
    # with zero mismatched vanilla voice audio; works identically on SE, AE, and VR).
    rumor_text = (
        "A quiet Moth Priest from Cyrodiil passed through here not long ago. "
        "Left a sealed note behind for anyone asking after the Ancestor Moths."
    )
    duration_sec = compute_reading_duration_sec(rumor_text)
    voice_types = ("MaleBrute", "FemaleUniqueDelphine")
    voice_targets = (
        "tgz_pilgr_tgz_innrum_0000080f_1",
        "tgz_pilgr_tgz_rumtop_00000812_1",
    )
    voice_root = repo_root / "skyrim" / "Sound" / "Voice" / "TrueGaze_ExpandedPlay.esp"
    for vtype in voice_types:
        vdir = voice_root / vtype
        vdir.mkdir(parents=True, exist_ok=True)
        for old_file in vdir.glob("*"):
            old_file.unlink()
        for stem in voice_targets:
            wav_path = vdir / f"{stem}.wav"
            build_silent_timing_wav(wav_path, duration_sec=duration_sec)
            print(
                f"Generated clean-room timing WAV: {wav_path} "
                f"({wav_path.stat().st_size} bytes, {duration_sec:.2f}s)"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
