"""Draws ShooterGame's HUD icons into one atlas, UI/HUDIcons.png, for LeonEd's texture import.

    python3 Game/ShooterGame/SourceArt/UI/make_hud_icons.py

Game/ShooterGame/SourceArt/ImportList.ini imports it as /Game/UI/T_HUDIcons, which AShooterHUD draws from
(DefaultGame.ini: IconsTextureName). The icons are white shapes whose alpha is their coverage, so the HUD tints each
one when it draws it (the GS's MODULATE) and the cook keeps the atlas in 16 colours (PSMT4: white at 16 alphas):

- the kill feed's (16 pixels high, CS's d_ sprites): the weapons pointing right, toward the victim (ak47, m4a1, awp,
  mp5, glock, usp, deagle, knife, hegrenade, flashbang, smokegrenade), the bomb (c4), the world (a skull) and the
  headshot;
- the status icons (24 x 24): health (a cross), armor (a vest), armor with a helmet, the buy zone (a cart), the bomb,
  the defuse kit (wire cutters) and the round's clock (a stopwatch).

Every shape is polygons, circles and rectangles in the icon's pixels, filled with 4 x 4 samples a pixel (16 coverage
levels, 0 to 15 x 17 of alpha): the same bytes on every run and machine (Python's standard library only, zlib for the
PNG). The rectangles are AShooterHUD's icon table (ShooterHUD.cpp, GetHUDIcon); the two must change together, and
ShooterGame.HUD.IconAtlas checks that every icon has texels in its rectangle and none outside the icons. No external
art (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
WIDTH = 256
HEIGHT = 64
SAMPLES = 4


# Shapes: each is (kind, data, subtract); a pixel's coverage is the share of its samples inside the icon's added shapes
# and outside its subtracted ones (a subtracted shape cuts every shape before it).

def poly(points, cut=False):
    return ("poly", points, cut)


def rect(x0, y0, x1, y1, cut=False):
    return ("poly", [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], cut)


def circle(cx, cy, r, cut=False):
    return ("circle", (cx, cy, r), cut)


def ring(cx, cy, r_out, r_in):
    return [circle(cx, cy, r_out), circle(cx, cy, r_in, cut=True)]


def bar(x0, y0, x1, y1, width, cut=False):
    """A segment Width thick, as a quad."""
    dx = x1 - x0
    dy = y1 - y0
    length = math.hypot(dx, dy)
    nx = -dy / length * width * 0.5
    ny = dx / length * width * 0.5
    return ("poly", [(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], cut)


def inside(shape, x, y):
    kind, data, _cut = shape
    if kind == "circle":
        cx, cy, r = data
        return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r
    # Even-odd rule.
    result = False
    count = len(data)
    for i in range(count):
        x0, y0 = data[i]
        x1, y1 = data[(i + 1) % count]
        if (y0 > y) != (y1 > y):
            cross = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
            if x < cross:
                result = not result
    return result


def covered(shapes, x, y):
    state = False
    for shape in shapes:
        if shape[2]:
            if state and inside(shape, x, y):
                state = False
        elif not state and inside(shape, x, y):
            state = True
    return state


# The icons: name -> (x, y, width, height, shapes in the icon's pixels, y down).

def rifle_ak47():
    return [
        poly([(0.5, 5.0), (12.0, 5.5), (12.0, 8.5), (3.0, 11.8), (0.5, 11.8)]),
        rect(12.0, 4.5, 30.5, 8.5),
        poly([(17.0, 8.5), (20.0, 8.5), (18.5, 13.0), (15.5, 13.0)]),
        poly([(23.0, 8.5), (27.0, 8.5), (28.5, 12.0), (29.8, 15.2), (26.2, 15.6), (24.4, 12.0)]),
        rect(30.5, 5.0, 37.0, 8.2),
        rect(30.5, 4.0, 40.0, 5.0),
        rect(37.0, 5.8, 47.5, 7.0),
        rect(44.0, 3.4, 45.2, 5.8),
        rect(26.0, 3.5, 28.0, 4.5),
    ]


def rifle_m4a1():
    return [
        poly([(0.5, 5.0), (8.0, 5.0), (8.0, 10.2), (0.5, 11.2)]),
        rect(8.0, 5.5, 13.0, 7.5),
        rect(13.0, 4.5, 28.0, 8.5),
        rect(15.0, 2.5, 24.0, 4.5),
        rect(16.5, 3.0, 22.5, 4.0, cut=True),
        poly([(16.0, 8.5), (19.0, 8.5), (17.5, 13.0), (14.5, 13.0)]),
        poly([(21.0, 8.5), (25.0, 8.5), (25.6, 14.6), (21.6, 14.6)]),
        rect(28.0, 5.0, 38.0, 8.6),
        poly([(35.0, 5.0), (37.0, 5.0), (36.5, 2.5), (35.5, 2.5)]),
        rect(38.0, 6.0, 45.0, 7.0),
        rect(45.0, 5.6, 47.5, 7.4),
    ]


def rifle_awp():
    return [
        poly([(0.5, 5.0), (11.0, 5.5), (11.0, 9.0), (6.5, 9.0), (3.5, 12.2), (0.5, 12.2)]),
        circle(7.2, 7.4, 1.2, cut=True),
        rect(11.0, 5.0, 24.0, 8.5),
        rect(13.5, 2.2, 24.5, 4.2),
        rect(12.2, 1.4, 14.2, 4.6),
        rect(24.0, 1.4, 26.0, 4.6),
        rect(16.0, 4.0, 22.0, 5.0),
        rect(19.5, 8.5, 22.5, 11.2),
        poly([(12.0, 8.5), (15.0, 8.5), (14.0, 11.2), (12.0, 11.2)]),
        rect(24.0, 5.8, 47.5, 7.2),
        rect(44.5, 5.2, 47.5, 7.8),
    ]


def smg_mp5():
    return [
        rect(1.5, 4.5, 12.0, 10.5),
        rect(3.5, 6.0, 12.0, 9.0, cut=True),
        rect(12.0, 4.5, 32.0, 8.5),
        poly([(17.0, 8.5), (20.0, 8.5), (19.0, 13.0), (16.0, 13.0)]),
        poly([(24.0, 8.5), (27.0, 8.5), (28.6, 14.6), (25.6, 15.2)]),
        rect(32.0, 5.0, 40.0, 9.0),
        rect(40.0, 6.0, 44.5, 7.2),
        rect(14.0, 3.0, 17.0, 4.5),
        rect(38.0, 3.0, 40.0, 5.0),
    ]


def pistol_glock():
    return [
        rect(4.0, 3.0, 28.0, 7.5),
        rect(5.0, 7.5, 27.0, 8.5),
        poly([(6.0, 8.5), (12.0, 8.5), (11.0, 15.2), (5.0, 15.2), (4.4, 12.0)]),
        poly([(12.0, 8.5), (17.0, 8.5), (16.0, 11.6), (12.5, 11.6)]),
        poly([(13.0, 8.5), (15.8, 8.5), (15.2, 10.7), (13.3, 10.7)], cut=True),
        rect(28.0, 4.0, 29.5, 6.0),
    ]


def pistol_usp():
    return [
        rect(4.0, 3.0, 29.0, 7.5),
        rect(3.0, 3.5, 4.5, 5.5),
        rect(5.0, 7.5, 27.0, 8.5),
        poly([(5.0, 8.0), (11.0, 8.0), (10.5, 15.2), (4.5, 15.2), (4.0, 11.0)]),
        poly([(11.0, 8.5), (16.5, 8.5), (15.5, 11.8), (11.5, 11.8)]),
        poly([(12.0, 8.5), (15.3, 8.5), (14.7, 10.8), (12.3, 10.8)], cut=True),
        rect(29.0, 4.2, 30.5, 6.0),
        rect(8.0, 3.5, 12.0, 4.2, cut=True),
    ]


def pistol_deagle():
    return [
        rect(3.0, 2.5, 30.0, 7.5),
        rect(10.0, 1.6, 29.0, 2.5),
        rect(4.0, 7.5, 26.0, 8.6),
        poly([(5.0, 8.0), (12.0, 8.0), (11.0, 15.6), (4.0, 15.6)]),
        poly([(12.0, 8.6), (18.0, 8.6), (17.0, 12.2), (12.5, 12.2)]),
        poly([(13.0, 8.6), (16.8, 8.6), (16.1, 11.2), (13.3, 11.2)], cut=True),
        rect(20.0, 4.0, 28.0, 5.0, cut=True),
    ]


def knife():
    return [
        poly([(14.0, 5.8), (36.0, 5.4), (39.6, 6.6), (35.5, 8.6), (14.0, 9.2)]),
        rect(12.0, 3.8, 14.0, 11.2),
        poly([(1.5, 6.0), (12.0, 6.3), (12.0, 8.8), (1.5, 9.6)]),
        rect(0.4, 5.4, 2.0, 10.2),
        rect(16.0, 6.6, 33.0, 7.0, cut=True),
    ]


def grenade_he():
    return [
        circle(8.0, 10.0, 5.6),
        rect(6.0, 3.0, 10.0, 5.2),
        poly([(9.5, 3.2), (12.5, 2.6), (14.2, 6.2), (12.8, 7.0), (11.0, 4.4)]),
    ] + ring(4.4, 4.0, 2.2, 1.2)


def grenade_flash():
    return [
        rect(4.5, 5.0, 11.5, 15.4),
        rect(5.5, 3.4, 10.5, 5.0),
        rect(10.5, 3.4, 12.6, 9.5),
        rect(4.5, 9.0, 11.5, 10.0, cut=True),
    ] + ring(4.0, 3.2, 2.0, 1.1)


def grenade_smoke():
    return [
        rect(4.0, 5.0, 12.0, 15.4),
        rect(5.0, 3.4, 11.0, 5.0),
        rect(11.0, 3.4, 13.0, 9.0),
        rect(4.0, 8.0, 12.0, 8.9, cut=True),
        rect(4.0, 11.5, 12.0, 12.4, cut=True),
    ] + ring(3.6, 3.2, 2.0, 1.1)


def bomb_c4_small():
    shapes = [rect(1.0, 4.0, 23.0, 13.5), rect(3.0, 5.5, 10.0, 8.5, cut=True)]
    for row in range(3):
        for col in range(3):
            x = 12.0 + col * 3.4
            y = 5.6 + row * 2.6
            shapes.append(rect(x, y, x + 2.2, y + 1.6, cut=True))
    shapes.append(bar(4.0, 4.0, 7.0, 1.0, 1.0))
    shapes.append(bar(7.0, 1.0, 12.0, 2.5, 1.0))
    return shapes


def skull():
    return [
        circle(8.0, 6.8, 5.6),
        rect(5.0, 9.0, 11.0, 14.6),
        circle(5.9, 7.2, 1.7, cut=True),
        circle(10.1, 7.2, 1.7, cut=True),
        poly([(8.0, 9.0), (9.0, 10.8), (7.0, 10.8)], cut=True),
        rect(6.6, 12.2, 7.2, 14.6, cut=True),
        rect(8.8, 12.2, 9.4, 14.6, cut=True),
    ]


def headshot():
    return [
        circle(8.0, 7.0, 5.8),
        rect(5.8, 11.0, 10.2, 15.4),
        circle(9.4, 6.0, 1.8, cut=True),
        bar(9.4, 6.0, 13.6, 2.6, 0.7, cut=True),
        bar(9.4, 6.0, 5.2, 3.2, 0.7, cut=True),
        bar(9.4, 6.0, 12.6, 9.8, 0.7, cut=True),
    ]


def status_health():
    return [rect(8.0, 2.0, 16.0, 22.0), rect(2.0, 8.0, 22.0, 16.0)]


def status_armor():
    return [
        poly([(4.0, 3.0), (9.0, 3.0), (12.0, 6.0), (15.0, 3.0), (20.0, 3.0), (21.2, 12.0), (18.0, 21.4),
              (6.0, 21.4), (2.8, 12.0)]),
        rect(11.4, 8.0, 12.6, 20.0, cut=True),
    ]


def status_armor_helmet():
    return [
        circle(12.0, 9.2, 7.0),
        rect(3.0, 9.2, 21.0, 20.0, cut=True),
        rect(3.6, 9.2, 20.4, 11.0),
        poly([(6.5, 12.4), (17.5, 12.4), (18.6, 17.0), (16.0, 22.2), (8.0, 22.2), (5.4, 17.0)]),
        rect(11.4, 13.6, 12.6, 21.0, cut=True),
    ]


def status_buyzone():
    shapes = [poly([(4.0, 5.0), (21.5, 6.0), (19.2, 14.2), (6.4, 14.2)])]
    for x in (8.4, 12.6, 16.6):
        shapes.append(bar(x, 7.2, x - 0.2, 12.8, 1.3, cut=True))
    shapes.append(bar(5.2, 10.0, 20.2, 10.0, 1.0, cut=True))
    shapes.append(bar(0.8, 3.2, 4.4, 4.2, 1.6))
    shapes.append(bar(6.4, 14.2, 6.8, 16.2, 1.6))
    shapes.append(bar(6.8, 16.2, 19.0, 16.2, 1.6))
    shapes.append(circle(8.2, 19.2, 2.2))
    shapes.append(circle(17.2, 19.2, 2.2))
    return shapes


def status_bomb():
    shapes = [rect(2.0, 8.0, 22.0, 18.5), rect(4.0, 10.0, 11.0, 13.5, cut=True)]
    for row in range(3):
        for col in range(3):
            x = 13.0 + col * 2.8
            y = 10.0 + row * 2.6
            shapes.append(rect(x, y, x + 1.8, y + 1.6, cut=True))
    shapes.append(bar(5.0, 8.0, 8.0, 3.5, 1.2))
    shapes.append(bar(8.0, 3.5, 14.0, 5.0, 1.2))
    shapes.append(bar(14.0, 5.0, 17.5, 8.0, 1.2))
    return shapes


def status_defuser():
    return [
        poly([(3.0, 21.0), (5.0, 22.6), (13.2, 12.2), (11.2, 10.6)]),
        poly([(19.0, 22.6), (21.0, 21.0), (12.8, 10.6), (10.8, 12.2)]),
        poly([(10.2, 11.0), (13.8, 11.0), (14.6, 3.0), (12.0, 1.2), (9.4, 3.0)]),
        rect(11.6, 2.0, 12.4, 10.0, cut=True),
        circle(12.0, 11.6, 2.2),
        circle(12.0, 11.6, 0.9, cut=True),
    ]


def status_clock():
    return ring(12.0, 13.6, 9.2, 7.2) + [
        rect(10.0, 1.4, 14.0, 3.8),
        rect(11.0, 3.8, 13.0, 4.8),
        rect(11.3, 7.6, 12.7, 14.2),
        bar(12.0, 13.6, 16.4, 11.2, 1.4),
        bar(17.6, 5.4, 19.6, 7.4, 1.6),
    ]


# The atlas's layout: AShooterHUD's icon table (ShooterHUD.cpp) holds the same rectangles.
ICONS = [
    ("ak47", 0, 0, 48, 16, rifle_ak47),
    ("m4a1", 48, 0, 48, 16, rifle_m4a1),
    ("awp", 96, 0, 48, 16, rifle_awp),
    ("mp5", 144, 0, 48, 16, smg_mp5),
    ("glock", 192, 0, 32, 16, pistol_glock),
    ("usp", 224, 0, 32, 16, pistol_usp),
    ("deagle", 0, 16, 32, 16, pistol_deagle),
    ("knife", 32, 16, 40, 16, knife),
    ("hegrenade", 72, 16, 16, 16, grenade_he),
    ("flashbang", 88, 16, 16, 16, grenade_flash),
    ("smokegrenade", 104, 16, 16, 16, grenade_smoke),
    ("c4", 120, 16, 24, 16, bomb_c4_small),
    ("world", 144, 16, 16, 16, skull),
    ("headshot", 160, 16, 16, 16, headshot),
    ("health", 0, 32, 24, 24, status_health),
    ("armor", 24, 32, 24, 24, status_armor),
    ("armorhelmet", 48, 32, 24, 24, status_armor_helmet),
    ("buyzone", 72, 32, 24, 24, status_buyzone),
    ("bomb", 96, 32, 24, 24, status_bomb),
    ("defuser", 120, 32, 24, 24, status_defuser),
    ("clock", 144, 32, 24, 24, status_clock),
]


def draw_atlas():
    alpha = bytearray(WIDTH * HEIGHT)
    step = 1.0 / SAMPLES
    for _name, ox, oy, width, height, make in ICONS:
        shapes = make()
        for py in range(height):
            for px in range(width):
                hits = 0
                for sy in range(SAMPLES):
                    for sx in range(SAMPLES):
                        if covered(shapes, px + (sx + 0.5) * step, py + (sy + 0.5) * step):
                            hits += 1
                # 16 samples: 0 to 16 hits, kept as 16 levels (the PSMT4 palette's).
                level = min(15, (hits * 15 + 8) // (SAMPLES * SAMPLES))
                alpha[(oy + py) * WIDTH + ox + px] = level * 17
    return alpha


def write_png(path, alpha):
    raw = bytearray()
    for y in range(HEIGHT):
        raw.append(0)
        for x in range(WIDTH):
            a = alpha[y * WIDTH + x]
            raw.extend((255, 255, 255, a))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as out:
        out.write(png)


def main():
    alpha = draw_atlas()
    path = os.path.join(HERE, "HUDIcons.png")
    write_png(path, alpha)
    levels = len(set(alpha))
    print("make_hud_icons: %s %d x %d, %d icons, %d alpha levels" % (path, WIDTH, HEIGHT, len(ICONS), levels))


if __name__ == "__main__":
    main()
