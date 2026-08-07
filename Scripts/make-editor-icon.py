#!/usr/bin/env python3
"""Regenerate Editor/Resources/Icons/LeonEditor.ico as BMP/DIB (rc.exe-compatible).

PNG-compressed ICO entries are rejected or ignored by older rc.exe / Explorer embeds.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow required (pip install pillow)", file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Editor" / "Resources" / "Brand" / "LeonLogo.png"
OUT_ICO = ROOT / "Editor" / "Resources" / "Icons" / "LeonEditor.ico"
OUT_PNG = ROOT / "Editor" / "Resources" / "Icons" / "LeonEditor.png"


def crop_logo(im: Image.Image) -> Image.Image:
    im = im.convert("RGBA")
    pixels = im.load()
    w, h = im.size
    minx, miny, maxx, maxy = w, h, 0, 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a > 10 and (r + g + b) > 40:
                minx = min(minx, x)
                miny = min(miny, y)
                maxx = max(maxx, x)
                maxy = max(maxy, y)
    cropped = im.crop((minx, miny, maxx + 1, maxy + 1))
    cw, ch = cropped.size
    side = max(cw, ch)
    canvas = Image.new("RGBA", (side, side), (18, 18, 18, 255))
    canvas.paste(cropped, ((side - cw) // 2, (side - ch) // 2), cropped)
    pad = int(side * 0.06)
    padded = Image.new("RGBA", (side + 2 * pad, side + 2 * pad), (18, 18, 18, 255))
    padded.paste(canvas, (pad, pad))
    return padded


def rgba_to_ico_dib(im: Image.Image) -> bytes:
    im = im.convert("RGBA")
    width, height = im.size
    row_bytes = width * 4
    xor = bytearray(row_bytes * height)
    px = im.load()
    for y in range(height):
        src_y = height - 1 - y
        for x in range(width):
            r, g, b, a = px[x, src_y]
            i = y * row_bytes + x * 4
            xor[i] = b
            xor[i + 1] = g
            xor[i + 2] = r
            xor[i + 3] = a
    and_row = ((width + 31) // 32) * 4
    and_mask = bytes(and_row * height)
    header = struct.pack(
        "<IIIHHIIIIII",
        40,
        width,
        height * 2,
        1,
        32,
        0,
        len(xor),
        0,
        0,
        0,
        0,
    )
    return header + bytes(xor) + and_mask


def main() -> int:
    if not SRC.is_file():
        print(f"ERROR: missing {SRC}", file=sys.stderr)
        return 1
    padded = crop_logo(Image.open(SRC))
    sizes = [256, 128, 64, 48, 32, 16]
    images: list[tuple[int, bytes]] = []
    for s in sizes:
        dib = rgba_to_ico_dib(padded.resize((s, s), Image.Resampling.LANCZOS))
        images.append((s, dib))

    offset = 6 + 16 * len(images)
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    payload = bytearray()
    for s, dib in images:
        wb = 0 if s >= 256 else s
        hb = 0 if s >= 256 else s
        out += struct.pack("<BBBBHHII", wb, hb, 0, 0, 1, 32, len(dib), offset)
        payload += dib
        offset += len(dib)
    out += payload

    OUT_ICO.parent.mkdir(parents=True, exist_ok=True)
    OUT_ICO.write_bytes(bytes(out))
    padded.resize((256, 256), Image.Resampling.LANCZOS).save(OUT_PNG)
    print(f"Wrote BMP-ICO {OUT_ICO} ({OUT_ICO.stat().st_size} bytes)")
    print(f"Wrote PNG     {OUT_PNG}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
