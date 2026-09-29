"""Writes CellsFixture.gltf, the map importer's cells and portals scene (Leon, Docs/PLANS/ps2-shipping.md N15).

Standard-library Python only; it reuses the glTF writer of Engine/SourceArt/Maps/MakeAxisTest.py:

    python Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/MakeCellsFixture.py

Two rooms side by side and the door between them, in glTF's frame (right-handed, +Y up, metres; the engine's position
is (x, z, y) * 100 cm):

    VIS_Hall                   a cell: the box x 0 to 10 m, z 0 to 10 m, 3 m high
    VIS_Room_B                 a cell next to it (x 10 to 20 m); its name holds an underscore
    PORTAL_Hall_Room_B         the door between them: a 1 x 2 m quad in the wall at x = 10 m, centred at z = 5 m
    PORTAL_Hall_Nowhere        a portal to a cell the map does not have: left out
    Floor, Crate_Hall, Crate_B static meshes
"""
import base64
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", ".."))
sys.path.insert(0, os.path.join(ENGINE, "SourceArt", "Maps"))
from MakeAxisTest import GltfWriter  # noqa: E402


def main():
    gltf = GltfWriter()
    grey = gltf.material("Grey", (0.5, 0.5, 0.5))
    floor = gltf.plane("Floor", (10.0, 5.0), grey)
    crate = gltf.box("Crate", (0.5, 0.5, 0.5), grey)
    room = gltf.box("RoomBox", (5.0, 1.5, 5.0), grey)
    # A vertical quad facing +X: 1 m across (z) and 2 m high (y).
    door = gltf._mesh("DoorQuad", [((0, 0, 0), (1, 0, 0), (0, 0, 0.5), (0, 1, 0))], grey)
    roots = [
        gltf.node("Floor", translation=(10, 0, 5), mesh=floor),
        gltf.node("VIS_Hall", translation=(5, 1.5, 5), mesh=room),
        gltf.node("VIS_Room_B", translation=(15, 1.5, 5), mesh=room),
        gltf.node("PORTAL_Hall_Room_B", translation=(10, 1, 5), mesh=door),
        gltf.node("PORTAL_Hall_Nowhere", translation=(0, 1, 5), mesh=door),
        gltf.node("Crate_Hall", translation=(3, 0.5, 3), mesh=crate),
        gltf.node("Crate_B", translation=(17, 0.5, 7), mesh=crate),
    ]
    doc = gltf.document(roots, "data:application/octet-stream;base64," + base64.b64encode(
        bytes(gltf.buffer)).decode("ascii"))
    with open(os.path.join(HERE, "CellsFixture.gltf"), "w", newline="\n") as f:
        f.write(json.dumps(doc, indent=1) + "\n")
    print("wrote CellsFixture.gltf in", HERE)


if __name__ == "__main__":
    main()
