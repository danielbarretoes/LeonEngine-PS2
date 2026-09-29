"""Builds ShooterGame's characters with leon_art (Docs/ART_PIPELINE.md, ps2-shipping N27): the counter-terrorist and
the terrorist on the shared 23-bone skeleton, and their third-person animations.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Characters/make_characters.py [-- --out <folder>]

Writes next to this script (or to <folder>):

- Body_CT.glb: the counter-terrorist, a blue-grey urban camouflage, a dark tactical vest with pouches, knee pads,
  black gloves and boots and a helmet; skinned to the skeleton (two weights a vertex), one 128 x 128 texture painted
  by code (P8).
- Body_T.glb: the terrorist, a brown jacket over olive cargo trousers, a black balaclava with an eye slit, brown
  gloves and boots, a backpack with a bedroll and a bandolier.
- Body_Animations.glb: the skeleton and every third-person clip (anim_body.py) at 30 fps, with their loop flags and
  notifies in the animations' extras.
- characters.blend: the scene the files were exported from (for looking; the script is the source).

The skeleton is leon_art.HUMANOID_BONES: standing on the origin, facing +X, T pose, with the socket Weapon_R on hand_r
where the weapon's grip goes. ImportList.ini imports the bodies as /Game/Characters/SK_Body_CT and SK_Body_T on
SKEL_Body, and the clips into /Game/Characters/Animations. Only Blender's own modules are used; the art is this
repository's (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, os.pardir))
sys.path.insert(0, HERE)

import leon_art  # noqa: E402 (after the path)
import anim_body  # noqa: E402

# The texture atlas of a body (128 x 128 texels, (x0, y0, x1, y1), y = 0 the bottom row).
SIZE = 128
R_TORSO = (0, 64, 64, 128)
R_LEG = (64, 64, 96, 128)
R_ARM = (96, 64, 128, 128)
R_HEAD = (0, 16, 48, 64)
R_HAND = (48, 40, 64, 64)
R_BOOT = (48, 16, 64, 40)
R_GEAR1 = (64, 32, 96, 64)
R_GEAR2 = (96, 32, 128, 64)
R_GEAR3 = (64, 0, 96, 32)
R_GEAR4 = (96, 0, 128, 32)
R_SKIN = (0, 0, 16, 16)
R_DARK = (16, 0, 32, 16)
R_SOLE = (32, 0, 48, 16)
R_METAL = (48, 0, 64, 16)
REGIONS = [R_TORSO, R_LEG, R_ARM, R_HEAD, R_HAND, R_BOOT, R_GEAR1, R_GEAR2, R_GEAR3, R_GEAR4, R_SKIN, R_DARK, R_SOLE,
           R_METAL]

# The characters' triangle budget (Docs/ART_PIPELINE.md).
BUDGET = 1000

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


def region_of(x, y):
    for region in REGIONS:
        if region[0] <= x < region[2] and region[1] <= y < region[3]:
            return region
    return None


def camo(x, y, colours, seed, cell=4):
    """A pixelated camouflage: blotches of the dark and the light colour on the base, cell texels a blotch."""
    base, dark, light = colours
    blotch = leon_art.noise(x // cell, y // cell, seed)
    if blotch < 0.28:
        return dark
    if blotch > 0.78:
        return light
    return base


def shade(colour, amount):
    return tuple(max(0, min(255, int(c + amount))) for c in colour)


def face(lx, ly, skin, eyes_only=False):
    """The face on the head's front (lx, ly texels in the head region, its centre column 24): eyes, brows, nose and
    mouth; eyes_only paints just the eye slit's features."""
    eye_y = 43
    for cx in (20, 28):
        if ly == eye_y and cx - 1 <= lx <= cx + 1:
            return (232, 226, 214) if lx != cx else (46, 56, 66)
        if ly == eye_y + 2 and cx - 2 <= lx <= cx + 1 and not eyes_only:
            return (78, 58, 40)
    if eyes_only:
        return skin[0]
    if 23 <= lx <= 25 and 36 <= ly <= 40:
        return skin[1]
    if 21 <= lx <= 27 and ly == 32:
        return (150, 96, 80)
    return skin[0]


