"""Builds ShooterGame's first-person arms with leon_art (Docs/ART_PIPELINE.md, ps2-shipping N27): the teams' arms on the
11-bone arms skeleton, and every weapon's first-person clips.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Characters/make_arms.py [-- --out <folder>]

Writes next to this script (or to <folder>):

- Arms_CT.glb: the counter-terrorist's sleeves (the blue-grey camouflage to the wrist) and black gloves;
- Arms_T.glb: the terrorist's brown jacket, its sleeves rolled up over bare forearms, and brown gloves;
- Arms_Animations.glb: the skeleton and the clips of every weapon (anim_arms.py), `<Weapon>_<Clip>`;
- arms.blend: the scene they were exported from.

Each is skinned to leon_art.ARMS_BONES (the origin at the eye, looking along +X), two weights a vertex, with one
128 x 128 texture painted by code (P8) and the socket Weapon_R on hand_r where the weapon's grip goes. The fingers
and the thumb are rigid on their bones (fingers_01, thumb_01), so a clip closes the hand by turning them.
ImportList.ini imports the arms as /Game/Characters/Arms/SK_Arms_CT and SK_Arms_T on SKEL_Arms and the clips into
/Game/Characters/Arms/Animations. Only Blender's own modules are used; the art is this repository's
(Game/ShooterGame/SourceArt/LICENSES.md).
"""

import os
import sys

from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, os.pardir))
sys.path.insert(0, HERE)

import leon_art  # noqa: E402 (after the path)
import anim_arms  # noqa: E402
import make_characters  # noqa: E402 (the teams' colours and camouflage)

SIZE = 128
R_SLEEVE = (0, 64, 64, 128)
R_CUFF = (64, 96, 128, 128)
R_SKIN = (64, 64, 96, 96)
R_PALM = (0, 0, 64, 64)
R_FINGER = (64, 32, 96, 64)
R_THUMB = (96, 32, 128, 64)
R_KNUCKLE = (64, 0, 128, 32)
R_WATCH = (96, 64, 128, 96)
REGIONS = [R_SLEEVE, R_CUFF, R_SKIN, R_PALM, R_FINGER, R_THUMB, R_KNUCKLE, R_WATCH]

# First-person arms, both (Docs/ART_PIPELINE.md).
BUDGET = 600
CT = make_characters.CT
T = make_characters.T


def region_of(x, y):
    for region in REGIONS:
        if region[0] <= x < region[2] and region[1] <= y < region[3]:
            return region
    return None


def paint(team):
    colours = CT if team == "CT" else T

    def painter(x, y):
        r = region_of(x, y)
        if r is None:
            return (0, 0, 0)
        lx, ly = x - r[0], y - r[1]
        u = lx / (r[2] - r[0] - 1)
        if r == R_SLEEVE:
            if team == "CT":
                return make_characters.camo(x, y, CT["camo"], 13)
            if abs(u - 0.5) < 0.03:
                return T["jacket"][1]
            return T["jacket"][2] if leon_art.noise(x, y, 34) > 0.9 else T["jacket"][0]
        if r == R_CUFF:
            if team == "CT":
                return CT["camo"][1] if ly % 8 else CT["camo"][0]
            return T["jacket"][1] if ly % 4 else T["jacket"][2]
        if r == R_SKIN:
            if team == "CT":  # unused by the counter-terrorist: the glove, so no skin bleeds into its mips
                return CT["glove"][0]
            return colours["skin"][1] if leon_art.noise(lx, ly, 41) > 0.93 else colours["skin"][0]
        if r in (R_PALM, R_FINGER, R_THUMB):
            glove = colours["glove"]
            return glove[1] if (lx + ly) % 11 == 0 or ly % 16 == 0 else glove[0]
        if r == R_KNUCKLE:
            glove = colours["glove"]
            return glove[1] if ly % 6 < 2 else glove[0]
        if r == R_WATCH:  # the terrorist's watch strap; the counter-terrorist's glove cuff
            if team == "CT":
                return (24, 24, 26) if ly % 5 else (50, 50, 54)
            return (40, 34, 26) if not (10 <= lx <= 21 and 10 <= ly <= 21) else (150, 150, 140)
        return (0, 0, 0)
    return painter


def bone_point(name, fraction):
    table = {entry[0]: entry for entry in leon_art.ARMS_BONES}
    head, tail = Vector(table[name][2]), Vector(table[name][3])
    return tuple(head.lerp(tail, fraction))


def ring(c, r, w):
    return {"c": c, "r": r, "w": w}


