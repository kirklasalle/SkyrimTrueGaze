#!/usr/bin/env python3
"""
Generate high-detail, publication-grade technical illustrations for TrueGaze Expanded Play.
Renders at 2x resolution (3840x2160) and downsamples with Lanczos filtering to 1920x1080
using real Windows TrueType typography, exact C++ specification data, mathematical plots,
and authentic Skyrim in-engine screenshots from docs/screengrabs/.
"""

from __future__ import annotations

import math
from pathlib import Path
from typing import Iterable, Tuple

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
IMAGES_DIR = ROOT / "docs" / "images"
SCREENGRABS_DIR = ROOT / "docs" / "screengrabs"

S = 2  # 2x supersampling factor (3840x2160 -> 1920x1080)
W = 1920 * S
H = 1080 * S

FONTS_DIR = Path("C:/Windows/Fonts")


def load_font(name: str, px_1x: float) -> ImageFont.FreeTypeFont:
    path = FONTS_DIR / name
    return ImageFont.truetype(str(path), int(round(px_1x * S)))


F_KICKER = load_font("consolab.ttf", 12)
F_TITLE = load_font("georgiab.ttf", 27)
F_SUB = load_font("segoeui.ttf", 13.5)
F_PANEL = load_font("segoeuib.ttf", 14.5)
F_H2 = load_font("segoeuib.ttf", 13)
F_BODY = load_font("segoeui.ttf", 11.5)
F_BODY_B = load_font("segoeuib.ttf", 11.5)
F_SMALL = load_font("segoeui.ttf", 10)
F_MONO = load_font("consola.ttf", 10.5)
F_MONO_B = load_font("consolab.ttf", 10.5)
F_WHISPER = load_font("georgiai.ttf", 15)
F_WHISPER_REG = load_font("georgia.ttf", 13.5)

# Color tokens (dark slate blueprint aesthetic matching TrueGazeConfig UI)
BG = (11, 15, 21)
GRID = (24, 32, 44)
PANEL_BG = (18, 24, 34)
CARD_BG = (23, 30, 43)
CARD_ALT = (16, 21, 30)
BORDER = (49, 63, 84)
BRASS = (212, 180, 117)
BRASS_DIM = (122, 104, 68)
IVORY = (237, 241, 247)
SLATE = (162, 176, 196)
MUTED = (114, 129, 148)
CYAN = (76, 201, 240)
GREEN = (51, 255, 89)
TEAL = (64, 212, 180)
ORANGE = (255, 153, 38)
CRIMSON = (240, 82, 92)
PURPLE = (205, 132, 247)


def sc(v: float) -> int:
    return int(round(v * S))


def new_canvas(kicker: str, title: str, subtitle: str, footer_left: str, footer_right: str):
    img = Image.new("RGBA", (W, H), BG + (255,))
    draw = ImageDraw.Draw(img, "RGBA")

    # Subtle architectural drafting grid
    step = sc(48)
    for x in range(step, W, step):
        draw.line([(x, 0), (x, H)], fill=GRID + (140,), width=1)
    for y in range(step, H, step):
        draw.line([(0, y), (W, y)], fill=GRID + (140,), width=1)

    # Outer frame with chamfered brass corner accents
    m = sc(18)
    draw.rectangle([m, m, W - m, H - m], outline=BORDER + (255,), width=2)
    m2 = sc(24)
    draw.rectangle([m2, m2, W - m2, H - m2], outline=(35, 45, 60, 255), width=1)
    corner = sc(18)
    for cx, cy, dx, dy in [
        (m, m, 1, 1),
        (W - m, m, -1, 1),
        (m, H - m, 1, -1),
        (W - m, H - m, -1, -1),
    ]:
        draw.line([(cx, cy), (cx + dx * corner, cy)], fill=BRASS, width=4)
        draw.line([(cx, cy), (cx, cy + dy * corner)], fill=BRASS, width=4)

    # Header band
    draw.text((sc(44), sc(36)), kicker, font=F_KICKER, fill=BRASS)
    draw.text((sc(44), sc(56)), title, font=F_TITLE, fill=IVORY)
    draw.text((sc(44), sc(92)), subtitle, font=F_SUB, fill=SLATE)
    draw.line([(sc(44), sc(116)), (W - sc(44), sc(116))], fill=BRASS_DIM, width=2)

    # Footer bar
    draw.line([(sc(44), H - sc(44)), (W - sc(44), H - sc(44))], fill=BORDER, width=1)
    draw.text((sc(44), H - sc(36)), footer_left, font=F_MONO, fill=MUTED)
    bbox = draw.textbbox((0, 0), footer_right, font=F_MONO)
    tw = bbox[2] - bbox[0]
    draw.text((W - sc(44) - tw, H - sc(36)), footer_right, font=F_MONO, fill=MUTED)
    return img, draw


def draw_panel(
    draw: ImageDraw.ImageDraw,
    x: float,
    y: float,
    w: float,
    h: float,
    title: str,
    tag: str = "",
) -> Tuple[int, int, int, int]:
    px, py, pw, ph = sc(x), sc(y), sc(w), sc(h)
    draw.rounded_rectangle([px, py, px + pw, py + ph], radius=sc(6), fill=PANEL_BG + (255,), outline=BORDER + (255,), width=2)
    # Header strip
    hh = sc(34)
    draw.rounded_rectangle([px, py, px + pw, py + hh], radius=sc(6), fill=(26, 34, 48, 255))
    draw.rectangle([px, py + hh - sc(6), px + pw, py + hh], fill=(26, 34, 48, 255))
    draw.line([(px, py + hh), (px + pw, py + hh)], fill=BRASS_DIM, width=2)
    draw.rectangle([px + sc(12), py + sc(11), px + sc(16), py + sc(23)], fill=BRASS)
    draw.text((px + sc(24), py + sc(7)), title, font=F_PANEL, fill=IVORY)
    if tag:
        tb = draw.textbbox((0, 0), tag, font=F_MONO_B)
        tw = tb[2] - tb[0]
        draw.text((px + pw - tw - sc(14), py + sc(10)), tag, font=F_MONO_B, fill=BRASS)
    return px, py + hh, pw, ph - hh


def blend_rgb(bg: Tuple[int, int, int], fg: Tuple[int, int, int], alpha: float) -> Tuple[int, int, int]:
    return (
        int(round(bg[0] * (1.0 - alpha) + fg[0] * alpha)),
        int(round(bg[1] * (1.0 - alpha) + fg[1] * alpha)),
        int(round(bg[2] * (1.0 - alpha) + fg[2] * alpha)),
    )


def draw_wrapped(
    draw: ImageDraw.ImageDraw,
    x: int,
    y: int,
    text: str,
    font: ImageFont.FreeTypeFont,
    fill: Tuple[int, ...],
    max_width_px: int,
    line_gap: int = 8,
) -> int:
    lines = []
    for paragraph in text.split("\n"):
        words = paragraph.split(" ")
        cur = ""
        for w in words:
            cand = w if not cur else f"{cur} {w}"
            bb = draw.textbbox((0, 0), cand, font=font)
            if bb[2] - bb[0] <= max_width_px:
                cur = cand
            else:
                if cur:
                    lines.append(cur)
                cur = w
        if cur:
            lines.append(cur)

    cy = y
    sample_bb = draw.textbbox((0, 0), "Ag", font=font)
    lh = (sample_bb[3] - sample_bb[1]) + line_gap
    for ln in lines:
        draw.text((x, cy), ln, font=font, fill=fill)
        cy += lh
    return cy


def draw_shape_glyph(
    draw: ImageDraw.ImageDraw,
    cx: int,
    cy: int,
    r: int,
    shape: str,
    color: Tuple[int, int, int],
    width: int = 3,
):
    if shape == "Diamond":
        pts = [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)]
        draw.polygon(pts, outline=color + (255,), fill=color + (45,))
        draw.line(pts + [pts[0]], fill=color + (255,), width=width)
    elif shape == "Triangle":
        pts = [(cx, cy - r), (cx + r, cy + r), (cx - r, cy + r)]
        draw.polygon(pts, outline=color + (255,), fill=color + (45,))
        draw.line(pts + [pts[0]], fill=color + (255,), width=width)
    elif shape == "Square":
        draw.rectangle([cx - r, cy - r, cx + r, cy + r], outline=color + (255,), fill=color + (45,), width=width)
    else:
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=color + (255,), fill=color + (45,), width=width)