def paint_ct(x, y):
    r = region_of(x, y)
    if r is None:
        return CT["belt"]
    lx, ly = x - r[0], y - r[1]
    v = ly / (r[3] - r[1] - 1)
    u = lx / (r[2] - r[0] - 1)
    if r == R_TORSO:
        if 0.10 <= v <= 0.19:  # the belt, a buckle at the front
            return CT["metal"] if abs(u - 0.5) < 0.04 and 0.12 <= v <= 0.17 else CT["belt"]
        if v > 0.93:  # the collar
            return CT["camo"][1]
        return camo(x, y, CT["camo"], 11)
    if r == R_LEG:
        if 0.40 <= v <= 0.46 and abs(u - 0.5) < 0.3:  # a seam above the knee pad
            return CT["camo"][1]
        if 0.62 <= v <= 0.80 and 0.62 <= u <= 0.86:  # a thigh pocket
            return shade(CT["camo"][0], -14) if (ly % 5) else CT["camo"][1]
        return camo(x, y, CT["camo"], 12)
    if r == R_ARM:
        if v > 0.90:  # the sleeve's cuff
            return CT["camo"][1]
        if 0.18 <= v <= 0.30 and 0.35 <= u <= 0.65:  # a shoulder patch
            return (40, 44, 52) if not (0.21 <= v <= 0.27 and 0.42 <= u <= 0.58) else (150, 158, 168)
        return camo(x, y, CT["camo"], 13)
    if r == R_HEAD:
        if u < 0.22 or u > 0.78 or v > 0.82:  # short hair at the back and top
            return CT["hair"]
        return face(lx, ly, CT["skin"])
    if r == R_HAND:
        return CT["glove"][1] if ly % 6 == 0 else CT["glove"][0]
    if r == R_BOOT:
        return CT["boot"][1] if (ly % 5 == 2 and 4 <= lx <= 11) else CT["boot"][0]
    if r == R_GEAR1:  # the vest: MOLLE webbing rows
        if ly % 6 == 0:
            return CT["vest"][2]
        if lx % 8 == 0:
            return CT["vest"][1]
        return CT["vest"][0]
    if r == R_GEAR2:  # the helmet
        if ly < 3:
            return CT["helmet"][1]
        return CT["helmet"][2] if leon_art.noise(lx, ly, 21) > 0.93 else CT["helmet"][0]
    if r == R_GEAR3:  # pouches
        if ly > 24:
            return CT["vest"][1]
        if lx % 16 in (0, 15):
            return CT["vest"][2]
        return CT["vest"][0]
    if r == R_GEAR4:  # knee pads
        return (38, 42, 48) if 6 <= ly <= 25 else (52, 58, 66)
    if r == R_SKIN:
        return CT["skin"][0]
    if r == R_DARK:
        return (26, 26, 28)
    if r == R_SOLE:
        return CT["sole"]
    return CT["metal"]


