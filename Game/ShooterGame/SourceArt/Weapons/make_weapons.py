"""Builds ShooterGame's weapons with leon_art (Docs/ART_PIPELINE.md, ps2-shipping N27): for each weapon a world model
(what the other players see, on the floor and in the body's hand) and a first-person model (the view model in the
arms' hand), low-poly and textured in the style of Counter-Strike 1.6.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Weapons/make_weapons.py [-- --out <folder>]

Writes <Weapon>.glb (100-300 triangles, a 64 x 64 texture of at most 16 colours: P4) and <Weapon>_1P.glb (300-600
triangles, a 128 x 64 texture: P8) next to this script (or to <folder>), for the knife, the Glock,
the USP, the Desert Eagle, the AK-47, the M4A1, the MP5, the AWP, the HE, flash and smoke grenades and the C4.
ImportList.ini imports them as /Game/Weapons/SM_<Weapon> and SM_<Weapon>_1P.

Each weapon is built once from parts in engine centimetres: the hand's grip at the origin (where the fist's centre is,
the socket Weapon_R of the hands), the barrel along +X, +Z up, a `SOCKET_Muzzle` node at the muzzle (the static mesh
import makes it the mesh's Muzzle socket; the C4's origin is its bottom and its socket its top). A part is in both
models, or only in the first-person one (small details); cylinders have fewer sides in the world model. Every part
maps a tile of the weapon's texture, one tile a material, painted by code with a lighter border that reads as a bevel
on each face. The rifles hold their handguard's middle at (27, 0, 6.5) cm, where the left hand goes
(anim_body.RIFLE_SUPPORT, anim_arms). Only Blender's own modules are used; the art is this repository's
(Game/ShooterGame/SourceArt/LICENSES.md).
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, os.pardir))

import leon_art  # noqa: E402 (after the path)

CM = 0.01

# The materials: (base, dark, light) sRGB bytes and the pattern painted on their tile.
MATERIALS = {
    "steel": ((56, 58, 62), (38, 39, 42), (92, 94, 100), "brushed"),
    "black": ((44, 44, 48), (30, 30, 33), (76, 76, 82), "plain"),
    "wood": ((126, 66, 30), (92, 46, 20), (156, 88, 42), "grain"),
    "bakelite": ((96, 44, 24), (70, 30, 16), (124, 62, 34), "plain"),
    "olive": ((86, 98, 60), (64, 74, 44), (110, 122, 78), "plain"),
    "silver": ((150, 152, 156), (116, 118, 122), (190, 192, 196), "brushed"),
    "blade": ((164, 168, 174), (120, 124, 130), (214, 218, 224), "brushed"),
    "glass": ((40, 86, 104), (22, 50, 62), (120, 176, 196), "glass"),
    "grey": ((150, 152, 148), (112, 114, 110), (186, 188, 184), "plain"),
    "bluegrey": ((96, 108, 120), (70, 80, 90), (126, 138, 150), "plain"),
    "yellow": ((196, 164, 52), (150, 124, 36), (224, 200, 96), "plain"),
    "white": ((206, 206, 200), (168, 168, 162), (232, 232, 228), "plain"),
    "clay": ((176, 164, 120), (140, 128, 92), (200, 190, 150), "clay"),
    "display": ((54, 150, 64), (22, 70, 28), (150, 220, 150), "display"),
    "red": ((168, 40, 32), (110, 24, 20), (210, 80, 64), "plain"),
}

# Parts: ("box", centre, size, material, options) or ("cyl", p0, p1, radius, material, options) in centimetres.
# Options: lean (degrees the part's bottom goes back, about Y), taper ((y, z) scale of the +x end), r1 (a cone's end
# radius), fp (first person only), sides (a cylinder's sides in first person; the world model takes 6, or 5 under 1 cm).


def box(center, size, material, **options):
    return ("box", center, size, material, options)


def cyl(p0, p1, radius, material, **options):
    return ("cyl", p0, p1, radius, material, options)


def rifle_grip(material, lean=18.0):
    return [box((-1.5, 0.0, -0.5), (3.8, 3.0, 9.5), material, lean=lean),
            box((3.2, 0.0, 2.2), (6.0, 0.8, 0.8), "steel", fp=True),  # the trigger guard
            box((2.6, 0.0, 2.9), (0.6, 0.4, 1.8), "steel", fp=True, lean=-10.0)]  # the trigger


WEAPONS = {
    "AK47": {
        "materials": ["steel", "wood", "bakelite", "black"],
        "muzzle": (65.0, 0.0, 7.8),
        "parts": rifle_grip("bakelite") + [
            box((8.0, 0.0, 6.2), (30.0, 3.6, 5.4), "steel"),  # the receiver
            box((8.5, 0.0, 9.2), (25.0, 3.2, 1.0), "steel", fp=True, taper=(0.9, 0.7)),  # the dust cover
            box((-20.5, 0.0, 5.0), (26.0, 3.4, 7.8), "wood", taper=(0.95, 0.62)),  # the stock
            box((-34.0, 0.0, 3.8), (1.2, 3.6, 8.2), "black", fp=True),  # the butt plate
            box((11.5, 0.0, 0.3), (6.0, 2.6, 8.0), "steel", lean=-12.0),  # the magazine
            box((14.8, 0.0, -6.8), (5.4, 2.4, 7.0), "steel", lean=-28.0),
            box((27.0, 0.0, 6.3), (16.0, 4.0, 4.2), "wood", taper=(0.9, 0.9)),  # the handguard
            box((27.0, 0.0, 9.3), (14.0, 3.0, 1.8), "wood", fp=True),  # the gas tube's cover
            cyl((23.0, 0.0, 7.8), (61.0, 0.0, 7.8), 0.9, "steel", sides=8),  # the barrel
            box((37.0, 0.0, 9.0), (3.0, 2.2, 3.4), "steel"),  # the gas block
            box((58.0, 0.0, 10.0), (1.6, 1.2, 4.0), "steel", lean=-6.0),  # the front sight
            cyl((61.0, 0.0, 7.8), (65.0, 0.0, 7.8), 1.25, "black", sides=8),  # the muzzle brake
            box((20.0, 0.0, 9.6), (3.0, 2.0, 1.2), "steel", fp=True),  # the rear sight
            cyl((35.0, 0.0, 6.0), (57.0, 0.0, 6.0), 0.3, "steel", fp=True, sides=4),  # the cleaning rod
            box((14.0, 2.2, 7.6), (2.0, 1.2, 0.9), "steel", fp=True),  # the charging handle
        ],
    },
    "M4A1": {
        "materials": ["black", "steel", "bluegrey"],
        "muzzle": (64.0, 0.0, 8.8),
        "parts": rifle_grip("black", 22.0) + [
            box((6.0, 0.0, 5.6), (20.0, 3.2, 4.4), "black"),  # the lower receiver
            box((9.0, 0.0, 9.0), (26.0, 3.0, 3.4), "black"),  # the upper receiver
            box((9.0, 0.0, 11.6), (18.0, 1.8, 1.8), "bluegrey"),  # the carrying handle's rail
            box((9.0, 0.0, 10.7), (4.0, 1.6, 1.0), "black", fp=True),
            cyl((-4.0, 0.0, 8.6), (-21.0, 0.0, 8.6), 1.4, "black", sides=8),  # the buffer tube
            box((-26.0, 0.0, 6.4), (12.0, 3.4, 8.6), "black", taper=(0.95, 0.75)),  # the stock
            box((9.5, 0.0, -1.0), (5.6, 2.4, 11.0), "steel", lean=-8.0),  # the magazine
            cyl((22.0, 0.0, 8.0), (40.0, 0.0, 8.0), 2.3, "bluegrey", sides=8),  # the handguard
            cyl((40.0, 0.0, 8.8), (60.0, 0.0, 8.8), 0.8, "steel", sides=8),  # the barrel
            box((42.0, 0.0, 11.4), (2.0, 1.6, 5.0), "black"),  # the front sight's tower
            cyl((60.0, 0.0, 8.8), (64.0, 0.0, 8.8), 1.15, "black", sides=8),  # the flash hider
            box((14.0, 2.0, 7.2), (2.4, 0.8, 1.6), "steel", fp=True),  # the ejection port's cover
            box((0.0, 0.0, 11.0), (2.0, 1.4, 0.8), "black", fp=True),  # the charging handle
        ],
    },
    "MP5": {
        "materials": ["black", "steel"],
        "muzzle": (41.0, 0.0, 7.6),
        "parts": rifle_grip("black", 15.0) + [
            box((9.0, 0.0, 7.2), (26.0, 3.4, 5.2), "black"),  # the receiver
            cyl((-2.0, 0.0, 10.4), (32.0, 0.0, 10.4), 1.2, "black", sides=8),  # the cocking tube
            box((26.0, 0.0, 6.3), (12.0, 4.2, 4.6), "black", taper=(0.85, 0.9)),  # the handguard
            cyl((32.0, 0.0, 7.6), (41.0, 0.0, 7.6), 1.0, "steel", sides=8),  # the barrel
            box((33.0, 0.0, 11.4), (2.0, 2.6, 3.0), "black"),  # the front sight's ring
            box((10.5, 0.0, -1.0), (3.6, 2.4, 10.0), "steel", lean=-14.0),  # the magazine
            box((-14.0, 0.0, 7.6), (24.0, 1.4, 3.0), "steel"),  # the retracted stock's arms
            box((-26.5, 0.0, 6.2), (1.8, 3.6, 8.0), "black"),  # the butt pad
            box((2.0, 0.0, 12.2), (3.0, 2.2, 1.6), "black", fp=True),  # the rear sight's drum
            box((24.0, 2.0, 10.4), (2.0, 1.2, 1.0), "steel", fp=True),  # the cocking handle
        ],
    },
    "AWP": {
        "materials": ["olive", "steel", "black", "glass"],
        "muzzle": (81.0, 0.0, 7.6),
        "parts": [
            box((-2.0, 0.0, 0.0), (4.6, 3.4, 9.5), "olive", lean=15.0),  # the grip
            box((3.2, 0.0, 2.4), (6.0, 0.8, 0.8), "steel", fp=True),
            box((12.0, 0.0, 6.3), (44.0, 4.4, 5.4), "olive", taper=(0.9, 0.85)),  # the stock's body
            box((-24.0, 0.0, 4.2), (20.0, 4.0, 9.4), "olive", taper=(0.95, 0.7)),  # the butt
            box((-20.0, 0.0, 9.6), (14.0, 3.4, 2.0), "olive"),  # the cheek rest
            box((-34.6, 0.0, 3.6), (1.4, 4.2, 9.8), "black"),  # the butt pad
            cyl((30.0, 0.0, 7.6), (76.0, 0.0, 7.6), 1.1, "steel", sides=8),  # the barrel
            box((78.5, 0.0, 7.6), (5.0, 2.8, 2.8), "steel"),  # the muzzle brake
            cyl((0.0, 0.0, 13.2), (24.0, 0.0, 13.2), 1.9, "black", sides=8),  # the scope's tube
            cyl((24.0, 0.0, 13.2), (32.0, 0.0, 13.2), 2.0, "black", sides=8, r1=2.8),  # the objective bell
            cyl((-7.0, 0.0, 13.2), (0.0, 0.0, 13.2), 2.3, "black", sides=8, r1=1.9),  # the eyepiece
            cyl((32.0, 0.0, 13.2), (32.4, 0.0, 13.2), 2.5, "glass", sides=8, fp=True),  # the lens
            box((5.0, 0.0, 10.4), (2.0, 2.2, 1.8), "steel"),  # the mounts
            box((19.0, 0.0, 10.4), (2.0, 2.2, 1.8), "steel"),
            box((12.0, 0.0, 16.0), (2.4, 2.4, 1.8), "black", fp=True),  # the turrets
            box((12.0, 2.6, 13.2), (2.4, 1.8, 2.4), "black", fp=True),
            cyl((-3.0, 2.2, 8.4), (-3.5, 5.2, 7.0), 0.5, "steel", fp=True, sides=5),  # the bolt handle
            box((-3.6, 5.6, 6.8), (1.4, 1.4, 1.4), "steel", fp=True),
            box((10.0, 0.0, 1.6), (7.0, 3.0, 4.4), "steel"),  # the magazine
        ],
    },
    "USP": {
        "materials": ["black", "steel"],
        "muzzle": (16.6, 0.0, 7.4),
        "parts": [
            box((-1.0, 0.0, -0.6), (3.6, 2.8, 10.0), "black", lean=18.0),  # the grip
            box((6.0, 0.0, 7.6), (19.0, 2.8, 3.4), "black", taper=(0.95, 0.9)),  # the slide
            box((5.5, 0.0, 4.6), (14.0, 2.6, 2.6), "black"),  # the frame
            cyl((15.5, 0.0, 7.4), (16.6, 0.0, 7.4), 0.6, "steel", sides=6),  # the barrel's tip
            box((3.4, 0.0, 2.6), (5.0, 0.7, 0.7), "black"),  # the trigger guard
            box((3.0, 0.0, 3.2), (0.6, 0.4, 1.6), "steel", fp=True, lean=-10.0),  # the trigger
            box((-2.4, 0.0, 9.6), (1.0, 2.0, 0.8), "black"),  # the sights
            box((14.0, 0.0, 9.5), (0.8, 0.6, 0.8), "black"),
            box((-4.2, 0.0, 6.4), (1.2, 1.0, 1.8), "steel", fp=True),  # the hammer
            box((-2.8, 0.0, -5.8), (4.0, 3.0, 1.0), "black", lean=18.0),  # the magazine's base
            box((2.0, 1.5, 7.9), (4.0, 0.3, 1.2), "steel", fp=True),  # the ejection port
            box((9.0, 0.0, 3.1), (6.0, 2.2, 0.6), "black", fp=True),  # the accessory rail
            box((-1.5, 1.35, 5.4), (2.2, 0.3, 0.6), "steel", fp=True),  # the safety lever
        ],
    },
    "Glock": {
        "materials": ["black", "steel"],
        "muzzle": (16.0, 0.0, 7.2),
        "parts": [
            box((-1.2, 0.0, -0.6), (3.8, 2.8, 10.0), "black", lean=22.0),  # the grip
            box((5.8, 0.0, 7.3), (18.4, 2.6, 3.0), "black"),  # the slide
            box((5.0, 0.0, 4.5), (13.0, 2.6, 2.6), "black", taper=(0.95, 0.85)),  # the frame
            cyl((15.2, 0.0, 7.2), (16.0, 0.0, 7.2), 0.55, "steel", sides=6),  # the barrel's tip
            box((3.2, 0.0, 2.6), (5.0, 0.7, 0.7), "black"),  # the trigger guard
            box((2.8, 0.0, 3.2), (0.6, 0.4, 1.6), "black", fp=True, lean=-10.0),  # the trigger
            box((-2.6, 0.0, 9.1), (1.0, 2.0, 0.8), "steel"),  # the sights
            box((13.8, 0.0, 9.0), (0.8, 0.6, 0.8), "steel"),
            box((1.5, 1.4, 7.5), (4.0, 0.3, 1.2), "steel", fp=True),  # the ejection port
            box((-3.0, 0.0, -5.7), (4.0, 3.0, 1.0), "black", lean=22.0),  # the magazine's base
            box((9.0, 0.0, 3.0), (5.0, 2.2, 0.6), "black", fp=True),  # the rail
            box((-2.0, 0.0, 7.3), (0.4, 2.8, 2.6), "steel", fp=True),  # the slide's serrations
            box((-1.2, 0.0, 7.3), (0.4, 2.8, 2.6), "steel", fp=True),
            box((-0.4, 0.0, 7.3), (0.4, 2.8, 2.6), "steel", fp=True),
        ],
    },
    "Deagle": {
        "materials": ["silver", "black", "steel"],
        "muzzle": (22.6, 0.0, 8.6),
        "parts": [
            box((-1.6, 0.0, -0.6), (4.2, 3.0, 10.6), "black", lean=15.0),  # the grip
            box((7.0, 0.0, 8.2), (22.0, 3.2, 4.2), "silver", taper=(0.9, 0.85)),  # the slide
            box((6.0, 0.0, 4.7), (16.0, 2.8, 3.0), "silver"),  # the frame
            box((20.0, 0.0, 8.6), (5.0, 2.6, 2.6), "silver", taper=(0.8, 0.8)),  # the barrel's end
            cyl((22.4, 0.0, 8.6), (22.6, 0.0, 8.6), 0.7, "black", sides=6, fp=True),  # the bore
            box((3.6, 0.0, 2.6), (5.4, 0.8, 0.8), "silver"),  # the trigger guard
            box((3.2, 0.0, 3.3), (0.6, 0.4, 1.6), "steel", fp=True, lean=-10.0),  # the trigger
            box((-3.2, 0.0, 10.6), (1.2, 2.2, 0.8), "black"),  # the sights
            box((19.0, 0.0, 10.6), (1.0, 0.8, 0.8), "black"),
            box((-4.8, 0.0, 7.2), (1.4, 1.2, 2.0), "steel", fp=True),  # the hammer
            box((4.0, 1.7, 8.8), (5.0, 0.3, 1.6), "steel", fp=True),  # the ejection port
            box((-3.2, 0.0, -6.2), (4.4, 3.2, 1.0), "black", lean=15.0),  # the magazine's base
        ],
    },
    "Knife": {
        "materials": ["blade", "black", "steel"],
        "muzzle": (27.0, 0.0, 0.8),
        "parts": [
            box((-1.5, 0.0, 0.0), (11.0, 2.2, 2.9), "black", taper=(0.9, 0.9)),  # the handle
            box((-7.6, 0.0, 0.0), (1.4, 2.5, 3.2), "steel"),  # the pommel
            box((4.6, 0.0, 0.0), (1.0, 2.8, 4.4), "steel"),  # the guard
            box((12.0, 0.0, 0.4), (14.0, 0.5, 3.4), "blade", taper=(1.0, 0.85)),  # the blade
            box((22.5, 0.0, 0.8), (7.0, 0.5, 2.9), "blade", taper=(0.5, 0.15)),  # the point
            box((13.0, 0.0, 2.0), (16.0, 0.7, 0.5), "steel", taper=(0.6, 0.6)),  # the spine
            box((-1.5, 1.15, 0.0), (8.0, 0.2, 1.6), "steel", fp=True),  # the grip's inlays
            box((-1.5, -1.15, 0.0), (8.0, 0.2, 1.6), "steel", fp=True),
            box((-4.0, 0.0, 0.0), (0.5, 2.4, 3.1), "steel", fp=True),  # the handle's rings
            box((0.5, 0.0, 0.0), (0.5, 2.4, 3.1), "steel", fp=True),
            box((6.0, 0.0, 0.2), (1.6, 0.6, 2.6), "steel", fp=True),  # the ricasso
        ],
    },
    "HEGrenade": {
        "materials": ["olive", "steel", "black"],
        "muzzle": (0.0, 0.0, 6.0),
        "parts": [
            ("loft", [(-3.9, 1.2), (-3.2, 2.5), (-1.6, 3.3), (0.0, 3.5), (1.6, 3.3), (3.0, 2.6), (3.9, 1.7)], "olive",
             {"sides": 10}),
            cyl((0.0, 0.0, 3.6), (0.0, 0.0, 5.4), 1.1, "steel", sides=10),  # the fuse
            cyl((0.0, 0.0, 5.4), (0.0, 0.0, 6.0), 1.1, "black", sides=10, r1=0.8, fp=True),
            box((1.9, 0.0, 2.6), (0.6, 1.4, 6.4), "steel", lean=-14.0),  # the lever
            box((1.2, 0.0, 5.4), (1.4, 1.6, 0.6), "steel"),
            box((-1.3, 0.0, 5.8), (1.8, 0.3, 0.3), "steel", fp=True),  # the pin's ring
            box((-1.3, 0.0, 4.2), (1.8, 0.3, 0.3), "steel", fp=True),
            box((-2.2, 0.0, 5.0), (0.3, 0.3, 1.8), "steel", fp=True),
            box((-0.4, 0.0, 5.0), (0.3, 0.3, 1.8), "steel", fp=True),
        ],
    },
    "Flashbang": {
        "materials": ["grey", "steel", "white"],
        "muzzle": (0.0, 0.0, 7.4),
        "parts": [
            ("loft", [(-5.0, 1.80), (-4.5, 2.40), (-0.2, 2.40), (4.1, 2.40), (4.6, 1.92)], "grey", {"sides": 10}),
            cyl((0.0, 0.0, -1.2), (0.0, 0.0, 1.2), 2.50, "white", sides=10, fp=True),  # the band
            cyl((0.0, 0.0, 4.6), (0.0, 0.0, 6.6), 1.0, "steel", sides=10),  # the fuse
            box((1.8, 0.0, 3.6), (0.6, 1.4, 6.6), "steel", lean=-10.0),  # the lever
            box((1.2, 0.0, 6.2), (1.4, 1.6, 0.6), "steel"),
            box((-1.3, 0.0, 6.8), (1.8, 0.3, 0.3), "steel", fp=True),  # the pin's ring
            box((-1.3, 0.0, 5.2), (1.8, 0.3, 0.3), "steel", fp=True),
            box((-2.2, 0.0, 6.0), (0.3, 0.3, 1.8), "steel", fp=True),
            box((-0.4, 0.0, 6.0), (0.3, 0.3, 1.8), "steel", fp=True),
        ],
    },
    "SmokeGrenade": {
        "materials": ["bluegrey", "steel", "yellow"],
        "muzzle": (0.0, 0.0, 7.6),
        "parts": [
            ("loft", [(-5.4, 2.03), (-4.9, 2.70), (-0.3, 2.70), (4.3, 2.70), (4.8, 2.16)], "bluegrey", {"sides": 10}),
            cyl((0.0, 0.0, 2.6), (0.0, 0.0, 3.6), 2.80, "yellow", sides=10, fp=True),  # the band
            cyl((0.0, 0.0, 4.8), (0.0, 0.0, 6.8), 1.0, "steel", sides=10),  # the fuse
            box((2.1, 0.0, 3.8), (0.6, 1.4, 6.6), "steel", lean=-10.0),  # the lever
            box((1.2, 0.0, 6.4), (1.4, 1.6, 0.6), "steel"),
            box((-1.3, 0.0, 7.0), (1.8, 0.3, 0.3), "steel", fp=True),  # the pin's ring
            box((-1.3, 0.0, 5.4), (1.8, 0.3, 0.3), "steel", fp=True),
            box((-2.2, 0.0, 6.2), (0.3, 0.3, 1.8), "steel", fp=True),
            box((-0.4, 0.0, 6.2), (0.3, 0.3, 1.8), "steel", fp=True),
        ],
    },
    "C4": {
        "materials": ["clay", "black", "display", "steel", "red"],
        "muzzle": (0.0, 0.0, 8.5),
        "parts": [
            box((0.0, -5.0, 3.0), (24.0, 5.0, 6.0), "clay"),  # the three charges
            box((0.0, 0.0, 3.0), (24.0, 5.0, 6.0), "clay"),
            box((0.0, 5.0, 3.0), (24.0, 5.0, 6.0), "clay"),
            box((-7.0, 0.0, 3.0), (2.0, 15.6, 6.4), "steel"),  # the tape
            box((7.0, 0.0, 3.0), (2.0, 15.6, 6.4), "steel"),
            box((-2.0, 0.0, 7.0), (12.0, 9.0, 2.0), "black"),  # the keypad
            box((-4.0, 0.0, 8.1), (5.0, 4.0, 0.3), "display"),  # the display
            cyl((4.0, -3.0, 8.0), (10.0, -6.5, 6.2), 0.35, "red", fp=True, sides=4),  # the wires
            cyl((4.0, 3.0, 8.0), (10.0, 6.5, 6.2), 0.35, "black", fp=True, sides=4),
            box((1.0, -2.4, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((1.0, -0.8, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((1.0, 0.8, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((1.0, 2.4, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((2.6, -2.4, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((2.6, -0.8, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((2.6, 0.8, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((2.6, 2.4, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((4.2, -2.4, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((4.2, -0.8, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((4.2, 0.8, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
            box((4.2, 2.4, 8.2), (1.2, 1.2, 0.5), "steel", fp=True),
        ],
    },
}

# The budgets (Docs/ART_PIPELINE.md).
WORLD_BUDGET = 300
VIEW_BUDGET = 600


def lean_axes(degrees):
    a = math.radians(degrees)
    return ((math.cos(a), 0.0, -math.sin(a)), (0.0, 1.0, 0.0), (math.sin(a), 0.0, math.cos(a)))


def chamfered(width, height, first_person):
    """A box's cross-section with its edges cut: all four in first person (8 sides), the top two in the world (6)."""
    c = 0.22 * min(width, height)
    w, h = width * 0.5, height * 0.5
    if first_person:
        return [(w - c, -h), (w, -h + c), (w, h - c), (w - c, h), (-w + c, h), (-w, h - c), (-w, -h + c), (-w + c, -h)]
    return [(w, -h), (w, h - c), (w - c, h), (-w + c, h), (-w, h - c), (-w, -h)]


