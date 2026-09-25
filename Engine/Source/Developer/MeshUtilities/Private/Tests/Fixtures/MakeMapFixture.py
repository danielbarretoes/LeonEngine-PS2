"""Writes MapFixture.gltf and MapFixture_Checker.png, the map importer's test scene (Leon, plan phase P15).

Standard-library Python only; it reuses the glTF writer of Engine/SourceArt/Maps/MakeAxisTest.py:

    python Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/MakeMapFixture.py

One node of every naming convention of the map importer (Engine/Config/BaseEditor.ini and a project's rules), in glTF's
frame (right-handed, +Y up, metres; the engine's position is (x, z, y) * 100 cm):

    Floor                      a 20 m floor (no rule: a static mesh actor)
    Crate_A, Group/Crate_B     two nodes showing the mesh Crate (one SM_Crate), textured material Crate
    UCX_Crate_A_01             convex collision of Crate_A: a 1.2 m box 10 cm above the crate's centre
    COL_Wall                   collision only
    Clip_Edge                  a blocking volume (a 2 x 4 x 1 m box, scaled 2 x 1 x 1)
    PlayerStart_CT / _T        player starts facing the engine's +Y / -X
    BombSite_A / _B            trigger volumes of the project rule BombSite
    BuyZone_CT / _T            trigger volumes of the project rule BuyZone
    NavWaypoint_01..03         waypoints with extras links / flags (a string and an array)
    Sun, Lamp, Spot            KHR_lights_punctual: directional, point (5 m), spot (becomes a point light)
    Empty                      no mesh, no rule: left out
"""
import base64
import json
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", ".."))
sys.path.insert(0, os.path.join(ENGINE, "SourceArt", "Maps"))
from MakeAxisTest import GltfWriter, normalized, rotation_from_to  # noqa: E402


def write_png(path, size, colors):
    """A size x size checker of two RGB colours."""
    rows = b""
    for y in range(size):
        rows += b"\0" + b"".join(bytes(colors[(x + y) % 2]) for x in range(size))
    chunk = lambda kind, data: struct.pack(">I", len(data)) + kind + data + struct.pack(
        ">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) + chunk(
        b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    gltf = GltfWriter()
    grey = gltf.material("Grey", (0.5, 0.5, 0.5))
    crate_material = gltf.material("Crate", (1.0, 1.0, 1.0), metallic=0.25, roughness=0.6)
    gltf.materials[crate_material]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    zone = gltf.material("Zone", (0.2, 0.8, 0.2))
    floor = gltf.plane("Floor", (10.0, 10.0), grey)
    crate = gltf.box("Crate", (0.5, 0.5, 0.5), crate_material)
    crate_collision = gltf.box("CrateCollision", (0.6, 0.6, 0.6), grey)
    wall = gltf.box("WallBox", (0.1, 1.0, 2.0), grey)
    clip = gltf.box("ClipBox", (1.0, 2.0, 0.5), grey)
    zone_box = gltf.box("ZoneBox", (2.0, 1.0, 2.0), zone)
    sun = gltf.light({"name": "Sun", "type": "directional", "color": [1.0, 0.95, 0.9], "intensity": 1.5})
    lamp = gltf.light({"name": "Lamp", "type": "point", "color": [1.0, 0.8, 0.6], "intensity": 2.0, "range": 5.0})
    spot = gltf.light({"name": "Spot", "type": "spot", "color": [0.5, 0.5, 1.0], "intensity": 3.0,
                       "spot": {"innerConeAngle": 0.2, "outerConeAngle": 0.6}})
    crate_b = gltf.node("Crate_B", translation=(0, 0.5, -2), mesh=crate)
    # Yaw 90 in the engine (facing +Y, glTF +Z): -90 degrees about glTF +Y; yaw 180: 180 degrees about +Y.
    quarter = 0.70710678118654752
    roots = [
        gltf.node("Floor", mesh=floor),
        gltf.node("Crate_A", translation=(2, 0.5, 0), mesh=crate),
        gltf.node("Group", translation=(10, 0, 0), children=[crate_b]),
        gltf.node("UCX_Crate_A_01", translation=(2, 0.6, 0), mesh=crate_collision),
        gltf.node("COL_Wall", translation=(-3, 1, 0), mesh=wall),
        gltf.node("Clip_Edge", translation=(0, 1, 5), scale=(2, 1, 1), mesh=clip),
        gltf.node("PlayerStart_CT", translation=(-8, 0.92, -8), rotation=(0, -quarter, 0, quarter)),
        gltf.node("PlayerStart_T", translation=(8, 0.92, 8), rotation=(0, 1, 0, 0)),
        gltf.node("BombSite_A", translation=(8, 1, -8), mesh=zone_box),
        gltf.node("BombSite_B", translation=(-8, 1, 8), mesh=zone_box),
        gltf.node("BuyZone_CT", translation=(-8, 1, -4), mesh=zone_box),
        gltf.node("BuyZone_T.001", translation=(8, 1, 4), mesh=zone_box),
        gltf.node("NavWaypoint_01", translation=(0, 0, 0), extras={"links": "NavWaypoint_02, NavWaypoint_03",
                                                                    "flags": "Jump"}),
        gltf.node("NavWaypoint_02", translation=(4, 0, 0), extras={"links": ["NavWaypoint_01"],
                                                                    "flags": ["Crouch", "Jump"]}),
        gltf.node("NavWaypoint_03", translation=(0, 0, 4)),
        gltf.node("Sun", translation=(0, 10, 0), rotation=rotation_from_to((0, 0, -1), normalized((0.3, -1, 0.2))),
                  light=sun),
        gltf.node("Lamp", translation=(1, 3, 1), light=lamp),
        gltf.node("Spot", translation=(-1, 3, -1), light=spot),
        gltf.node("Empty", translation=(5, 5, 5)),
    ]
    doc = gltf.document(roots, "data:application/octet-stream;base64," + base64.b64encode(
        bytes(gltf.buffer)).decode("ascii"))
    doc["images"] = [{"uri": "MapFixture_Checker.png"}]
    doc["textures"] = [{"source": 0}]
    with open(os.path.join(HERE, "MapFixture.gltf"), "w", newline="\n") as f:
        f.write(json.dumps(doc, indent=1) + "\n")
    write_png(os.path.join(HERE, "MapFixture_Checker.png"), 4, [(200, 60, 40), (240, 220, 180)])
    print("wrote MapFixture.gltf and MapFixture_Checker.png in", HERE)


if __name__ == "__main__":
    main()