def draw_callout_leader(
    draw: ImageDraw.ImageDraw,
    p_target: Tuple[int, int],
    p_elbow: Tuple[int, int],
    p_label: Tuple[int, int],
    label_num: str,
    accent: Tuple[int, int, int] = BRASS,
):
    draw.line([p_target, p_elbow, p_label], fill=accent + (235,), width=3)
    r_dot = sc(4)
    draw.ellipse([p_target[0] - r_dot, p_target[1] - r_dot, p_target[0] + r_dot, p_target[1] + r_dot], fill=accent)
    r_badge = sc(11)
    bx, by = p_label
    draw.ellipse([bx - r_badge, by - r_badge, bx + r_badge, by + r_badge], fill=(16, 22, 31, 245), outline=accent, width=3)
    bb = draw.textbbox((0, 0), label_num, font=F_MONO_B)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    draw.text((bx - tw // 2, by - th // 2 - sc(2)), label_num, font=F_MONO_B, fill=IVORY)


def save_downsampled(img: Image.Image, out_path: Path):
    final = img.convert("RGB").resize((1920, 1080), Image.Resampling.LANCZOS)
    final.save(out_path, format="PNG", optimize=True)
    print(f"Generated {out_path.relative_to(ROOT)} ({final.size[0]}x{final.size[1]})")


# ==============================================================================
# PLATE 1: HUD RETICLE OVER REAL SKYRIM FRAME + HCEP-02 PALETTE + DWELL TIMELINE
# ==============================================================================
def build_plate_1():
    img, draw = new_canvas(
        "TRUEGAZE EXPANDED PLAY  ·  TECHNICAL PLATE 01  ·  UI & PERCEPTION SPECIFICATION",
        "First-Person Gaze Reticle, HCEP-02 Region Palette & Dwell Escalation",
        "Composited over an authentic Riverwood Trader capture (ScreenShot95.png) using exact GazeReticle.hpp & ExpandedPlayTypes.hpp constants",
        "Sources: src/ExpandedPlay/GazeReticle.hpp/.cpp  |  src/ExpandedPlay/ExpandedPlayTypes.hpp",
        "1920x1080  ·  Clean Rendering / Zero External Assets",
    )

    # Left panel: Real in-game capture + vector HUD overlay + 4 callout cards
    lx, ly, lw, lh = draw_panel(
        draw,
        44,
        130,
        966,
        890,
        "IN-ENGINE COMPOSITE: 1ST-PERSON RETICLE, HYBRID BADGE & DIEGETIC WHISPERS",
        "Skyrim AE + TrueGazeReticle.swf",
    )

    shot_path = SCREENGRABS_DIR / "ScreenShot95.png"
    raw_shot = Image.open(shot_path).convert("RGBA")
    # Crop symmetrically around (720, 405) to remove top-left screenshot toast while keeping 16:9
    cropped = raw_shot.crop((160, 90, 1280, 720))  # 1120x630 (16:9)
    frame_w = sc(926)
    frame_h = sc(521)
    frame_x = lx + (lw - frame_w) // 2
    frame_y = ly + sc(18)
    scaled_shot = cropped.resize((frame_w, frame_h), Image.Resampling.LANCZOS)
    img.paste(scaled_shot, (frame_x, frame_y))
    draw = ImageDraw.Draw(img, "RGBA")
    draw.rectangle([frame_x, frame_y, frame_x + frame_w, frame_y + frame_h], outline=BRASS_DIM + (255,), width=3)

    # Overlay translucent HUD elements via true alpha composition over the Skyrim frame
    hud_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    hd = ImageDraw.Draw(hud_layer, "RGBA")

    cx = frame_x + frame_w // 2
    cy = frame_y + frame_h // 2

    # Soft reciprocity pulse halo (golden)
    for r_p, alpha in [(sc(38), 110), (sc(44), 170)]:
        hd.ellipse([cx - r_p, cy - r_p, cx + r_p, cy + r_p], outline=(255, 217, 102, alpha), width=3)

    # Outer dwell background track + 4 threshold ticks (Look 0.6s, Gaze 2.0s, Stare 4.5s, Fix 9.0s)
    r_arc = sc(29)
    hd.ellipse([cx - r_arc, cy - r_arc, cx + r_arc, cy + r_arc], outline=(200, 210, 225, 110), width=4)
    for frac in (0.25, 0.50, 0.75, 1.0):
        ang = -math.pi / 2 + frac * 2 * math.pi
        x1 = cx + (r_arc - sc(5)) * math.cos(ang)
        y1 = cy + (r_arc - sc(5)) * math.sin(ang)
        x2 = cx + (r_arc + sc(5)) * math.cos(ang)
        y2 = cy + (r_arc + sc(5)) * math.sin(ang)
        hd.line([(x1, y1), (x2, y2)], fill=(235, 240, 248, 220), width=3)

    # Filled Dwell Arc (58% progress, Gaze band, LeftEye green #33FF59)
    arc_box = [cx - r_arc, cy - r_arc, cx + r_arc, cy + r_arc]
    hd.arc(arc_box, start=-90, end=-90 + int(360 * 0.58), fill=GREEN + (245,), width=7)

    # Central accessibility glyph (Diamond for LeftEye/RightEye) + magnetism vector preview
    draw_shape_glyph(hd, cx, cy, sc(12), "Diamond", GREEN, width=4)
    hd.line([(cx + sc(16), cy - sc(6)), (cx + sc(32), cy - sc(14))], fill=GREEN + (220,), width=3)

    # Top-right 3rd-Person / Active Demeanor Badge
    bx1, by1, bx2, by2 = frame_x + frame_w - sc(330), frame_y + sc(18), frame_x + frame_w - sc(18), frame_y + sc(66)
    hd.rounded_rectangle([bx1, by1, bx2, by2], radius=sc(5), fill=(12, 16, 22, 215), outline=BRASS + (235,), width=2)
    hd.text((bx1 + sc(14), by1 + sc(8)), "DEMEANOR: COURTEOUS", font=F_BODY_B, fill=BRASS + (255,))
    hd.text((bx1 + sc(14), by1 + sc(26)), "HYBRID [AUTO]  ·  PIN IDLE", font=F_MONO, fill=SLATE + (255,))

    # Lower-center Telltale-style Whisper banner
    wh_text = "Camilla Valerius noted your steady eyes."
    wb = hd.textbbox((0, 0), wh_text, font=F_WHISPER)
    ww = (wb[2] - wb[0]) + sc(48)
    wx1 = cx - ww // 2
    wy1 = frame_y + frame_h - sc(64)
    hd.rounded_rectangle([wx1, wy1, wx1 + ww, wy1 + sc(42)], radius=sc(5), fill=(10, 14, 20, 220), outline=(230, 216, 184, 195), width=2)
    hd.text((wx1 + sc(24), wy1 + sc(9)), wh_text, font=F_WHISPER, fill=(230, 216, 184, 255))

    img = Image.alpha_composite(img, hud_layer)
    draw = ImageDraw.Draw(img, "RGBA")

    # Crisp numbered callout leaders on screenshot
    draw_callout_leader(draw, (cx - sc(26), cy - sc(20)), (frame_x + sc(210), cy - sc(80)), (frame_x + sc(80), cy - sc(80)), "1", GREEN)
    draw_callout_leader(draw, (cx + sc(34), cy + sc(26)), (cx + sc(180), cy + sc(58)), (cx + sc(290), cy + sc(58)), "2", (255, 217, 102))
    draw_callout_leader(draw, (bx1, by1 + sc(24)), (bx1 - sc(46), by1 + sc(24)), (bx1 - sc(90), by1 + sc(24)), "3", BRASS)
    draw_callout_leader(draw, (wx1, wy1 + sc(21)), (wx1 - sc(60), wy1 + sc(21)), (frame_x + sc(80), wy1 + sc(21)), "4", (230, 216, 184))

    # 4 Callout Explanation Cards below screenshot
    cards = [
        (
            "1",
            "Region-Tinted Glyph & Magnetism",
            GREEN,
            "Renders semantic color + unique vector shape (Diamond=Eye, Circle=Mouth/Chin, Triangle=Forehead/Periph, Square=Torso/Ground) with optional <=2 deg gentle magnetism (disabled in combat/weapon drawn).",
        ),
        (
            "2",
            "4-Tick Dwell Arc & Reciprocity Pulse",
            (255, 217, 102),
            "Progress ring fills cleanly across Look (0.6s), Gaze (2.0s), Stare (4.5s), and Fixation (9.0s). When NPC gazes back simultaneously, ReciprocityPulseState fires a soft 0.45s gold halo.",
        ),
        (
            "3",
            "3rd-Person Hybrid Demeanor Badge",
            BRASS,
            "Displays active posture (Courteous/Warm/Stern/Evasive/Deferential/Predatory) and whether selection is Auto-inferred or 60s manual pin (clears automatically when location/combat/dialogue changes).",
        ),
        (
            "4",
            "Diegetic Whispers & Menu Safety",
            (230, 216, 184),
            "Rung-4 non-verbal feedback (rate-limited >=20s per NPC) plus BookStudy & First Canticle alerts. Auto-hides under Inventory, Barter, Map, Book, or Main Menu via DiagnosticMenuPolicy.",
        ),
    ]
    cw = sc(454)
    ch = sc(142)
    gap_x = sc(18)
    gap_y = sc(16)
    base_y = frame_y + frame_h + sc(20)
    for idx, (num, ctitle, col, cdesc) in enumerate(cards):
        col_i = idx % 2
        row_i = idx // 2
        cx0 = frame_x + col_i * (cw + gap_x)
        cy0 = base_y + row_i * (ch + gap_y)
        draw.rounded_rectangle([cx0, cy0, cx0 + cw, cy0 + ch], radius=sc(5), fill=CARD_BG, outline=BORDER, width=2)
        draw.rectangle([cx0, cy0 + sc(10), cx0 + sc(4), cy0 + ch - sc(10)], fill=col)
        # Badge
        rb = sc(10)
        draw.ellipse([cx0 + sc(16), cy0 + sc(12), cx0 + sc(16) + 2 * rb, cy0 + sc(12) + 2 * rb], fill=(14, 18, 25), outline=col, width=2)
        bb = draw.textbbox((0, 0), num, font=F_MONO_B)
        draw.text((cx0 + sc(16) + rb - (bb[2] - bb[0]) // 2, cy0 + sc(12) + rb - (bb[3] - bb[1]) // 2 - sc(2)), num, font=F_MONO_B, fill=IVORY)
        draw.text((cx0 + sc(44), cy0 + sc(13)), ctitle, font=F_H2, fill=IVORY)
        draw_wrapped(draw, cx0 + sc(16), cy0 + sc(42), cdesc, F_SMALL, SLATE, cw - sc(28), line_gap=5)

    # Right panel: 13-region table, Color-blind modes, Dwell band timeline
    rx, ry, rw, rh = draw_panel(
        draw,
        1030,
        130,
        846,
        890,
        "HCEP-02 REGION SPECIFICATION, COLOR-BLIND PALETTES & DWELL MATRIX",
        "13 Regions · 4 Palettes",
    )

    # Sub-header: 13-Region Table
    tx, ty, tw = rx + sc(18), ry + sc(14), rw - sc(36)
    draw.text((tx, ty), "Canonical HCEP-02 Gaze Regions (GazeReticle.hpp & InterpretationEngine.hpp)", font=F_H2, fill=BRASS)
    ty += sc(26)

    # Table column header
    row_h = sc(32)
    draw.rectangle([tx, ty, tx + tw, ty + sc(26)], fill=(26, 34, 48))
    draw.text((tx + sc(10), ty + sc(5)), "ID", font=F_MONO_B, fill=MUTED)
    draw.text((tx + sc(52), ty + sc(5)), "REGION", font=F_MONO_B, fill=MUTED)
    draw.text((tx + sc(215), ty + sc(5)), "HEX", font=F_MONO_B, fill=MUTED)
    draw.text((tx + sc(315), ty + sc(5)), "GLYPH", font=F_MONO_B, fill=MUTED)
    draw.text((tx + sc(430), ty + sc(5)), "INTERPRETATION MAPPING (CONTEXT-GATED)", font=F_MONO_B, fill=MUTED)
    ty += sc(28)

    regions = [
        ("0", "LeftEye", (51, 255, 89), "#33FF59", "Diamond", "Respect -> Challenge (unknown) | Warmth / Attraction (close)"),
        ("1", "RightEye", (255, 153, 38), "#FF9926", "Diamond", "Same face-eye reciprocity; paired with LeftEye social anchor"),
        ("2", "Mouth", (255, 89, 191), "#FF59BF", "Circle", "Warmth (Friend+), Attraction (Spouse), Contempt (Rival); None >8m"),
        ("3", "Forehead", (217, 102, 255), "#D966FF", "Triangle", "Clinical / evaluative face scan; follows eye dwell escalation"),
        ("4", "Chin", (230, 191, 153), "#E6BF99", "Circle", "Deference for Superior/Jarl court >=Look; else neutral/face"),
        ("5", "Torso", (255, 38, 51), "#FF2633", "Square", "Compassion / Recognition first; IntrusionLeer ONLY if adult+opt-in"),
        ("6/7", "Right / LeftHand", (230, 216, 184), "#E6D8B8", "Circle", "Suspicion at >=Gaze (weapon/merchant counter inspection)"),
        ("8", "Ground", (38, 89, 230), "#2659E6", "Square", "Deference toward Superior >=Look (yielded / respectful posture)"),
        ("9", "ULPeripheral", (51, 204, 255), "#33CCFF", "Triangle", "CGA upper-left positivity aversion (no false accusation)"),
        ("10", "URPeripheral", (102, 242, 255), "#66F2FF", "Triangle", "CGA upper-right memory/thought construction aversion"),
        ("11", "LLPeripheral", (166, 179, 204), "#A6B3CC", "Triangle", "Contempt on Stranger/Hostile >=Look (dismissive look-away)"),
        ("12", "LRPeripheral", (128, 128, 166), "#8080A6", "Triangle", "Deceit while speaking claim (>=Look); Suspicion otherwise (>=Gaze)"),
    ]

    for i, (rid, rname, rgb, hx, shp, desc) in enumerate(regions):
        bg_row = CARD_BG if i % 2 == 0 else CARD_ALT
        draw.rectangle([tx, ty, tx + tw, ty + row_h], fill=bg_row)
        draw.text((tx + sc(10), ty + sc(8)), rid, font=F_MONO_B, fill=IVORY)
        draw.text((tx + sc(52), ty + sc(8)), rname, font=F_BODY_B, fill=IVORY)
        # Swatch
        swx = tx + sc(215)
        draw.rounded_rectangle([swx, ty + sc(8), swx + sc(16), ty + sc(24)], radius=sc(3), fill=rgb, outline=(255, 255, 255, 80), width=1)
        draw.text((swx + sc(24), ty + sc(8)), hx, font=F_MONO, fill=SLATE)
        # Shape glyph
        gx = tx + sc(330)
        gy = ty + row_h // 2
        draw_shape_glyph(draw, gx, gy, sc(7), shp, rgb, width=2)
        draw.text((gx + sc(16), ty + sc(8)), shp, font=F_SMALL, fill=SLATE)
        # Meaning
        draw.text((tx + sc(430), ty + sc(8)), desc, font=F_SMALL, fill=SLATE)
        ty += row_h

    # Color-blind accessible palettes section
    ty += sc(18)
    draw.text((tx, ty), "Accessibility Palettes ([ExpandedPlay] iColorBlindMode = 0..3)", font=F_H2, fill=BRASS)
    ty += sc(26)
    palettes = [
        ("0: Normal (HCEP-02)", [(51, 255, 89), (255, 153, 38), (255, 89, 191), (217, 102, 255), (230, 191, 153), (255, 38, 51), (38, 89, 230), (51, 204, 255)]),
        ("1: Deuteranopia", [(0, 158, 115), (230, 159, 0), (204, 121, 167), (86, 180, 233), (240, 228, 66), (213, 94, 0), (0, 114, 178), (136, 204, 238)]),
        ("2: Protanopia", [(0, 114, 178), (240, 228, 66), (86, 180, 233), (204, 121, 167), (230, 159, 0), (213, 94, 0), (51, 34, 136), (136, 204, 238)]),
        ("3: Tritanopia", [(0, 180, 160), (255, 110, 84), (255, 153, 204), (255, 255, 255), (255, 208, 166), (230, 46, 77), (61, 109, 153), (102, 224, 216)]),
    ]
    for pname, cols in palettes:
        draw.rounded_rectangle([tx, ty, tx + tw, ty + sc(32)], radius=sc(4), fill=CARD_BG, outline=BORDER, width=1)
        draw.text((tx + sc(12), ty + sc(8)), pname, font=F_MONO_B, fill=IVORY)
        sx0 = tx + sc(220)
        sw_w = (tw - sc(236)) // len(cols)
        for ci, c in enumerate(cols):
            x_a = sx0 + ci * sw_w
            draw.rounded_rectangle([x_a, ty + sc(6), x_a + sw_w - sc(8), ty + sc(26)], radius=sc(3), fill=c)
        ty += sc(38)

    # Dwell Band Timeline
    ty += sc(14)
    draw.text((tx, ty), "Continuous Dwell Classification Timeline (ClassifyDwellBand)", font=F_H2, fill=BRASS)
    ty += sc(26)

    bar_h = sc(44)
    bands = [
        ("None", "<0.08s", 0.11, (60, 72, 90), "0.00"),
        ("Glance", "0.08-0.60s", 0.15, (94, 129, 172), "0.20"),
        ("Look", "0.60-2.00s", 0.21, (76, 201, 240), "0.40"),
        ("Gaze", "2.00-4.50s", 0.21, (51, 255, 89), "0.60"),
        ("Stare", "4.50-9.00s", 0.18, (255, 153, 38), "0.80"),
        ("Fixation", ">=9.00s", 0.14, (240, 82, 92), "1.00"),
    ]
    bx = tx
    for bname, brange, bfrac, bcol, bstr in bands:
        seg_w = int(tw * bfrac)
        draw.rectangle([bx, ty, bx + seg_w - sc(4), ty + bar_h], fill=blend_rgb(CARD_ALT, bcol, 0.16), outline=bcol, width=2)
        draw.text((bx + sc(8), ty + sc(6)), bname, font=F_BODY_B, fill=IVORY)
        draw.text((bx + sc(8), ty + sc(23)), brange, font=F_SMALL, fill=SLATE)
        draw.text((bx + seg_w - sc(38), ty + sc(6)), bstr, font=F_MONO_B, fill=bcol)
        bx += seg_w

    ty += bar_h + sc(10)
    draw.rounded_rectangle([tx, ty, tx + tw, ty + sc(52)], radius=sc(4), fill=blend_rgb(CARD_ALT, CRIMSON, 0.10), outline=CRIMSON, width=1)
    draw.text((tx + sc(12), ty + sc(8)), "Torso Mature Escalation Rule (bModuleMatureReactions=true & !isChild & !compassionOrEmblem):", font=F_BODY_B, fill=CRIMSON)
    draw.text((tx + sc(12), ty + sc(28)), "Shifts 1 band faster (Look >= 0.45s, Gaze >= 1.50s, Stare/IntrusionLeer >= 3.00s). Disabled by default (false).", font=F_SMALL, fill=SLATE)

    save_downsampled(img, IMAGES_DIR / "expanded_play_hud_and_regions.png")


# ==============================================================================
# PLATE 2: HOW TO ENABLE / DISABLE + ARCHITECTURE PIPELINE + CO-SAVE & RUNG 1-4
# ==============================================================================
def build_plate_2():
    img, draw = new_canvas(
        "TRUEGAZE EXPANDED PLAY  ·  TECHNICAL PLATE 02  ·  CONTROLS & ARCHITECTURE",
        "Control Surfaces (Enable / Disable) & 10 Hz / 2 Hz Native C++23 Pipeline",
        "Zero-ESP layer isolation, reversible ActorValue modifiers, hard child-safety gate, and 4-rung response dispatch",
        "Sources: ConfigManager.hpp/.cpp  |  ExpandedPlayManager.hpp/.cpp  |  ResponseLadder.hpp/.cpp",
        "1920x1080  ·  Clean Rendering / Verified Code Contracts",
    )

    # LEFT COLUMN: HOW TO ENABLE / DISABLE
    lx, ly, lw, lh = draw_panel(
        draw,
        44,
        130,
        850,
        890,
        "HOW TO ENABLE, TUNE & SAFELY DISABLE TRUEGAZE",
        "Two Independent Layers",
    )

    cy = ly + sc(16)
    cx = lx + sc(18)
    cw = lw - sc(36)

    # Top Layer Split Banner
    draw.rounded_rectangle([cx, cy, cx + cw, cy + sc(68)], radius=sc(5), fill=CARD_BG, outline=BRASS, width=2)
    draw.text((cx + sc(14), cy + sc(8)), "SHIPPED DEFAULT SAFETY CONTRACT: Core Gaze = ON  |  Expanded Play = OFF", font=F_H2, fill=BRASS)
    draw_wrapped(
        draw,
        cx + sc(14),
        cy + sc(30),
        "With [ExpandedPlay] bEnabled=false, TrueGaze.dll performs an immediate early-return before touching Perception, Reticle, Ledger, Progression, or ActorValues—preserving 100% v1.1.0 visual/kinematics parity at zero cost.",
        F_SMALL,
        SLATE,
        cw - sc(28),
        line_gap=4,
    )
    cy += sc(82)

    # Surface 1: In-Game Console
    draw.text((cx, cy), "METHOD 1 — IN-GAME CONSOLE (~ key, instant live toggle + INI persist)", font=F_H2, fill=CYAN)
    cy += sc(24)
    cmds = [
        ("stg", "Toggle Core Kinematics engine (bEnableTrueGaze on/off)."),
        ("stgexpanded", "Toggle Expanded Play (bEnabled); if NPC selected & active, starts/aborts Staring Contest!"),
        ("stgdemeanor", "Cycle 3rd-person Demeanor (Courteous..Predatory); pins 60s in Hybrid mode."),
        ("stgoff / stgvisuals", "Hide or toggle dev-only 3D laser rays and HCEP floating diagram panels."),
        ("stgstatus / stgreload", "Inspect live Expanded Play counters or reload edits from TrueGaze.ini."),
    ]
    for cmd, cdesc in cmds:
        draw.rounded_rectangle([cx, cy, cx + cw, cy + sc(32)], radius=sc(4), fill=CARD_ALT, outline=BORDER, width=1)
        draw.text((cx + sc(12), cy + sc(8)), cmd, font=F_MONO_B, fill=GREEN)
        draw.text((cx + sc(200), cy + sc(8)), cdesc, font=F_SMALL, fill=SLATE)
        cy += sc(37)

    # Surface 2: HTML5 Configurator & TrueGaze.ini
    cy += sc(8)
    draw.text((cx, cy), "METHOD 2 — STANDALONE HTML5 CONFIGURATOR OR TrueGaze.ini", font=F_H2, fill=CYAN)
    cy += sc(24)
    draw.text((cx, cy), "Launch  Launch-TrueGazeConfig.cmd  (or edit  Data\\SKSE\\Plugins\\TrueGaze.ini  directly):", font=F_SMALL, fill=IVORY)
    cy += sc(22)

    ini_box_h = sc(332)
    draw.rounded_rectangle([cx, cy, cx + cw, cy + ini_box_h], radius=sc(5), fill=(12, 16, 22), outline=BORDER, width=2)
    ini_lines = [
        ("[General]", BRASS, "Master Kinematics Switch"),
        ("bEnableTrueGaze=true", GREEN, "false disables all procedural eye/head motion"),
        ("bEnableCreatures=true", SLATE, "Include non-humanoid rigs"),
        ("", MUTED, ""),
        ("[ExpandedPlay]", BRASS, "Gameplay Layer (Independent Sub-Modules)"),
        ("bEnabled=false", ORANGE, "Change to 'true' (or type stgexpanded) to activate"),
        ("sDemeanorMode=Hybrid", CYAN, "Manual | Auto | Hybrid (Auto + 60s manual pin)"),
        ("bModuleSocialReactions=true", TEAL, "Rungs 1-4, Whispers, Eye-Contact Greeting, Contests"),
        ("bModuleRelationshipMemory=true", TEAL, "512-NPC LRU GazeLedger + Spouse/Triangle witnesses"),
        ("bModuleBookStudy=true", TEAL, "BookStudyTracker + Moth's Stillness RPG stats"),
        ("bModulePresenceProgression=true", TEAL, "Presence 0..100 + 4-Axis Signature Titles"),
        ("bModuleMatureReactions=false", CRIMSON, "Opt-in adult disambiguation (children ALWAYS excluded)"),
        ("bEnableReticle=true", SLATE, "1st-person TrueGazeReticle.swf HUD overlay"),
        ("bImmersiveHideReticle=false", SLATE, "Hide HUD reticle & keep whispers + reactions"),
        ("bEnableEyeContactInitiate=true", SLATE, "Open dialogue after >=2.0s mutual Respect/Warmth"),
        ("bEmitAiNpcBridgeEvents=true", SLATE, "Emit TrueGaze_OnGazeInterpreted ModEvent & log"),
    ]
    iy = cy + sc(10)
    for k, kcol, kcomment in ini_lines:
        if not k:
            iy += sc(6)
            continue
        draw.text((cx + sc(14), iy), k, font=F_MONO_B if "=" not in k else F_MONO, fill=kcol)
        if kcomment:
            draw.text((cx + sc(315), iy), "; " + kcomment, font=F_SMALL, fill=MUTED)
        iy += sc(20)
    cy += ini_box_h + sc(16)

    # Surface 3: Mid-Save Removal & Clean State Contract
    draw.rounded_rectangle([cx, cy, cx + cw, cy + sc(158)], radius=sc(5), fill=CARD_BG, outline=TEAL, width=2)
    draw.text((cx + sc(14), cy + sc(10)), "METHOD 3 — CLEAN MID-SAVE DISABLE & MOD REMOVAL SAFETY", font=F_H2, fill=TEAL)
    draw_wrapped(
        draw,
        cx + sc(14),
        cy + sc(36),
        "• Zero ESP/ESM & Zero Baked Papyrus Scripts: Removing TrueGaze.dll leaves zero orphaned FormIDs or scripts inside Skyrim saves.",
        F_SMALL,
        IVORY,
        cw - sc(28),
        line_gap=4,
    )
    draw_wrapped(
        draw,
        cx + sc(14),
        cy + sc(76),
        "• Reversible ActorValues (ModifierLedger): Every Moth's Stillness buff (Magicka Regen / Speechcraft / Illusion) is automatically stripped before save serialization and reverted cleanly whenever Expanded Play or Book Study is toggled off.",
        F_SMALL,
        SLATE,
        cw - sc(28),
        line_gap=4,
    )
    draw_wrapped(
        draw,
        cx + sc(14),
        cy + sc(122),
        "• SKSE 'TGZE' Co-Save Isolation: SIGN / LEDG / BOOK records live strictly inside the .skse co-save and never dirty the core .ess save file.",
        F_SMALL,
        SLATE,
        cw - sc(28),
        line_gap=4,
    )

    # RIGHT COLUMN: C++23 RUNTIME ARCHITECTURE
    rx, ry, rw, rh = draw_panel(
        draw,
        914,
        130,
        962,
        890,
        "EXPANDED PLAY RUNTIME PIPELINE & 4-RUNG RESPONSE LADDER",
        "< 0.1 ms / frame",
    )

    def arch_box(bx, by, bw, bh, head, lines_spec, border=BORDER, bg=CARD_BG, badge_col=BRASS):
        draw.rounded_rectangle([bx, by, bx + bw, by + bh], radius=sc(5), fill=bg, outline=border, width=2)
        draw.rectangle([bx, by + sc(6), bx + sc(5), by + bh - sc(6)], fill=badge_col)
        draw.text((bx + sc(14), by + sc(8)), head, font=F_H2, fill=badge_col)
        ly_i = by + sc(30)
        for txt, col in lines_spec:
            draw.text((bx + sc(14), ly_i), txt, font=F_SMALL, fill=col)
            ly_i += sc(17)

    ax = rx + sc(22)
    aw = rw - sc(44)
    ay = ry + sc(16)

    # Row 1: Frame Hook + Resolvers + Book Observer
    w3 = (aw - sc(24)) // 3
    arch_box(
        ax, ay, w3, sc(102),
        "GazeEngine::EndFrame",
        [("• Hooked post Actor::Update", SLATE), ("• Early exit if bEnabled=false", GREEN), ("• Drives 10 Hz / 2 Hz scheduler", SLATE), ("• Passes camera & deltaSeconds", SLATE)],
        border=CYAN, badge_col=CYAN,
    )
    arch_box(
        ax + w3 + sc(12), ay, w3, sc(102),
        "PlayerGazeResolver",
        [("• CrosshairPickData sweet-spot", SLATE), ("• Eye/Mouth/Forehead/Torso hit", SLATE), ("• HCEP / OpenVR / Tobii fusion", TEAL), ("• Blink suppression (500ms stale)", SLATE)],
        border=CYAN, badge_col=CYAN,
    )
    arch_box(
        ax + 2 * (w3 + sc(12)), ay, w3, sc(102),
        "BookStudyTracker",
        [("• Read-only RE::BookMenu sink", SLATE), ("• [pagebreak] / 220w parser", SLATE), ("• BSInputDeviceManager scan", SLATE), ("• Co-existing with all book mods", TEAL)],
        border=BRASS, badge_col=BRASS,
    )

    # Down arrows into Stage 2
    for i in range(3):
        mx = ax + i * (w3 + sc(12)) + w3 // 2
        draw.line([(mx, ay + sc(102)), (mx, ay + sc(128))], fill=BRASS, width=2)
        draw.polygon([(mx, ay + sc(134)), (mx - sc(5), ay + sc(124)), (mx + sc(5), ay + sc(124))], fill=BRASS)

    # Row 2: Demeanor + Perception + Interpretation (Safety Highlighted)
    ay2 = ay + sc(136)
    w2a = sc(290)
    w2b = sc(290)
    w2c = aw - w2a - w2b - sc(24)
    arch_box(
        ax, ay2, w2a, sc(128),
        "DemeanorController",
        [("1. Jarl Court / Superior (!Thane)", SLATE), ("2. Weapon Drawn / Combat -> Stern", SLATE), ("3. Sneak / Stolen -> Evasive", SLATE), ("4. Close Bond -> Warm | Pin 60s", SLATE), ("5. 3rd-person scale = 0.80x", BRASS)],
        border=BORDER, badge_col=BRASS,
    )
    arch_box(
        ax + w2a + sc(12), ay2, w2b, sc(128),
        "PerceptionGate",
        [("• Alive / Conscious / Awake / Combat", SLATE), ("• <=8m region, <=15m presence", SLATE), ("• HasLineOfSight ray + 197 deg arc", SLATE), ("• Scene 0.50x | Night-Eye 0.85 floor", SLATE), ("• Helmet 0.35x / Hood 0.70x bond", ORANGE)],
        border=BORDER, badge_col=ORANGE,
    )
    arch_box(
        ax + w2a + w2b + sc(24), ay2, w2c, sc(128),
        "InterpretationEngine",
        [("HARD CHILD SAFETY GATE (Line 1):", CRIMSON), ("  isChild -> NEVER Attraction / Leer", IVORY), ("• Heart vs Leer Torso disambiguation", SLATE), ("• 10 Skyrim racial/cultural modifiers", SLATE), ("• Dwell Band x Region x Tier -> [0..1]", GREEN)],
        border=CRIMSON, badge_col=CRIMSON, bg=(30, 20, 26),
    )

    # Down arrow into Stage 3
    mx_mid = rx + rw // 2
    draw.line([(mx_mid, ay2 + sc(128)), (mx_mid, ay2 + sc(154))], fill=BRASS, width=2)
    draw.polygon([(mx_mid, ay2 + sc(160)), (mx_mid - sc(5), ay2 + sc(150)), (mx_mid + sc(5), ay2 + sc(150))], fill=BRASS)

    # Row 3: Memory, Progression & Persistence
    ay3 = ay2 + sc(162)
    w3p = (aw - sc(14)) // 2
    arch_box(
        ax, ay3, w3p, sc(126),
        "GazeLedger (512-LRU Social Memory) & Witness 2Hz",
        [("• 512 persistent + 16 transient neutral-stranger buffer", SLATE), ("• 14-day exponential decay toward permanent floors", SLATE), ("• 8-event ring buffer (StareDownWon, LeerPublic, etc.)", SLATE), ("• 2 Hz witness: Spouse jealousy & Camilla/Sven/Faendal", BRASS)],
        border=BRASS, badge_col=BRASS,
    )
    arch_box(
        ax + w3p + sc(14), ay3, w3p, sc(126),
        "PresenceSignature & 'TGZE' Co-Save Serializer",
        [("• Presence 0..100 (3.0 full / 6.0 daily cap + regen)", SLATE), ("• 4 Signature Axes -> 8 Emergent Gossip Titles", SLATE), ("• Binary 'TGZE' v1 chunks: SIGN, LEDG, BOOK (FormID remap)", TEAL), ("• ExpandedPlayModifiers reversible ActorValue ledger", GREEN)],
        border=TEAL, badge_col=TEAL,
    )

    # Connector to Response Ladder
    draw.line([(mx_mid, ay3 + sc(126)), (mx_mid, ay3 + sc(152))], fill=GREEN, width=2)
    draw.polygon([(mx_mid, ay3 + sc(158)), (mx_mid - sc(5), ay3 + sc(148)), (mx_mid + sc(5), ay3 + sc(148))], fill=GREEN)

    # Row 4: 4-Rung Response Ladder + First-Slice Interactions
    ay4 = ay3 + sc(160)
    ladder_h = sc(330)
    draw.rounded_rectangle([ax, ay4, ax + aw, ay4 + ladder_h], radius=sc(6), fill=(21, 28, 39), outline=GREEN, width=2)
    draw.text((ax + sc(16), ay4 + sc(10)), "RESPONSE LADDER — ESCALATING 4-RUNG WORLD & ENGINE DISPATCH + FIRST-SLICE ACTIONS", font=F_H2, fill=GREEN)

    rungs = [
        ("RUNG 1: EYES", (51, 255, 89), "ActorGazeRuntime overrides HCEP mode, locks mutual eye-contact reciprocity (0.35s hold), or triggers CGA quadrant aversion on Leer/Contempt."),
        ("RUNG 2: FACE", (76, 201, 240), "BSFaceGenAnimationData::expressionKeyFrame applies gentle mood weights (<=0.50: Happy, Anger, Puzzled, Sad, Disgusted); yields during scenes/dialogue."),
        ("RUNG 3: OAR V3", (212, 180, 117), "Publishes 7 custom OAR conditions at kPostLoad (IsMode, IsMutualGaze, GetGazeRegion + GetInterpretation, GetDemeanor, GetPresenceTier, GetImpression)."),
        ("RUNG 4: BROADCAST", (255, 153, 38), "Rate-limited (>=20s/NPC): GazeReticle diegetic whisper + SKSE ModCallbackEvent 'TrueGaze_OnGazeInterpreted' + [TrueGaze::AiNpcBridge] context (Mantella/CHIM)."),
    ]
    ry_i = ay4 + sc(38)
    for rname, rcol, rdesc in rungs:
        draw.rounded_rectangle([ax + sc(14), ry_i, ax + aw - sc(14), ry_i + sc(50)], radius=sc(4), fill=CARD_ALT, outline=BORDER, width=1)
        draw.rounded_rectangle([ax + sc(22), ry_i + sc(11), ax + sc(168), ry_i + sc(39)], radius=sc(3), fill=blend_rgb(CARD_ALT, rcol, 0.18), outline=rcol, width=2)
        draw.text((ax + sc(30), ry_i + sc(17)), rname, font=F_MONO_B, fill=rcol)
        draw_wrapped(draw, ax + sc(182), ry_i + sc(8), rdesc, F_SMALL, IVORY, aw - sc(210), line_gap=3)
        ry_i += sc(56)

    # First-Slice Action Strip inside Response Ladder footer
    draw.rounded_rectangle([ax + sc(14), ry_i + sc(4), ax + aw - sc(14), ay4 + ladder_h - sc(12)], radius=sc(4), fill=(26, 35, 48), outline=BRASS_DIM, width=1)
    draw.text((ax + sc(24), ry_i + sc(10)), "FIRST-SLICE RIVERWOOD INTERACTIONS:", font=F_MONO_B, fill=BRASS)
    draw.text((ax + sc(300), ry_i + sc(10)), "Eye-Contact Dialogue (>=2.0s mutual <=4.5m)   |   3-Round Tavern Staring Contest (5s->10s->15s)   |   Sleeping Giant Inn Canticle (+5.0)", font=F_SMALL, fill=IVORY)

    save_downsampled(img, IMAGES_DIR / "expanded_play_architecture_and_switches.png")


# ==============================================================================
# PLATE 3: OCULAR CONTEMPLATION CURVE + MOTH'S STILLNESS + RIVERWOOD GAMEPLAY
# ==============================================================================
def build_plate_3():
    img, draw = new_canvas(
        "TRUEGAZE EXPANDED PLAY  ·  TECHNICAL PLATE 03  ·  PROGRESSION & LORE",
        "Ocular Contemplation, Moth's Stillness Stats & Riverwood Playable Slice",
        "Exact BookStudyTracker dwell mathematics, 4-axis Gaze Signature radar, Presence economy, and Sleeping Giant Inn mechanics",
        "Sources: BookStudyTracker.hpp/.cpp  |  PresenceSignature.hpp  |  ExpandedPlayModifiers.hpp/.cpp",
        "1920x1080  ·  Clean Rendering / Verified Equations",
    )

    # TOP-LEFT PANEL: BOOK STUDY MATHEMATICAL CURVE + MOTH'S STILLNESS RANKS
    p1x, p1y, p1w, p1h = draw_panel(
        draw,
        44,
        130,
        1060,
        442,
        "OCULAR CONTEMPLATION (BookStudyTracker) & MOTH'S STILLNESS STAT BONUSES",
        "Read-Only RE::BookMenu Observer",
    )

    # Left half of Top-Left: Real Mathematical Plot of MinDwell(Wp)
    gx, gy, gw, gh = p1x + sc(56), p1y + sc(42), sc(420), sc(268)
    draw.text((p1x + sc(22), p1y + sc(12)), "Required Spread Dwell Time: MinDwell(Wp) = clamp(Wp / 6.5, 2.5s, 12.0s)", font=F_H2, fill=BRASS)

    # Plot background
    draw.rectangle([gx, gy, gx + gw, gy + gh], fill=(13, 18, 26), outline=BORDER, width=2)
    # Speed flip danger zone (< 1.5s)
    y_15 = gy + gh - int((1.5 / 14.0) * gh)
    draw.rectangle([gx, y_15, gx + gw, gy + gh], fill=blend_rgb((13, 18, 26), CRIMSON, 0.16))
    draw.line([(gx, y_15), (gx + gw, y_15)], fill=CRIMSON, width=2)
    draw.text((gx + sc(10), y_15 + sc(4)), "< 1.5s: SPEED-FLIP IGNORED (0 credit)", font=F_MONO_B, fill=CRIMSON)

    # Grid lines y = 2.5, 5, 7.5, 10, 12
    for val_s, col_l in [(2.5, TEAL), (5.0, GRID), (7.5, GRID), (10.0, GRID), (12.0, ORANGE)]:
        yv = gy + gh - int((val_s / 14.0) * gh)
        draw.line([(gx, yv), (gx + gw, yv)], fill=col_l, width=1 if col_l == GRID else 2)
        lbl = f"{val_s:g}s"
        bb = draw.textbbox((0, 0), lbl, font=F_MONO)
        draw.text((gx - (bb[2] - bb[0]) - sc(8), yv - sc(7)), lbl, font=F_MONO, fill=SLATE if col_l == GRID else col_l)

    for wp_val in (0, 25, 50, 78, 100, 125):
        xv = gx + int((wp_val / 130.0) * gw)
        draw.line([(xv, gy), (xv, gy + gh)], fill=GRID, width=1)
        lbl = f"{wp_val}w"
        draw.text((xv - sc(10), gy + gh + sc(6)), lbl, font=F_MONO, fill=MUTED)

    # Actual clamped curve points Wp = 0..130
    curve_pts = []
    for wp in range(0, 131, 2):
        dwell = max(2.5, min(12.0, wp / 6.5))
        px_c = gx + int((wp / 130.0) * gw)
        py_c = gy + gh - int((dwell / 14.0) * gh)
        curve_pts.append((px_c, py_c))
    draw.line(curve_pts, fill=CYAN, width=5)

    # Mark inflection points Wp=16.25 (2.5s) and Wp=78 (12.0s)
    for wp_pt, s_pt, txt_pt in [(16.25, 2.5, "2.5s floor (<=16w)"), (78.0, 12.0, "12.0s cap (>=78w)")]:
        px_m = gx + int((wp_pt / 130.0) * gw)
        py_m = gy + gh - int((s_pt / 14.0) * gh)
        draw.ellipse([px_m - sc(5), py_m - sc(5), px_m + sc(5), py_m + sc(5)], fill=BRASS, outline=IVORY, width=2)
        draw.text((px_m + sc(8), py_m - sc(18)), txt_pt, font=F_MONO, fill=IVORY)

    # Anti-exploit note below plot
    draw.text((gx, gy + gh + sc(28)), "Max counted dwell/spread = 45.0s AFK cap  |  7-day 25% refresh  |  >=80% completion = Book Studied", font=F_SMALL, fill=SLATE)

    # Right half of Top-Left: 6 Moth's Stillness Ranks Table
    rx1 = p1x + sc(510)
    rw1 = p1w - sc(530)
    ry1 = p1y + sc(12)
    draw.text((rx1, ry1), "Moth's Stillness Ranks & Reversible Player Buffs", font=F_H2, fill=BRASS)
    ry1 += sc(26)
    draw.rectangle([rx1, ry1, rx1 + rw1, ry1 + sc(24)], fill=(26, 34, 48))
    draw.text((rx1 + sc(8), ry1 + sc(4)), "RANK", font=F_MONO_B, fill=MUTED)
    draw.text((rx1 + sc(108), ry1 + sc(4)), "BOOKS", font=F_MONO_B, fill=MUTED)
    draw.text((rx1 + sc(180), ry1 + sc(4)), "ACTORVALUE MODIFIER BUFFS (kPermanent)", font=F_MONO_B, fill=MUTED)
    ry1 += sc(26)

    ranks = [
        ("Novice", "0 - 4", "Baseline only (+0.05 Max Presence / studied spread)", MUTED),
        ("Reader", ">= 5", "+1% Magicka Regen  |  +1% Presence Regen", SLATE),
        ("Acolyte", ">= 15", "+2% Magicka Regen, +1 Speech & Illusion, +2% P.Regen", CYAN),
        ("Scholar", ">= 35", "+3% Magicka Regen, +2 Speech & Illusion, +4% P.Regen", TEAL),
        ("Prelate", ">= 75", "+4% Magicka Regen, +3 Speech & Illusion, +7% P.Regen", BRASS),
        ("Canticle", ">= 150", "+5% Magicka Regen, +5 Speech & Illusion, +10% P.Regen", GREEN),
    ]
    for idx, (rk, bks, buff, rcol) in enumerate(ranks):
        bg_r = CARD_BG if idx % 2 == 0 else CARD_ALT
        draw.rectangle([rx1, ry1, rx1 + rw1, ry1 + sc(32)], fill=bg_r)
        draw.text((rx1 + sc(8), ry1 + sc(8)), rk, font=F_BODY_B, fill=rcol)
        draw.text((rx1 + sc(108), ry1 + sc(8)), bks, font=F_MONO_B, fill=IVORY)
        draw.text((rx1 + sc(180), ry1 + sc(8)), buff, font=F_SMALL, fill=SLATE)
        ry1 += sc(34)

    # Bottom bar inside Top-Left: Deep Craft + Compatibility note
    draw.rounded_rectangle([rx1, ry1 + sc(12), rx1 + rw1, p1y + p1h - sc(16)], radius=sc(4), fill=(22, 31, 44), outline=TEAL, width=1)
    draw_wrapped(
        draw,
        rx1 + sc(12),
        ry1 + sc(20),
        "Complementary by Design: Never hooks or replaces other book mods (Book Covers Skyrim, Unread Books Glow, etc.). Skill books studied >=80% also grant +0.5 Presence & 2h Deep Craft.",
        F_SMALL,
        IVORY,
        rw1 - sc(24),
        line_gap=4,
    )

    # BOTTOM-LEFT PANEL: 4-AXIS SIGNATURE RADAR + PRESENCE ECONOMY
    p2x, p2y, p2w, p2h = draw_panel(
        draw,
        44,
        592,
        1060,
        428,
        "PRESENCE TIERS (0..100), DAILY CAPS & 4-AXIS GAZE SIGNATURE TITLES",
        "Emergent Character Identity",
    )

    # Radar Plot (center-left of bottom panel)
    rcx, rcy = p2x + sc(252), p2y + sc(196)
    rr = sc(106)
    for frac in (0.33, 0.60, 1.0):
        rad = int(rr * frac)
        col_r = BRASS if frac == 0.60 else BORDER
        draw.ellipse([rcx - rad, rcy - rad, rcx + rad, rcy + rad], outline=col_r, width=2 if frac == 0.60 else 1)
    draw.line([(rcx - rr, rcy), (rcx + rr, rcy)], fill=(70, 84, 105), width=2)
    draw.line([(rcx, rcy - rr), (rcx, rcy + rr)], fill=(70, 84, 105), width=2)
    # Diagonals for subtle quadrant clarity
    d = int(rr * 0.707)
    draw.line([(rcx - d, rcy - d), (rcx + d, rcy + d)], fill=GRID, width=1)
    draw.line([(rcx - d, rcy + d), (rcx + d, rcy - d)], fill=GRID, width=1)

    # Sample emergent player signature polygon (e.g., Warm +68, Restraint +72, Candour +64, Dominance +35)
    sample_poly = [
        (rcx, rcy - int(rr * 0.72)),       # Top: Restraint +72
        (rcx + int(rr * 0.68), rcy),       # Right: Warmth +68
        (rcx, rcy + int(rr * 0.64)),       # Bottom: Candour +64
        (rcx - int(rr * 0.35), rcy),       # Left: Dominance +35
    ]
    draw.polygon(sample_poly, fill=blend_rgb(PANEL_BG, GREEN, 0.22), outline=GREEN)
    draw.line(sample_poly + [sample_poly[0]], fill=GREEN, width=3)

    # Axis labels cleanly placed outside circle
    draw.text((rcx - sc(64), rcy - rr - sc(26)), "Disciplined (+100 R)", font=F_MONO_B, fill=CYAN)
    draw.text((rcx + rr + sc(10), rcy - sc(10)), "Warm (+100 W)", font=F_MONO_B, fill=(255, 217, 102))
    draw.text((rcx - sc(54), rcy + rr + sc(10)), "Candid (+100 C)", font=F_MONO_B, fill=GREEN)
    draw.text((rcx - rr - sc(126), rcy - sc(18)), "Dominant\n(+100 D)", font=F_MONO_B, fill=ORANGE)
    draw.text((p2x + sc(22), p2y + sc(12)), "Gold Ring = +/-60.0 Title Threshold", font=F_SMALL, fill=BRASS)

    # Titles & Presence Economy right side of Bottom-Left
    rx2 = p2x + sc(500)
    rw2 = p2w - sc(520)
    ry2 = p2y + sc(14)
    draw.text((rx2, ry2), "8 Emergent Signature Titles (EvaluateSignatureTitle >= |60|)", font=F_H2, fill=BRASS)
    ry2 += sc(24)

    titles = [
        ("Mara's Own", "W >= +60 & C >= +60", GREEN),
        ("Gentle Soul", "W >= +60 & D <= -60", (255, 217, 102)),
        ("Kind Eyes", "W >= +60", TEAL),
        ("Ice-Gaze", "W <= -60 & D >= +60", CYAN),
        ("The Unblinking", "D >= +60 & R >= +60", ORANGE),
        ("Steady-Eyed", "D >= +60 & C >= +60", BRASS),
        ("Quick-Eyes", "C <= -60", SLATE),
        ("Wandering Eyes", "R <= -60", CRIMSON),
    ]
    tw_col = (rw2 - sc(10)) // 2
    for idx, (tname, tcond, tcol) in enumerate(titles):
        cx_t = rx2 + (idx % 2) * (tw_col + sc(10))
        cy_t = ry2 + (idx // 2) * sc(32)
        draw.rounded_rectangle([cx_t, cy_t, cx_t + tw_col, cy_t + sc(27)], radius=sc(4), fill=CARD_BG, outline=BORDER, width=1)
        draw.text((cx_t + sc(10), cy_t + sc(6)), tname, font=F_BODY_B, fill=tcol)
        draw.text((cx_t + sc(132), cy_t + sc(6)), tcond, font=F_MONO, fill=SLATE)

    ry2 += 4 * sc(32) + sc(16)
    draw.text((rx2, ry2), "Presence Economy (Start = 20.0 -> Max 100.0) & Daily Diminishing Returns", font=F_H2, fill=BRASS)
    ry2 += sc(24)

    tiers = [
        ("Unnoticed", "0-19", (90, 102, 120)),
        ("Noticed", "20-39", (76, 201, 240)),
        ("Steady", "40-59", (64, 212, 180)),
        ("Commanding", "60-79", (212, 180, 117)),
        ("Legendary", "80-100", (51, 255, 89)),
    ]
    t_seg = rw2 // len(tiers)
    for idx, (tn, tr, tc) in enumerate(tiers):
        tx_i = rx2 + idx * t_seg
        draw.rectangle([tx_i, ry2, tx_i + t_seg - sc(4), ry2 + sc(42)], fill=blend_rgb(CARD_ALT, tc, 0.16), outline=tc, width=2)
        draw.text((tx_i + sc(8), ry2 + sc(5)), tn, font=F_BODY_B, fill=IVORY)
        draw.text((tx_i + sc(8), ry2 + sc(22)), tr, font=F_MONO_B, fill=tc)

    ry2 += sc(52)
    draw.rounded_rectangle([rx2, ry2, rx2 + rw2, p2y + p2h - sc(16)], radius=sc(4), fill=CARD_ALT, outline=BORDER, width=1)
    draw_wrapped(
        draw,
        rx2 + sc(12),
        ry2 + sc(10),
        "Daily Cap Rule: First 3.0 Max Presence/day at 100% rate -> next 3.0 at 50% rate -> hard cap at +6.0/day. One-shot First Canticle awakening (+5.0) bypasses cap. Staring Contest loss applies -1.0 current + 1-day -0.5 Shaken debuff.",
        F_SMALL,
        SLATE,
        rw2 - sc(24),
        line_gap=4,
    )

    # RIGHT PANEL: RIVERWOOD & SLEEPING GIANT INN FIRST PLAYABLE SLICE
    p3x, p3y, p3w, p3h = draw_panel(
        draw,
        1124,
        130,
        752,
        890,
        "RIVERWOOD & SLEEPING GIANT INN SLICE",
        "Lore + Gameplay Features",
    )

    cy3 = p3y + sc(18)
    cx3 = p3x + sc(20)
    cw3 = p3w - sc(40)

    # 1. Moth Priest Lore Foundation Card
    draw.rounded_rectangle([cx3, cy3, cx3 + cw3, cy3 + sc(142)], radius=sc(5), fill=CARD_BG, outline=BRASS, width=2)
    draw.text((cx3 + sc(14), cy3 + sc(10)), "LORE GROUNDING: THE FIRST CANTICLE OF THE EYE (MOTH PRIESTS)", font=F_H2, fill=BRASS)
    draw_wrapped(
        draw,
        cx3 + sc(14),
        cy3 + sc(34),
        "Before the Elder Scrolls cost the Moth Priests their physical sight, the First Canticle taught acolytes how to still their own eyes so deeply that another soul's gaze became legible like script. Introduced organically either via a DLL-only Awakening Whisper in the Sleeping Giant Inn or later via Brother Cassian (independent of Dexion Evicus & Septimus Signus).",
        F_SMALL,
        IVORY,
        cw3 - sc(28),
        line_gap=4,
    )
    draw.text((cx3 + sc(14), cy3 + sc(116)), "AWAKENING TRIGGER:  >= 4.5s Mutual Gaze in Sleeping Giant Inn  OR  1st Canticle/Book >= 80%  ->  +5.0 Presence", font=F_MONO_B, fill=GREEN)
    cy3 += sc(158)

    # 2. Eye-Contact Conversation Start
    draw.rounded_rectangle([cx3, cy3, cx3 + cw3, cy3 + sc(136)], radius=sc(5), fill=CARD_BG, outline=CYAN, width=2)
    draw.text((cx3 + sc(14), cy3 + sc(10)), "EYE-CONTACT CONVERSATION START (bEnableEyeContactInitiate=true)", font=F_H2, fill=CYAN)
    draw_wrapped(
        draw,
        cx3 + sc(14),
        cy3 + sc(34),
        "Hold >= 2.0s of mutual Respect or Warmth eye contact with any calm adult NPC inside 4.5m while PresenceTier >= Noticed (or immediately inside the Sleeping Giant Inn) to open dialogue cleanly via ActivateRef without touching the [E] key.",
        F_SMALL,
        SLATE,
        cw3 - sc(28),
        line_gap=4,
    )
    draw.text((cx3 + sc(14), cy3 + sc(108)), "HARD BLOCKS:  Combat = Yes  |  Weapon Drawn = Yes  |  Scene/Dialogue = Yes  |  Child = Yes  |  30s cooldown", font=F_MONO, fill=CYAN)
    cy3 += sc(152)

    # 3. 3-Round Staring Contest State Diagram
    draw.rounded_rectangle([cx3, cy3, cx3 + cw3, cy3 + sc(230)], radius=sc(5), fill=CARD_BG, outline=ORANGE, width=2)
    draw.text((cx3 + sc(14), cy3 + sc(10)), "TAVERN STARING CONTEST (Console: Target NPC + stgexpanded)", font=F_H2, fill=ORANGE)
    draw.text((cx3 + sc(14), cy3 + sc(32)), "Locks opponent eye-to-eye in LOGIC challenge while player resists frost-vignette blink pressure:", font=F_SMALL, fill=SLATE)

    round_w = (cw3 - sc(44)) // 3
    ry_box = cy3 + sc(58)
    for r_idx, (r_lbl, r_time, r_rate, r_col) in enumerate([
        ("ROUND 1", "0.0 - 5.0s", "0.045 / sec pressure", TEAL),
        ("ROUND 2", "5.0 - 10.0s", "0.060 / sec pressure", ORANGE),
        ("ROUND 3", "10.0 - 15.0s", "0.075 / sec + Victory!", CRIMSON),
    ]):
        rx_b = cx3 + sc(14) + r_idx * (round_w + sc(8))
        draw.rounded_rectangle([rx_b, ry_box, rx_b + round_w, ry_box + sc(72)], radius=sc(4), fill=CARD_ALT, outline=r_col, width=2)
        draw.text((rx_b + sc(10), ry_box + sc(8)), r_lbl, font=F_MONO_B, fill=r_col)
        draw.text((rx_b + sc(10), ry_box + sc(28)), r_time, font=F_BODY_B, fill=IVORY)
        draw.text((rx_b + sc(10), ry_box + sc(48)), r_rate, font=F_SMALL, fill=SLATE)

    draw_wrapped(
        draw,
        cx3 + sc(14),
        ry_box + sc(84),
        "• Relief Scaling: Higher Max Presence reduces blink-pressure buildup by up to 40%.\n"
        "• Look-Away Grace: 0.45s tolerance; exceeding grace -> LostLookedAway. Eye-tracker blink -> LostBlinked.\n"
        "• Victory Reward: +1.0 Max Presence, Permanent Respect floor (+12), StareDownWon event.",
        F_SMALL,
        IVORY,
        cw3 - sc(28),
        line_gap=4,
    )
    cy3 += sc(246)

    # 4. Riverwood Triangle Witness Diagram
    draw.rounded_rectangle([cx3, cy3, cx3 + cw3, p3y + p3h - sc(18)], radius=sc(5), fill=CARD_BG, outline=CRIMSON, width=2)
    draw.text((cx3 + sc(14), cy3 + sc(10)), "WITNESS MECHANICS: RIVERWOOD LOVE TRIANGLE & SPOUSE HOOK", font=F_H2, fill=CRIMSON)

    # Schematic triangle inside card
    tri_cx = cx3 + sc(136)
    tri_cy = cy3 + sc(148)
    p_player = (cx3 + sc(52), cy3 + sc(200))
    p_camilla = (cx3 + sc(196), cy3 + sc(106))
    p_rival = (cx3 + sc(240), cy3 + sc(210))

    draw.line([p_player, p_camilla], fill=GREEN, width=4)
    draw.line([p_rival, p_camilla], fill=CRIMSON, width=3)
    draw.line([p_rival, p_player], fill=ORANGE, width=2)

    for px_n, py_n, lbl_n, ncol, dy_lbl in [
        (p_player[0], p_player[1], "PLAYER", CYAN, sc(12)),
        (p_camilla[0], p_camilla[1], "CAMILLA (0x1347B)", GREEN, -sc(26)),
        (p_rival[0], p_rival[1], "SVEN / FAENDAL", CRIMSON, sc(12)),
    ]:
        draw.ellipse([px_n - sc(8), py_n - sc(8), px_n + sc(8), py_n + sc(8)], fill=ncol, outline=IVORY, width=2)
        draw.text((px_n - sc(32), py_n + dy_lbl), lbl_n, font=F_MONO_B, fill=IVORY)

    info_x = cx3 + sc(310)
    info_w = cw3 - sc(324)
    ny_info = draw_wrapped(
        draw,
        info_x,
        cy3 + sc(44),
        "Every 0.5s (2 Hz), ExpandedPlayManager inspects up to 4 aware observers within 12.0m with dual HasLineOfSight to Player & Target:",
        F_SMALL,
        IVORY,
        info_w,
        line_gap=4,
    )
    ny_info = draw_wrapped(
        draw,
        info_x,
        ny_info + sc(10),
        "• Riverwood Triangle (Sven 0x1347F / Faendal 0x13480 watching Warmth / Attraction / Leer on Camilla): Unease +10, Suspicion +8, Warmth -4.",
        F_SMALL,
        SLATE,
        info_w,
        line_gap=4,
    )
    draw_wrapped(
        draw,
        info_x,
        ny_info + sc(8),
        "• Spouse / Partner Jealousy: Unease +15, Warmth -10, Floor 15, SpouseJealousy whisper.",
        F_SMALL,
        SLATE,
        info_w,
        line_gap=4,
    )

    save_downsampled(img, IMAGES_DIR / "expanded_play_moth_stillness_and_mechanics.png")


if __name__ == "__main__":
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    build_plate_1()
    build_plate_2()
    build_plate_3()