def paint_t(x, y):
    r = region_of(x, y)
    if r is None:
        return T["belt"]
    lx, ly = x - r[0], y - r[1]
    v = ly / (r[3] - r[1] - 1)
    u = lx / (r[2] - r[0] - 1)
    if r == R_TORSO:
        if v < 0.10:  # the trousers under the jacket's hem
            return camo(x, y, T["camo"], 31)
        if 0.10 <= v <= 0.16:  # the hem
            return T["jacket"][1]
        if abs(u - 0.5) < 0.02 and v > 0.16:  # the zip
            return T["jacket"][1]
        if 0.26 <= v <= 0.40 and (0.30 <= u <= 0.44 or 0.56 <= u <= 0.70):  # chest pockets
            return T["jacket"][1] if ly % 7 == 0 or lx % 9 == 0 else T["jacket"][2]
        return T["jacket"][2] if leon_art.noise(x, y, 32) > 0.9 else T["jacket"][0]
    if r == R_LEG:
        return camo(x, y, T["camo"], 33, 5)
    if r == R_ARM:
        if v > 0.84:  # rolled sleeves
            return T["jacket"][1] if ly % 3 else T["jacket"][2]
        if abs(u - 0.5) < 0.03:
            return T["jacket"][1]
        return T["jacket"][2] if leon_art.noise(x, y, 34) > 0.9 else T["jacket"][0]
    if r == R_HEAD:
        # The balaclava, an eye slit at the front.
        if 0.50 <= v <= 0.66 and 0.34 <= u <= 0.66:
            return face(lx, ly, T["skin"], eyes_only=True)
        if 0.49 <= v <= 0.67 and 0.32 <= u <= 0.68:
            return T["mask"][1]
        return T["mask"][1] if (lx + ly) % 7 == 0 else T["mask"][0]
    if r == R_HAND:
        return T["glove"][1] if ly % 7 == 3 else T["glove"][0]
    if r == R_BOOT:
        return T["boot"][1] if (ly % 4 == 1 and 5 <= lx <= 10) else T["boot"][0]
    if r == R_GEAR1:  # the backpack
        if lx in (6, 7, 24, 25):
            return T["strap"]
        if 18 <= ly <= 20:
            return T["pack"][1]
        return T["pack"][2] if leon_art.noise(lx, ly, 35) > 0.92 else T["pack"][0]
    if r == R_GEAR2:  # the collar and the bedroll
        return T["pack"][1] if ly % 4 == 0 else T["pack"][2]
    if r == R_GEAR3:  # cargo pockets
        return T["camo"][1] if ly > 24 or lx % 15 == 0 else T["camo"][0]
    if r == R_GEAR4:  # the bandolier: a strap with cartridges
        return T["metal"] if lx % 4 in (1, 2) and 8 <= ly <= 23 else T["strap"]
    if r == R_SKIN:
        return T["skin"][0]
    if r == R_DARK:
        return T["mask"][0]
    if r == R_SOLE:
        return T["sole"]
    return T["metal"]


def ring(c, r, w, **extra):
    entry = {"c": c, "r": r, "w": w}
    entry.update(extra)
    return entry


def mix(a, b, weight_b=0.5):
    return [(a, 1.0 - weight_b), (b, weight_b)]


