"""Builds ShooterGame's third-person skeleton and animations with leon_art (Docs/ART_PIPELINE.md, ps2-shipping N27):
the shared 23-bone skeleton SKEL_Body and every third-person clip; the teams' palettes, which the first-person arms
(make_arms.py) paint with.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Characters/make_characters.py [-- --out <folder>]

Writes next to this script (or to <folder>):

- Body_Animations.glb: the skeleton and every third-person clip (anim_body.py) at 30 fps, with their loop flags and
  notifies in the animations' extras.
- characters.blend: the scene the file was exported from (for looking; the script is the source).

The skeleton is leon_art.HUMANOID_BONES: standing on the origin, facing +X, T pose, with the socket Weapon_R on hand_r
where the weapon's grip goes. The bodies skinned to it are the Counter-Strike 1.6 models (make_cs16_characters.py,
Body_CT.glb and Body_T.glb); ImportList.ini imports the clips into /Game/Characters/Animations. Only Blender's own
modules are used; the art is this repository's (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, os.pardir))
sys.path.insert(0, HERE)

import leon_art  # noqa: E402 (after the path)
import anim_body  # noqa: E402

# The teams' colours: the first-person arms (make_arms.py) are painted with them.
CT = {
    "camo": ((96, 108, 122), (70, 80, 94), (128, 138, 152)),
    "vest": ((46, 54, 66), (34, 40, 50), (64, 74, 88)),
    "glove": ((34, 34, 36), (56, 56, 60)),
    "boot": ((32, 30, 28), (52, 50, 46)),
    "sole": (18, 18, 18),
    "helmet": ((60, 72, 86), (46, 56, 68), (80, 92, 106)),
    "skin": ((214, 170, 136), (182, 138, 106)),
    "hair": (70, 54, 40),
    "belt": (40, 40, 42),
    "metal": (150, 150, 140),
}
T = {
    "jacket": ((116, 86, 56), (88, 64, 40), (142, 110, 74)),
    "camo": ((96, 100, 62), (72, 76, 44), (124, 126, 86)),
    "mask": ((38, 36, 34), (58, 54, 50)),
    "glove": ((84, 60, 40), (108, 80, 54)),
    "boot": ((72, 54, 36), (96, 74, 50)),
    "sole": (26, 22, 18),
    "pack": ((88, 92, 58), (66, 70, 42), (110, 112, 76)),
    "strap": (52, 44, 34),
    "skin": ((200, 150, 112), (168, 122, 88)),
    "belt": (46, 40, 32),
    "metal": (170, 150, 90),
}


def camo(x, y, colours, seed, cell=4):
    """A pixelated camouflage: blotches of the dark and the light colour on the base, cell texels a blotch."""
    base, dark, light = colours
    blotch = leon_art.noise(x // cell, y // cell, seed)
    if blotch < 0.28:
        return dark
    if blotch > 0.78:
        return light
    return base


def build():
    leon_art.reset_scene()
    armature = leon_art.build_armature("Body", leon_art.HUMANOID_BONES)
    sockets = [leon_art.add_socket(armature, name, bone, location, axes=axes)
               for name, bone, location, axes in leon_art.HUMANOID_SOCKETS]
    anim_body.add_clips(armature)
    return armature, sockets


def export(armature, sockets, out):
    leon_art.export_glb(os.path.join(out, "Body_Animations.glb"), objects=[armature] + sockets, extras=True)
    leon_art.save_blend(os.path.join(out, "characters.blend"))


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    export(*build(), out)
    print("make_characters: wrote the skeleton and the clips in %s" % out)