def add_arm(mesh, team, side):
    upper, lower, hand = "upperarm_" + side, "lowerarm_" + side, "hand_" + side
    fingers, thumb = "fingers_01_" + side, "thumb_01_" + side
    mix = [(upper, 0.5), (lower, 0.5)]
    up = (0.0, 0.0, 1.0)
    rolled = team == "T"
    sleeve = [
        ring(bone_point(upper, -0.25), 0.062, upper),
        ring(bone_point(upper, 0.35), 0.060, upper),
        ring(bone_point(upper, 0.90), 0.054, upper),
        ring(bone_point(upper, 1.00), 0.052, mix),
        ring(bone_point(lower, 0.12), 0.050, lower),
    ]
    if rolled:
        sleeve.append(ring(bone_point(lower, 0.42), 0.048, lower))
    else:
        sleeve += [ring(bone_point(lower, 0.55), 0.045, lower), ring(bone_point(lower, 0.86), 0.041, lower)]
    mesh.loft(sleeve, 7, R_SLEEVE, reference=up, caps=(True, False))
    if rolled:
        # The rolled cuff, then the bare forearm to the wrist and a watch.
        mesh.loft([ring(bone_point(lower, 0.40), 0.055, lower), ring(bone_point(lower, 0.52), 0.055, lower)], 7,
                  R_CUFF, reference=up, caps=(False, False))
        mesh.loft([ring(bone_point(lower, 0.50), 0.044, lower), ring(bone_point(lower, 0.80), 0.038, lower),
                   ring(bone_point(lower, 1.00), 0.034, [(lower, 0.5), (hand, 0.5)])], 7, R_SKIN, reference=up,
                  caps=(False, True))
        mesh.loft([ring(bone_point(lower, 0.84), 0.040, lower), ring(bone_point(lower, 0.93), 0.039, lower)], 6,
                  R_WATCH, reference=up, caps=(False, False))
    else:
        # The glove's cuff over the sleeve's end.
        mesh.loft([ring(bone_point(lower, 0.80), 0.045, lower), ring(bone_point(lower, 1.00), 0.040,
                                                                          [(lower, 0.5), (hand, 0.5)])],
                  8, R_WATCH, reference=up, caps=(False, True))
    # The hand: the palm along the hand bone, the thumb's side up at rest (Weapon_R's z), the palm inward.
    table = {entry[0]: entry for entry in leon_art.ARMS_BONES}
    head, tail = Vector(table[hand][2]), Vector(table[hand][3])
    x = (tail - head).normalized()
    thumb_side = leon_art._perpendicular(up, x)
    palm_profile = [(0.010, -0.040), (0.017, -0.030), (0.017, 0.030), (0.010, 0.040), (-0.010, 0.040),
                    (-0.017, 0.030), (-0.017, -0.030), (-0.010, -0.040)]
    mesh.prism(head - x * 0.005, tail + x * 0.004, palm_profile, R_PALM, hand, up=up, taper=(1.0, 1.08))
    # Four fingers from the knuckles, index (by the thumb) to little finger, on fingers_01.
    fhead, ftail = Vector(table[fingers][2]), Vector(table[fingers][3])
    fx = (ftail - fhead).normalized()
    finger = [(0.008, -0.008), (0.008, 0.008), (0.0, 0.010), (-0.008, 0.008), (-0.008, -0.008), (0.0, -0.010)]
    for offset, length in ((0.028, 0.072), (0.009, 0.080), (-0.010, 0.076), (-0.028, 0.062)):
        start = fhead + thumb_side * offset - fx * 0.004
        mesh.prism(start, start + fx * length, finger, R_FINGER, fingers, up=up, taper=(0.85, 0.85))
    mesh.prism(fhead + thumb_side * 0.0 - fx * 0.006, fhead + fx * 0.004, [(0.012, -0.041), (0.012, 0.041),
                                                                            (-0.012, 0.041), (-0.012, -0.041)],
               R_KNUCKLE, fingers, up=up)
    thead, ttail = Vector(table[thumb][2]), Vector(table[thumb][3])
    tx = (ttail - thead).normalized()
    thumb_profile = [(0.010, -0.009), (0.010, 0.009), (0.0, 0.012), (-0.010, 0.009), (-0.010, -0.009),
                     (0.0, -0.012)]
    mid = thead.lerp(ttail, 0.55)
    mesh.prism(thead - tx * 0.01, mid, thumb_profile, R_THUMB, thumb, up=up)
    mesh.prism(mid, ttail + tx * 0.012, thumb_profile, R_THUMB, thumb, up=up, taper=(0.8, 0.8))


def build_arms(team, armature):
    name = "Arms_" + team
    image = leon_art.paint_image(name + "_D", SIZE, SIZE, paint(team))
    assert leon_art.count_colours(image) <= 256, "the arms' texture must stay P8"
    material = leon_art.make_material(name, roughness=0.8, image=image, surface="Flesh")
    mesh = leon_art.MeshBuilder(SIZE, SIZE)
    for side in ("r", "l"):
        add_arm(mesh, team, side)
    arms = mesh.build(name, material)
    count = leon_art.check_triangles(arms, BUDGET)
    leon_art.skin_rigid(arms, armature, mesh.vertex_bones())
    print("make_arms: %s has %d triangles" % (name, count))
    return arms


def build():
    leon_art.reset_scene()
    armature = leon_art.build_armature("Arms", leon_art.ARMS_BONES)
    sockets = [leon_art.add_socket(armature, name, bone, location, axes=axes)
               for name, bone, location, axes in leon_art.ARMS_SOCKETS]
    arms = {team: build_arms(team, armature) for team in ("CT", "T")}
    anim_arms.add_clips(armature)
    return armature, sockets, arms


def export(armature, sockets, arms, out):
    for team, mesh in arms.items():
        leon_art.export_glb(os.path.join(out, "Arms_%s.glb" % team), objects=[armature] + sockets + [mesh],
                            extras=True, animations=False)
    leon_art.export_glb(os.path.join(out, "Arms_Animations.glb"), objects=[armature] + sockets, extras=True)
    leon_art.save_blend(os.path.join(out, "arms.blend"))


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    export(*build(), out)
    print("make_arms: wrote the arms in %s" % out)