def add_body_core(mesh):
    """The parts both teams share: torso, neck, head, arms, fists, legs and boots."""
    mesh.loft([
        ring((0.0, 0.0, 0.84), (0.110, 0.155), "pelvis"),
        ring((0.0, 0.0, 0.95), (0.125, 0.175), "pelvis"),
        ring((0.0, 0.0, 1.06), (0.120, 0.160), mix("pelvis", "spine_01")),
        ring((0.0, 0.0, 1.20), (0.125, 0.170), mix("spine_01", "spine_02")),
        ring((0.0, 0.0, 1.33), (0.135, 0.190), mix("spine_02", "spine_03", 0.6)),
        ring((0.0, 0.0, 1.44), (0.120, 0.200), "spine_03"),
        ring((0.0, 0.0, 1.52), (0.075, 0.120), "spine_03"),
    ], 8, R_TORSO)
    mesh.loft([
        ring((0.0, 0.0, 1.49), 0.055, mix("spine_03", "neck_01")),
        ring((0.01, 0.0, 1.63), 0.052, mix("neck_01", "head")),
    ], 6, R_SKIN, caps=(False, False))
    mesh.loft([
        ring((0.01, 0.0, 1.585), (0.070, 0.060), "head"),
        ring((0.01, 0.0, 1.630), (0.095, 0.080), "head"),
        ring((0.01, 0.0, 1.700), (0.105, 0.088), "head"),
        ring((0.00, 0.0, 1.770), (0.100, 0.087), "head"),
        ring((-0.005, 0.0, 1.820), (0.075, 0.068), "head"),
        ring((-0.01, 0.0, 1.845), (0.035, 0.032), "head"),
    ], 8, R_HEAD)
    mesh.box((0.112, 0.0, 1.705), (0.028, 0.024, 0.040), R_SKIN, "head", taper=(0.6, 0.8))  # the nose
    for side, s in (("r", 1.0), ("l", -1.0)):
        mesh.loft([
            ring((0.0, s * 0.12, 1.45), (0.065, 0.070), mix("clavicle_" + side, "upperarm_" + side)),
            ring((0.0, s * 0.20, 1.45), (0.062, 0.065), "upperarm_" + side),
            ring((0.0, s * 0.33, 1.45), (0.056, 0.058), "upperarm_" + side),
            ring((0.0, s * 0.44, 1.45), (0.050, 0.052), "upperarm_" + side),
            ring((0.0, s * 0.48, 1.45), (0.048, 0.050), mix("upperarm_" + side, "lowerarm_" + side)),
            ring((0.0, s * 0.52, 1.45), (0.047, 0.048), "lowerarm_" + side),
            ring((0.0, s * 0.64, 1.45), (0.043, 0.042), "lowerarm_" + side),
            ring((0.0, s * 0.705, 1.45), (0.040, 0.037), "lowerarm_" + side),
        ], 6, R_ARM)
        hand = "hand_" + side
        # A fist, the palm down in the T pose: the back of the hand, the curled fingers, the thumb.
        mesh.box((0.0, s * 0.752, 1.452), (0.085, 0.095, 0.040), R_HAND, hand)
        mesh.box((0.0, s * 0.815, 1.435), (0.080, 0.036, 0.060), R_HAND, hand)
        mesh.box((0.048, s * 0.765, 1.440), (0.026, 0.052, 0.026), R_HAND, hand)
        mesh.loft([
            ring((0.0, s * 0.10, 0.10), (0.048, 0.045), "calf_" + side),
            ring((0.0, s * 0.10, 0.30), (0.058, 0.052), "calf_" + side),
            ring((0.0, s * 0.10, 0.45), (0.056, 0.055), "calf_" + side),
            ring((0.0, s * 0.10, 0.50), (0.058, 0.058), mix("thigh_" + side, "calf_" + side)),
            ring((0.0, s * 0.10, 0.56), (0.065, 0.065), "thigh_" + side),
            ring((0.0, s * 0.10, 0.80), (0.082, 0.080), "thigh_" + side),
            ring((0.0, s * 0.10, 0.92), (0.085, 0.085), mix("thigh_" + side, "pelvis", 0.4)),
        ], 6, R_LEG)
        # The boot: the shaft over the ankle, the foot and the toe cap.
        mesh.loft([
            ring((0.0, s * 0.10, 0.04), (0.056, 0.050), "foot_" + side),
            ring((0.0, s * 0.10, 0.18), (0.054, 0.050), mix("calf_" + side, "foot_" + side)),
        ], 6, R_BOOT)
        mesh.box((0.03, s * 0.10, 0.045), (0.20, 0.100, 0.090), R_BOOT, "foot_" + side,
                 regions={"-z": R_SOLE})
        mesh.box((0.165, s * 0.10, 0.035), (0.090, 0.095, 0.070), R_BOOT, "ball_" + side, taper=(0.85, 0.7),
                 regions={"-z": R_SOLE})


def add_ct_gear(mesh):
    """The counter-terrorist: the vest, its pouches, the helmet, knee pads."""
    mesh.loft([
        ring((0.0, 0.0, 1.07), (0.140, 0.180), mix("pelvis", "spine_01", 0.7)),
        ring((0.005, 0.0, 1.22), (0.150, 0.186), mix("spine_01", "spine_02")),
        ring((0.01, 0.0, 1.36), (0.158, 0.200), mix("spine_02", "spine_03", 0.7)),
        ring((0.0, 0.0, 1.46), (0.132, 0.150), "spine_03"),
    ], 8, R_GEAR1, caps=(False, False))
    for y in (-0.09, 0.0, 0.09):  # magazine pouches across the front
        mesh.box((0.165, y, 1.16), (0.05, 0.075, 0.10), R_GEAR3, mix("spine_01", "spine_02"))
    mesh.box((-0.12, 0.0, 1.29), (0.08, 0.20, 0.20), R_GEAR3, mix("spine_02", "spine_03"))  # a back plate
    mesh.loft([
        ring((0.0, 0.0, 1.735), (0.122, 0.108), "head"),
        ring((-0.005, 0.0, 1.800), (0.118, 0.104), "head"),
        ring((-0.01, 0.0, 1.855), (0.085, 0.076), "head"),
        ring((-0.01, 0.0, 1.875), (0.035, 0.030), "head"),
    ], 8, R_GEAR2, caps=(True, True))
    for side, s in (("r", 1.0), ("l", -1.0)):
        mesh.box((0.062, s * 0.10, 0.50), (0.035, 0.085, 0.11), R_GEAR4, mix("thigh_" + side, "calf_" + side, 0.7))
        mesh.box((0.0, s * 0.20, 1.45), (0.14, 0.05, 0.14), R_GEAR1, "upperarm_" + side)  # shoulder pads


