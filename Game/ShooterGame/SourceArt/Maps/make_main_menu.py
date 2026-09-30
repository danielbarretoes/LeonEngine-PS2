"""Writes MainMenu.glb, the main menu's backdrop (ps2-polish P9): a corner of a desert town in de_leon's colours.

Standard-library Python only, so the source art is reproducible without Blender (the glTF writer is the engine's
AxisTest map's, Engine/SourceArt/Maps/MakeAxisTest.py):

    python Game/ShooterGame/SourceArt/Maps/make_main_menu.py

The scene, in glTF's frame (right-handed, +Y up, metres; the importer makes glTF (x, y, z) m the engine's
(x, z, y) * 100 cm). The menu's camera is the player start (AShooterGame_Menu spawns no pawn), looking along +X at a
sandstone wall with a dark doorway, two stacks of crates and a house on the right, the sun behind the camera's left.
The map has no bomb site, buy zone nor team start: DefaultEditor.ini's MapsWithoutRequiredTags lets it import.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "Engine", "SourceArt", "Maps"))

from MakeAxisTest import GltfWriter, normalized, rotation_from_to  # noqa: E402


def main():
    gltf = GltfWriter()
    sand = gltf.material("MainMenu_Sand", (0.74, 0.62, 0.43))
    sandstone = gltf.material("MainMenu_Sandstone", (0.82, 0.71, 0.53))
    trim = gltf.material("MainMenu_Trim", (0.62, 0.52, 0.37))
    wood = gltf.material("MainMenu_CrateWood", (0.52, 0.36, 0.19))
    door = gltf.material("MainMenu_Door", (0.2, 0.18, 0.16))

    floor = gltf.plane("MainMenu_Floor", (14.0, 14.0), sand)
    wall = gltf.box("MainMenu_Wall", (0.5, 2.5, 9.0), sandstone)
    wall_trim = gltf.box("MainMenu_WallTrim", (0.6, 0.15, 9.0), trim)
    doorway = gltf.box("MainMenu_Doorway", (0.05, 1.3, 1.1), door)
    side_wall = gltf.box("MainMenu_SideWall", (4.0, 1.75, 0.4), sandstone)
    house = gltf.box("MainMenu_House", (2.5, 3.5, 2.5), sandstone)
    house_trim = gltf.box("MainMenu_HouseTrim", (2.6, 0.15, 2.6), trim)
    crate = gltf.box("MainMenu_Crate", (0.55, 0.55, 0.55), wood)

    # The sun shines along its node's -Z: down, from behind the camera's left.
    sun_direction = normalized((0.6, -1.0, 0.35))
    sun = gltf.light({"name": "Sun", "type": "directional", "color": [1.0, 0.95, 0.85], "intensity": 1.0})
    roots = [
        gltf.node("Floor", mesh=floor),
        gltf.node("Wall", translation=(7.0, 2.5, 0.0), mesh=wall),
        gltf.node("WallTrim", translation=(7.0, 5.0, 0.0), mesh=wall_trim),
        gltf.node("Doorway", translation=(6.45, 1.3, -1.5), mesh=doorway),
        gltf.node("SideWall_Left", translation=(3.0, 1.75, -6.5), mesh=side_wall),
        gltf.node("House", translation=(4.0, 3.5, 5.5), mesh=house),
        gltf.node("HouseTrim", translation=(4.0, 7.0, 5.5), mesh=house_trim),
        gltf.node("Crate_1", translation=(3.2, 0.55, -2.4), mesh=crate),
        gltf.node("Crate_2", translation=(3.2, 1.65, -2.4), mesh=crate),
        gltf.node("Crate_3", translation=(3.3, 0.55, -1.2), rotation=rotation_from_to(
            (1, 0, 0), normalized((math.cos(0.3), 0.0, math.sin(0.3)))), mesh=crate),
        gltf.node("Crate_4", translation=(1.8, 0.55, 2.6), mesh=crate),
        gltf.node("PlayerStart", translation=(-5.0, 1.6, 0.0)),
        gltf.node("Sun", translation=(0, 12, 0), rotation=rotation_from_to((0, 0, -1), sun_direction), light=sun),
    ]
    path = os.path.join(HERE, "MainMenu.glb")
    gltf.write_glb(path, roots)
    print("wrote", path, os.path.getsize(path), "bytes")


if __name__ == "__main__":
    main()
