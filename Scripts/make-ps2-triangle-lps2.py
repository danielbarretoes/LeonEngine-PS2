#!/usr/bin/env python3
"""Host helper: write a tiny LPS2 triangle for Ps2Lab / PCSX2 hostfs tests."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Projects" / "Ps2Lab" / "Content" / "Meshes" / "SM_Triangle.lps2"


def main() -> int:
    # Unit triangle in XY, z=0
    verts = [
        (-0.5, -0.5, 0.0),
        (0.5, -0.5, 0.0),
        (0.0, 0.5, 0.0),
    ]
    indices = [0, 1, 2]
    header = struct.pack("<4sIII", b"LPS2", 1, len(verts), len(indices))
    body = b"".join(struct.pack("<fff", *v) for v in verts)
    body += b"".join(struct.pack("<H", i) for i in indices)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(header + body)
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