def tile_region(index, tile_w, tile_h, columns):
    x = (index % columns) * tile_w
    y = (index // columns) * tile_h
    return (x, y, x + tile_w, y + tile_h)


def paint_tile(material, lx, ly, size, seed):
    """A material's tile: a border lighter on top and left, darker on bottom and right (a bevel on every face that
    maps it), and its pattern inside."""
    base, dark, light, pattern = MATERIALS[material]
    edge = 2 if size >= 32 else 1
    if lx < edge or ly >= size - edge:
        return light
    if lx >= size - edge or ly < edge:
        return dark
    n = leon_art.noise(lx, ly, seed)
    if pattern == "grain":
        return dark if (ly * 7 + int(3.0 * math.sin(lx * 0.4))) % 9 == 0 else (light if n > 0.94 else base)
    if pattern == "brushed":
        return light if leon_art.noise(0, ly, seed) > 0.85 else base
    if pattern == "glass":
        return light if lx < size // 3 and ly > size // 2 else base
    if pattern == "clay":
        return dark if n > 0.9 else base
    if pattern == "display":
        return light if (size // 4 <= ly <= size // 2 and lx % 4 != 3) else base
    return dark if n > 0.96 else base


def build_weapon(name, spec, first_person):
    suffix = "_1P" if first_person else ""
    width, height = (128, 64) if first_person else (64, 64)
    columns = 4
    tile = 32 if first_person else 16
    materials = spec["materials"]
    tiles = {material: tile_region(i, tile, tile, columns) for i, material in enumerate(materials)}
    seed = sum(ord(c) for c in name)

    def painter(x, y):
        index = (y // tile) * columns + (x // tile)
        material = materials[index] if index < len(materials) else materials[0]
        return paint_tile(material, x % tile, y % tile, tile, seed + index)

    image = leon_art.paint_image(name + suffix + "_D", width, height, painter)
    limit = 256 if first_person else 16
    colours = leon_art.count_colours(image)
    assert colours <= limit, "%s%s's texture has %d colours, over %d" % (name, suffix, colours, limit)
    # Steel and polymer are metal to a bullet (CS); the C4 is a computer's circuits.
    surface = "Computer" if name == "C4" else "Metal"
    material = leon_art.make_material(name + suffix, roughness=0.5, image=image, surface=surface)
    mesh = leon_art.MeshBuilder(width, height)
    for part in spec["parts"]:
        kind = part[0]
        if kind == "loft":
            _kind, rings, material_name, options = part
            sides = options["sides"] if first_person else 6
            mesh.loft([{"c": (0.0, 0.0, z * CM), "r": r * CM} for z, r in rings], sides, tiles[material_name],
                      reference=(1.0, 0.0, 0.0))
            continue
        options = part[-1]
        if options.get("fp") and not first_person:
            continue
        if kind == "box":
            _kind, center, size, material_name, _options = part
            ax, _ay, az = lean_axes(options.get("lean", 0.0))
            c = [v * CM for v in center]
            half = size[0] * CM * 0.5
            start = [c[i] - ax[i] * half for i in range(3)]
            end = [c[i] + ax[i] * half for i in range(3)]
            mesh.prism(start, end, chamfered(size[1] * CM, size[2] * CM, first_person), tiles[material_name],
                       up=az, taper=options.get("taper", (1.0, 1.0)))
        else:
            _kind, p0, p1, radius, material_name, _options = part
            sides = options.get("sides", 8) if first_person else (6 if radius >= 1.0 else 5)
            sides = min(sides, options.get("sides", sides))
            r1 = options.get("r1")
            mesh.cylinder([c * CM for c in p0], [c * CM for c in p1], radius * CM, sides, tiles[material_name],
                          radius1=None if r1 is None else r1 * CM)
    obj = mesh.build(name + suffix, material)
    budget = VIEW_BUDGET if first_person else WORLD_BUDGET
    count = leon_art.check_triangles(obj, budget)
    muzzle = leon_art.add_object("SOCKET_Muzzle", None)
    muzzle.empty_display_type = "ARROWS"
    muzzle.empty_display_size = 0.03
    muzzle.parent = obj
    muzzle.location = leon_art.to_blender(*[c * CM for c in spec["muzzle"]])
    print("make_weapons: %s%s has %d triangles" % (name, suffix, count))
    return obj, muzzle, count


def models():
    """Every model's name, weapon and view: (name, spec, first_person), in order."""
    return [(name + ("_1P" if first_person else ""), name, first_person)
            for name in sorted(WEAPONS) for first_person in (False, True)]


def build(model=None):
    """Builds one model in an empty scene (the first when none is named): each file has the scene to itself, so its
    socket keeps the name SOCKET_Muzzle."""
    name, weapon, first_person = next(entry for entry in models() if model in (None, entry[0]))
    leon_art.reset_scene()
    return build_weapon(weapon, WEAPONS[weapon], first_person)


def export(out):
    for name, _weapon, _first_person in models():
        obj, muzzle, _count = build(name)
        leon_art.export_glb(os.path.join(out, name + ".glb"), objects=[obj, muzzle], extras=True, animations=False)


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    export(out)
    print("make_weapons: wrote the weapons in %s" % out)
