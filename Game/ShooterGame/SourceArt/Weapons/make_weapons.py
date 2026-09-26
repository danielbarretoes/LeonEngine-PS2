"""Builds ShooterGame's weapon meshes as glTF binaries (.glb) for LeonEd's static mesh import.

    python3 Game/ShooterGame/SourceArt/Weapons/make_weapons.py

Writes Pistol.glb, Rifle.glb, Sniper.glb, Grenade.glb and C4.glb next to this script;
Game/ShooterGame/SourceArt/ImportList.ini imports them as /Game/Weapons/SM_Pistol, SM_Rifle, SM_Sniper, SM_Grenade and
SM_C4 with their materials (M_WeaponMetal, M_WeaponWood, M_WeaponPolymer, M_WeaponOlive, M_WeaponGlass, M_BombDisplay).
AShooterWeapon shows the weapons as the view model and on the bodies, AShooterBomb the bomb on the floor
(DefaultGame.ini: MeshName of each class).

Each weapon is a few boxes, the grip at the origin, the barrel along +X (the engine's forward), +Z up, with a
`SOCKET_Muzzle` node at the muzzle (the static mesh import makes it the mesh's `Muzzle` socket). Sizes are in
centimetres below and written in metres. glTF is right-handed Y-up: the engine's (X, Y, Z) is glTF's (X, Z, Y), so a
point is written as (x, z, y) and every box keeps its faces counter-clockwise from outside in glTF's frame.

Only the Python standard library is used, and the output is deterministic (the reimport gate G5 compares the imported
assets byte for byte): no timestamps, fixed float formatting, sorted JSON keys. No external art
(Game/ShooterGame/SourceArt/LICENSES.md).
"""

import json
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))

# Base colours (linear RGB) and roughness of the materials the weapons share.
MATERIALS = {
    "WeaponMetal": ((0.22, 0.22, 0.24), 0.45, 0.6),
    "WeaponWood": ((0.28, 0.12, 0.04), 0.7, 0.0),
    "WeaponPolymer": ((0.1, 0.1, 0.1), 0.8, 0.0),
    "WeaponOlive": ((0.17, 0.2, 0.08), 0.75, 0.0),
    "WeaponGlass": ((0.1, 0.25, 0.3), 0.1, 0.0),
    "BombDisplay": ((0.1, 0.45, 0.12), 0.3, 0.0),
}

# Boxes: (material, (min x, min y, min z), (max x, max y, max z)) in engine centimetres; the muzzle in centimetres.
WEAPONS = {
    "C4": {
        "boxes": [
            ("WeaponOlive", (-12.0, -8.0, 0.0), (12.0, 8.0, 6.0)),  # the charge
            ("WeaponPolymer", (-6.0, -5.0, 6.0), (6.0, 5.0, 8.0)),  # the keypad
            ("BombDisplay", (-5.0, -2.0, 8.0), (1.0, 2.0, 8.5)),  # the display
            ("WeaponMetal", (-12.5, -8.5, 2.0), (12.5, 8.5, 3.0)),  # the tape
        ],
        "muzzle": (0.0, 0.0, 8.5),
    },
    "Pistol": {
        "boxes": [
            ("WeaponMetal", (-3.0, -1.4, 0.0), (17.0, 1.4, 3.6)),  # slide
            ("WeaponMetal", (16.0, -0.6, 1.2), (19.0, 0.6, 2.4)),  # barrel tip
            ("WeaponPolymer", (-2.0, -1.3, -2.0), (13.0, 1.3, 0.0)),  # frame
            ("WeaponPolymer", (-2.5, -1.5, -11.0), (3.5, 1.5, -2.0)),  # grip
            ("WeaponPolymer", (3.5, -0.4, -4.0), (7.0, 0.4, -3.4)),  # trigger guard
        ],
        "muzzle": (19.0, 0.0, 1.8),
    },
    "Rifle": {
        "boxes": [
            ("WeaponMetal", (-4.0, -1.8, -1.0), (30.0, 1.8, 5.0)),  # receiver
            ("WeaponMetal", (30.0, -0.8, 1.6), (62.0, 0.8, 3.2)),  # barrel
            ("WeaponMetal", (58.0, -0.4, 3.2), (60.0, 0.4, 6.0)),  # front sight
            ("WeaponWood", (30.0, -2.0, -0.6), (46.0, 2.0, 4.0)),  # handguard
            ("WeaponWood", (-30.0, -1.8, -6.0), (-4.0, 1.8, 3.0)),  # stock
            ("WeaponWood", (-4.0, -1.6, -11.0), (2.0, 1.6, -1.0)),  # grip
            ("WeaponMetal", (10.0, -1.4, -16.0), (17.0, 1.4, -1.0)),  # magazine
        ],
        "muzzle": (62.0, 0.0, 2.4),
    },
    "Sniper": {
        "boxes": [
            ("WeaponOlive", (-4.0, -2.2, -1.5), (40.0, 2.2, 4.5)),  # body
            ("WeaponMetal", (40.0, -1.0, 1.0), (86.0, 1.0, 3.0)),  # barrel
            ("WeaponMetal", (80.0, -1.4, 0.6), (88.0, 1.4, 3.4)),  # muzzle brake
            ("WeaponOlive", (-34.0, -2.0, -7.0), (-4.0, 2.0, 3.5)),  # stock
            ("WeaponOlive", (-4.0, -1.8, -11.0), (2.0, 1.8, -1.5)),  # grip
            ("WeaponMetal", (8.0, -1.3, -9.0), (15.0, 1.3, -1.5)),  # magazine
            ("WeaponMetal", (4.0, -2.0, 5.0), (32.0, 2.0, 9.0)),  # scope tube
            ("WeaponGlass", (32.0, -2.4, 4.6), (34.0, 2.4, 9.4)),  # objective lens
            ("WeaponMetal", (14.0, -0.8, 4.5), (18.0, 0.8, 5.0)),  # scope mount
        ],
        "muzzle": (88.0, 0.0, 2.0),
    },
    "Grenade": {
        "boxes": [
            ("WeaponOlive", (-3.2, -3.2, -4.0), (3.2, 3.2, 4.0)),  # body
            ("WeaponMetal", (-1.2, -1.2, 4.0), (1.2, 1.2, 6.0)),  # fuse
            ("WeaponMetal", (1.2, -0.5, 1.0), (1.8, 0.5, 6.0)),  # lever
        ],
        "muzzle": (0.0, 0.0, 6.0),
    },
}