def add_t_gear(mesh):
    """The terrorist: the jacket's collar and hem, the backpack and its bedroll, cargo pockets, the bandolier."""
    mesh.loft([
        ring((0.0, 0.0, 1.50), (0.090, 0.130), "spine_03"),
        ring((-0.01, 0.0, 1.575), (0.085, 0.105), mix("spine_03", "neck_01", 0.3)),
    ], 8, R_GEAR2, caps=(False, False))
    mesh.loft([
        ring((0.0, 0.0, 0.86), (0.140, 0.185), mix("pelvis", "thigh_r", 0.0)),
        ring((0.0, 0.0, 0.95), (0.132, 0.180), "pelvis"),
    ], 8, R_TORSO, caps=(False, False))
    mesh.box((-0.19, 0.0, 1.24), (0.12, 0.26, 0.30), R_GEAR1, mix("spine_02", "spine_03", 0.6), taper=(0.9, 0.9))
    mesh.cylinder((-0.19, -0.16, 1.44), (-0.19, 0.16, 1.44), 0.055, 6, R_GEAR2, "spine_03",
                  reference=(1.0, 0.0, 0.0))
    for side, s in (("r", 1.0), ("l", -1.0)):
        mesh.box((0.0, s * 0.19, 0.70), (0.10, 0.04, 0.12), R_GEAR3, "thigh_" + side)
    # The bandolier across the chest, from the right shoulder to the left hip.
    mesh.box_between((0.14, 0.13, 1.44), (0.14, -0.14, 1.02), 0.075, 0.03, R_GEAR4,
                     mix("spine_02", "spine_03", 0.5), up=(1.0, 0.0, 0.0))


TEAMS = {
    "CT": (paint_ct, add_ct_gear),
    "T": (paint_t, add_t_gear),
}


def build_body(team, armature):
    painter, add_gear = TEAMS[team]
    name = "Body_" + team
    image = leon_art.paint_image(name + "_D", SIZE, SIZE, painter)
    assert leon_art.count_colours(image) <= 256, "a body's texture must stay P8"
    material = leon_art.make_material(name, roughness=0.8, image=image, surface="Flesh")
    mesh = leon_art.MeshBuilder(SIZE, SIZE)
    add_body_core(mesh)
    add_gear(mesh)
    body = mesh.build(name, material)
    count = leon_art.check_triangles(body, BUDGET)
    leon_art.skin_rigid(body, armature, mesh.vertex_bones())
    print("make_characters: %s has %d triangles" % (name, count))
    return body


def build():
    leon_art.reset_scene()
    armature = leon_art.build_armature("Body", leon_art.HUMANOID_BONES)
    sockets = [leon_art.add_socket(armature, name, bone, location, axes=axes)
               for name, bone, location, axes in leon_art.HUMANOID_SOCKETS]
    bodies = {team: build_body(team, armature) for team in TEAMS}
    anim_body.add_clips(armature)
    return armature, sockets, bodies


def export(armature, sockets, bodies, out):
    for team, body in bodies.items():
        leon_art.export_glb(os.path.join(out, "Body_%s.glb" % team), objects=[armature] + sockets + [body],
                            extras=True, animations=False)
    leon_art.export_glb(os.path.join(out, "Body_Animations.glb"), objects=[armature] + sockets, extras=True)
    leon_art.save_blend(os.path.join(out, "characters.blend"))


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    export(*build(), out)
    print("make_characters: wrote the characters in %s" % out)
