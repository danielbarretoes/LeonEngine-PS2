"""Builds the art pipeline's samples with leon_art (Docs/ART_PIPELINE.md, ps2-shipping N26's gate):

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Samples/make_art_samples.py [-- --out <folder>]

Writes next to this script (or to <folder>):

- ArtSample_Crate.glb: a 0.6 m crate, a painted 64 x 64 texture of 5 colours (P4 once cooked), a 2-bone skin (Base,
  and Lid hinged at the back edge; the hinge strip weighted half and half), a socket SOCKET_Top on the lid and one
  clip, Open (1 s, one-shot, the notify Creak at frame 2): the lid opens 80 degrees.
- ArtSample_Mannequin.glb: the CT / T bodies' shared skeleton (leon_art.HUMANOID_BONES, 23 bones) skinned rigidly to a
  box per bone (264 triangles), the socket Weapon_R on hand_r and one clip, Idle (2 s, looping): arms down, breathing.

They are samples, not game content: nothing in ImportList.ini imports them. Game/ShooterGame/SourceArt/
check_art_determinism.py exports them twice and compares the bytes with each other and with the files here; LeonCook
imports them into a scratch project (Docs/ART_PIPELINE.md). No .blend is kept: the script is the source.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir))

import leon_art  # noqa: E402 (after the path)

HERE = os.path.dirname(os.path.abspath(__file__))

# The planks' colours (sRGB bytes): P4 holds 16.
WOOD_DARK = (74, 46, 22)
WOOD_LIGHT = (156, 108, 58)
WOOD_MID = (128, 86, 44)
METAL = (96, 100, 104)
NAIL = (40, 40, 44)


def paint_planks(x, y):
    """Three horizontal planks with dark seams, a metal band around the middle and a nail at each band end."""
    if y in (0, 21, 42, 63):
        return WOOD_DARK
    if 28 <= y <= 35:
        return NAIL if (x in (4, 5, 58, 59) and y in (31, 32)) else METAL
    return WOOD_LIGHT if (y // 21) % 2 == 0 else WOOD_MID


def build_crate():
    leon_art.reset_scene()
    texture = leon_art.paint_image("ArtSample_Crate_D", 64, 64, paint_planks)
    assert leon_art.count_colours(texture) <= 16, "the crate's texture must stay P4"
    material = leon_art.make_material("ArtSample_Crate", roughness=0.8, image=texture)

    armature = leon_art.build_armature("ArtSample_Crate", [
        ("Base", None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.3)),
        ("Lid", "Base", (-0.3, 0.0, 0.5), (-0.3, 0.0, 0.6)),
    ])
    # Blender metres: the body, the lid and the hinge strip at the back (8 vertices a box, in this order).
    boxes = [
        (leon_art.to_blender(0.0, 0.0, 0.24), (0.6, 0.6, 0.48)),
        (leon_art.to_blender(0.0, 0.0, 0.55), (0.64, 0.64, 0.1)),
        (leon_art.to_blender(-0.31, 0.0, 0.5), (0.04, 0.5, 0.08)),
    ]
    mesh = leon_art.mesh_from_boxes("ArtSample_Crate_Mesh", boxes, [material])
    crate = leon_art.add_object("ArtSample_Crate_Mesh", mesh)
    leon_art.check_triangles(crate, 300)
    leon_art.skin_rigid(crate, armature, ["Base"] * 8 + ["Lid"] * 8 + [[("Base", 0.5), ("Lid", 0.5)]] * 8)
    leon_art.add_socket(armature, "Top", "Lid", (0.0, 0.0, 0.6))

    # The lid turns about the hinge (the Lid bone's head, along Y): about Blender's -Y lifts its front edge.
    leon_art.add_action(armature, "Open", [
        (0, {"Lid": (((0.0, -1.0, 0.0), 0.0), None)}),
        (30, {"Lid": (((0.0, -1.0, 0.0), 80.0), None)}),
    ], notifies=[(2, "Creak")], loop=False)


def segment_box(head, tail, thickness):
    """The box around a bone from head to tail (engine metres), `thickness` wide across it; Blender centre and size."""
    center = [(h + t) * 0.5 for h, t in zip(head, tail)]
    size = [max(abs(t - h), thickness) for h, t in zip(head, tail)]
    return leon_art.to_blender(*center), tuple(size)


def build_mannequin():
    leon_art.reset_scene()
    material = leon_art.make_material("ArtSample_Mannequin", (0.45, 0.45, 0.5), roughness=0.7)
    armature = leon_art.build_armature("ArtSample_Mannequin", leon_art.HUMANOID_BONES)
    thickness = {"pelvis": 0.30, "spine_01": 0.28, "spine_02": 0.32, "spine_03": 0.36, "head": 0.22, "neck_01": 0.10}
    boxes = []
    bones = []
    for name, _parent, head, tail in leon_art.HUMANOID_BONES[1:]:
        boxes.append(segment_box(head, tail, thickness.get(name, 0.12)))
        bones.extend([name] * 8)
    mesh = leon_art.mesh_from_boxes("ArtSample_Mannequin_Mesh", boxes, [material])
    body = leon_art.add_object("ArtSample_Mannequin_Mesh", mesh)
    leon_art.check_triangles(body, 1000)
    leon_art.skin_rigid(body, armature, bones)
    for name, bone, location, axes in leon_art.HUMANOID_SOCKETS:
        leon_art.add_socket(armature, name, bone, location, axes=axes)

    # Arms down 60 degrees (about Blender's X: the left arm is on +Y) and the chest leaning 3 degrees forward.
    arms = {"upperarm_l": (((1.0, 0.0, 0.0), -60.0), None), "upperarm_r": (((1.0, 0.0, 0.0), 60.0), None)}
    leon_art.add_action(armature, "Idle", [
        (0, dict(arms, spine_03=(((0.0, 1.0, 0.0), 0.0), None))),
        (30, dict(arms, spine_03=(((0.0, 1.0, 0.0), 3.0), None))),
        (60, dict(arms, spine_03=(((0.0, 1.0, 0.0), 0.0), None))),
    ], loop=True)


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    for name, build in (("ArtSample_Crate", build_crate), ("ArtSample_Mannequin", build_mannequin)):
        build()
        leon_art.export_glb(os.path.join(out, name + ".glb"), extras=True)
    print("make_art_samples: wrote the samples in %s" % out)