CM = 0.01


def to_gltf(point_cm):
    """Engine centimetres (x, y, z) to glTF metres (x, z, y)."""
    x, y, z = point_cm
    return (round(x * CM, 6), round(z * CM, 6), round(y * CM, 6))


def box_faces(lo, hi):
    """The six faces of a box in glTF space: (normal, four corners counter-clockwise from outside)."""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    return [
        ((1.0, 0.0, 0.0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        ((-1.0, 0.0, 0.0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
        ((0.0, 1.0, 0.0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
        ((0.0, -1.0, 0.0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
        ((0.0, 0.0, 1.0), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
        ((0.0, 0.0, -1.0), [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)]),
    ]


def build_primitive_data(boxes):
    """Positions, normals and indices of boxes (engine centimetres) in glTF space."""
    positions, normals, indices = [], [], []
    for lo_cm, hi_cm in boxes:
        a, b = to_gltf(lo_cm), to_gltf(hi_cm)
        lo = tuple(min(a[i], b[i]) for i in range(3))
        hi = tuple(max(a[i], b[i]) for i in range(3))
        for normal, corners in box_faces(lo, hi):
            base = len(positions)
            positions.extend(corners)
            normals.extend([normal] * 4)
            indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return positions, normals, indices


def pad4(data, byte=b"\x00"):
    return data + byte * ((4 - len(data) % 4) % 4)


def write_glb(name, weapon):
    by_material = {}
    for material, lo, hi in weapon["boxes"]:
        by_material.setdefault(material, []).append((lo, hi))
    material_names = sorted(by_material)

    buffer = bytearray()
    buffer_views, accessors, primitives = [], [], []

    def add_view(data, target):
        offset = len(buffer)
        buffer.extend(pad4(data))
        buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
        return len(buffer_views) - 1

    for material_index, material in enumerate(material_names):
        positions, normals, indices = build_primitive_data(by_material[material])
        pos_view = add_view(b"".join(struct.pack("<3f", *p) for p in positions), 34962)
        nrm_view = add_view(b"".join(struct.pack("<3f", *n) for n in normals), 34962)
        idx_view = add_view(b"".join(struct.pack("<H", i) for i in indices), 34963)
        mins = [min(p[i] for p in positions) for i in range(3)]
        maxs = [max(p[i] for p in positions) for i in range(3)]
        accessors.append({"bufferView": pos_view, "componentType": 5126, "count": len(positions), "type": "VEC3",
                          "min": mins, "max": maxs})
        accessors.append({"bufferView": nrm_view, "componentType": 5126, "count": len(normals), "type": "VEC3"})
        accessors.append({"bufferView": idx_view, "componentType": 5123, "count": len(indices), "type": "SCALAR"})
        primitives.append({"attributes": {"POSITION": len(accessors) - 3, "NORMAL": len(accessors) - 2},
                           "indices": len(accessors) - 1, "material": material_index, "mode": 4})

    materials = []
    for material in material_names:
        rgb, roughness, metallic = MATERIALS[material]
        materials.append({"name": material, "pbrMetallicRoughness": {
            "baseColorFactor": [rgb[0], rgb[1], rgb[2], 1.0], "metallicFactor": metallic,
            "roughnessFactor": roughness}})

    gltf = {
        "asset": {"generator": "ShooterGame make_weapons.py", "version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": name, "mesh": 0, "children": [1]},
            {"name": "SOCKET_Muzzle", "translation": list(to_gltf(weapon["muzzle"]))},
        ],
        "meshes": [{"name": name, "primitives": primitives}],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(buffer)}],
    }
    json_chunk = pad4(json.dumps(gltf, sort_keys=True, separators=(",", ":")).encode("utf-8"), b" ")
    bin_chunk = bytes(buffer)
    total = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    out = bytearray()
    out += struct.pack("<4sII", b"glTF", 2, total)
    out += struct.pack("<I4s", len(json_chunk), b"JSON") + json_chunk
    out += struct.pack("<I4s", len(bin_chunk), b"BIN\x00") + bin_chunk
    path = os.path.join(HERE, name + ".glb")
    with open(path, "wb") as file:
        file.write(out)
    print("wrote", os.path.relpath(path))


def main():
    for name in sorted(WEAPONS):
        write_glb(name, WEAPONS[name])


if __name__ == "__main__":
    main()
