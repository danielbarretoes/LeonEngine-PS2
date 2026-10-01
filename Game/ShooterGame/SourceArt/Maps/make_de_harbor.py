"""Builds de_harbor, ShooterGame's second bomb defusal map, in Blender and exports it for LeonEd's map importer.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Maps/make_de_harbor.py [-- --out <folder>]

Writes de_harbor.blend and de_harbor.glb next to this script (the script is the source, plan decision D6); then import
it with the rest of the source art (Game/ShooterGame/SourceArt/ImportList.ini):

    LeonCook Game/ShooterGame/ShooterGame.lproj -run=ImportAssets -importlist=Game/ShooterGame/SourceArt/ImportList.ini

An industrial port at the end of the afternoon (Docs/LEVELS.md, "de_harbor", has the plan of it): an asphalt truck
yard for the terrorists to the south, a quay along the water to the east with a ship moored off it, container stacks,
a customs shed and two blocks between the lanes, a gantry crane over bomb site A on the quay apron, bomb site B inside
a roofed warehouse, and the counter-terrorists' yard to the north. Three routes: the quay (A long), mid (into the
courtyard before the CT spawn, its flank: the gate at mid's end and the fence's opening hide the CT spawn from mid)
and the alley to the warehouse's front door (B short), with a roofed connector from mid to the alley. A is entered from
the quay through a chicane of container stacks and from the CT spawn, B through a vestibule behind its front door and
by the CT spawn's door. Its own palette and materials (painted and rusty corrugated metal, shipping
containers, concrete, asphalt, pine pallets, water), painted texel by texel here like de_leon's, so nothing is shared
with de_leon but the conventions.

The map follows make_de_leon.py's conventions (the same node names, the same axes, the same faces cut on a 3 m grid
for the baked lighting, the same checks); the geometry helpers below are that script's, with this map's textures.
The layout is in the engine's axes, metres (X north, Y east, Z up), placed in Blender's (X, -Y, Z):

    <Cell>_<Piece>            a static mesh actor with its UCX_ box; the containers, pallets, bollards and lamps share
                              their meshes (SM_Container_<Colour>_<Axis>, SM_Pallet, SM_PalletBig, SM_Bollard, SM_Lamp)
    Outside_<Piece>           the water, the quay wall, the ship and the crane's top: no cell (always drawn)
    PlayerStart_CT ... _T     the teams' starts;  BombSite_A / _B, BuyZone_CT / _T: ShooterGame's trigger volumes
    Ladder_A                  the ladder up the container stack at A (N30c); Clip_Quay keeps everyone out of the water
    NavWaypoint_*             the waypoint graph ("links", "flags": Lookout, Ladder)
    Sun, Light_<Place>_NN     the late sun and the lamps;  WorldSettings: the coastal sky, its fog and ambient
    VIS_<Cell>, PORTAL_<A>_<B>  the cells and portals (N15)

Every cell stays within ART_PIPELINE.md's 1 500 triangles (the script fails otherwise and prints the counts). Only
Blender's own modules, through the shared leon_art; no external art (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir))

import leon_art  # noqa: E402 (after the path)

HERE = os.path.dirname(os.path.abspath(__file__))

# Heights (metres): the floor's top at Z = 0. Pallet crates 1.1 m (a jump clears them) and 1.6 m; a shipping container
# (20 ft) 6 x 2.44 x 2.6 m, stacked two high (5.2 m) as walls; the player start's capsule centre 0.92 m up.
CRATE = 1.1
CRATE_BIG = 1.6
CONTAINER = (6.0, 2.44, 2.6)
START_HEIGHT = 0.92
# The bake's vertices (make_de_leon.py): a 3 m world grid, the walls cut again FOOT above the floor.
STEP = 3.0
FOOT = 1.2
MIN_SEGMENT = 0.5
CELL_BUDGET = 1500
# The top of the cells' volumes: above the eyes on the container stack at A (5.2 + 1.7 m). What is wholly above it
# (the crane's beams and boom) is in no cell, so it is always drawn.
CELL_TOP = 7.5
# The tops of the blocks between the lanes and of the container stacks: the sky's portals start there.
SKY = 5.5
STACK = 5.2
AGENT_CLEARANCE = 0.45
# Where the sun's light travels (engine axes, not normalized): a late sun, lower than de_leon's, from the south-west
# over the warehouses, so the long shadows fall toward the water. Sky/make_sky.py reads this line for the coast sky.
SUN_DIRECTION = (0.30, 0.70, -0.62)
# The water's surface below the quay's top.
WATER_Z = -1.5
# Mid's gate into the courtyard: Y -GATE .. GATE in a wall GATE_HEIGHT high.
GATE = 1.75
GATE_HEIGHT = 4.0
# The courtyard's opening into the CT spawn: Y -FENCE .. FENCE in a fence as high as mid's gate.
FENCE = 3.0

CELLS = ["TSpawn", "LongA", "Mid", "Connector", "Alley", "Courtyard", "CTSpawn", "SiteA", "SiteB"]
# The pieces in no cell (the water, the quay's face, the ship, the crane's top), counted on their own.
OUTSIDE = "Outside"

to_blender = leon_art.to_blender


# The textures, painted texel by texel (y = 0 is the bottom row, as leon_art.paint_image paints).

def value_noise(x, y, cell, size, seed):
    """Smooth noise in [0, 1) over cells of `cell` texels that tiles every `size` texels (a texture repeats)."""
    n = size // cell
    cx, cy = x // cell, y // cell
    fx, fy = (x % cell) / cell, (y % cell) / cell
    fx, fy = fx * fx * (3.0 - 2.0 * fx), fy * fy * (3.0 - 2.0 * fy)

    def corner(i, j):
        return leon_art.noise(i % n, j % n, seed)

    top = corner(cx, cy) * (1.0 - fx) + corner(cx + 1, cy) * fx
    bottom = corner(cx, cy + 1) * (1.0 - fx) + corner(cx + 1, cy + 1) * fx
    return top * (1.0 - fy) + bottom * fy


def shade(rgb, k, step=0.04):
    """A colour scaled by k, k rounded to `step` so a texture keeps to a small palette."""
    k = round(k / step) * step
    return tuple(max(0, min(255, int(round(c * k)))) for c in rgb)


def paint_asphalt(x, y):
    """128 x 128 (2 m): dark asphalt, light and dark grains of its aggregate, worn and oily patches."""
    patch = value_noise(x, y, 32, 128, 41)
    tint = (78, 78, 80) if patch < 0.66 else (70, 70, 73)
    grain = leon_art.noise(x, y, 43)
    if grain > 0.965:
        return shade(tint, 1.36)
    if grain < 0.03:
        return shade(tint, 0.76)
    k = 1.0 + 0.10 * (value_noise(x, y, 8, 128, 44) - 0.5) + 0.06 * (leon_art.noise(x, y, 45) - 0.5)
    return shade(tint, max(0.88, min(1.12, k)))


def paint_concrete(x, y):
    """128 x 128 (2 m): a cast concrete slab of the quay, its joints at the edges, mottling and rust stains."""
    if x < 2 or y < 2:
        return shade((112, 110, 104), 1.0 if leon_art.noise(x, y, 51) > 0.3 else 0.92)
    tint = (158, 156, 148)
    if value_noise(x, y, 16, 128, 52) > 0.78:
        tint = (150, 138, 120)
    k = 1.0 + 0.12 * (value_noise(x, y, 8, 128, 53) - 0.5) + 0.05 * (leon_art.noise(x, y, 54) - 0.5)
    if x == 2 or y == 2:
        k += 0.06
    if leon_art.noise(x, y, 55) > 0.985:
        k -= 0.16
    return shade(tint, max(0.88, min(1.08, k)))


def paint_panel(x, y):
    """128 x 128 (2 m): the boundary wall's precast panels, 2 m wide with a deep joint, tie holes on a grid, grime
    running down from them."""
    if x < 3:
        return (88, 86, 82)
    if x == 3:
        return (196, 192, 182)
    if (x - 3) % 31 in (13, 14) and y % 42 in (20, 21):
        return (74, 72, 70)
    tint = (176, 172, 162)
    k = 1.0 + 0.10 * (value_noise(x, y, 16, 128, 61) - 0.5) + 0.04 * (leon_art.noise(x, y, 62) - 0.5)
    # Grime: a streak below each tie hole.
    if (x - 3) % 31 in (12, 13, 14, 15) and y % 42 < 20 and leon_art.noise(x // 2, y // 6, 63) > 0.4:
        k -= 0.1
    if y < 10:
        k -= 0.06
    return shade(tint, max(0.8, min(1.1, k)))


def paint_corrugated(x, y):
    """128 x 128 (2 m): painted corrugated sheet (the sheds and the warehouse): ribs every 25 cm lit on one side,
    faded blue-grey paint, thin rust streaks running down the ribs' shadowed side from the sheet's lap, grime at the
    foot."""
    rib = x % 16
    column = x // 16
    light = (1.08, 1.12, 1.08, 1.0, 0.94, 0.86, 0.82, 0.86, 0.94, 1.0, 1.04, 1.04, 1.02, 1.0, 1.0, 1.04)[rib]
    colour = (98, 120, 140) if value_noise(x, y, 32, 128, 72) < 0.7 else (110, 128, 142)
    run = 10 + int(36 * leon_art.noise(column, 1, 71))
    if leon_art.noise(column, 0, 71) > 0.55 and rib in (5, 6) and y >= 127 - run:
        colour = (122, 96, 78) if leon_art.noise(x, y, 73) > 0.3 else (108, 98, 94)
    if y >= 125:
        colour = (84, 96, 108)
    if y < 6:
        colour = (90, 96, 98)
    k = light * (1.0 + 0.06 * (leon_art.noise(x, y, 74) - 0.5))
    return shade(colour, k, 0.02)


def paint_girder(x, y):
    """64 x 64 (1 m): dark painted steel (the lintels, the roofs' edges, the ladder): scratches, rust spots, a seam."""
    tint = (72, 76, 82)
    if y < 2 or x % 32 < 1:
        return shade(tint, 0.78)
    if leon_art.noise(x, y, 81) > 0.97:
        return (128, 78, 46)
    if leon_art.noise(x // 3, y, 82) > 0.95:
        return shade(tint, 1.3)
    return shade(tint, 1.0 if value_noise(x, y, 8, 64, 83) > 0.45 else 0.92)


def paint_crane(x, y):
    """64 x 64 (1 m): the crane's yellow paint, worn to the primer and rusted at the edges of its plates."""
    tint = (222, 176, 40)
    if y < 2 or x < 2:
        return (104, 80, 34)
    wear = value_noise(x, y, 16, 64, 91)
    if wear > 0.74:
        return (146, 92, 48) if leon_art.noise(x, y, 92) > 0.3 else (120, 74, 40)
    k = 1.0 + 0.1 * (value_noise(x, y, 4, 64, 93) - 0.5)
    return shade(tint, max(0.9, min(1.1, k)))


def make_container_painter(paint):
    """64 x 128 (1 m x the container's 2.6 m): a container's corrugated wall in its paint: ribs every 25 cm, the top
    and bottom rails, scrapes to the steel and rust."""

    def painter(x, y):
        if y < 8 or y >= 120:
            return shade(paint, 0.62 if y in (0, 127) else 0.74)
        rib = x % 16
        light = (1.1, 1.1, 1.0, 0.86, 0.8, 0.86, 1.0, 1.0, 1.0, 1.0, 1.04, 1.04, 1.0, 1.0, 1.0, 1.0)[rib]
        if leon_art.noise(x // 2, y // 3, 101) > 0.975:
            return (124, 76, 44)
        if value_noise(x, y, 16, 64, 102) > 0.82:
            light *= 0.88
        return shade(paint, light, 0.04)

    return painter


def paint_pallet(x, y):
    """64 x 64 (a face): a pine pallet crate: four horizontal slats with dark gaps between them, the corner posts, nail
    heads, a stencilled mark."""
    if x < 6 or x >= 58:
        return shade((176, 138, 86), 0.84 if x in (0, 63) else 0.92)
    slat = y // 16
    yy = y % 16
    if yy < 2:
        return (64, 48, 30)
    if (x in (8, 9, 54, 55)) and yy in (7, 8):
        return (70, 70, 72)
    if 22 <= x < 42 and slat == 2 and 4 <= yy < 12 and (x // 2) % 3 != 2:
        return (52, 46, 40)
    k = (0.96, 1.04, 1.0, 1.08)[slat]
    if leon_art.noise(x // 4, y, 103) > 0.86:
        k *= 0.9
    return shade((206, 168, 112), k, 0.02)


def paint_water(x, y):
    """64 x 64 (4 m): the harbour's water: a dark green-blue, ripples as light and dark wavy lines."""
    wave = math.sin((x / 64.0) * 2.0 * math.pi * 2.0 + y * 0.35) * 2.0
    band = (y + int(round(wave))) % 8
    tint = (34, 72, 80)
    if band == 0 and leon_art.noise(x // 4, y, 111) > 0.35:
        return shade(tint, 1.4)
    if band == 4 and leon_art.noise(x // 4, y, 112) > 0.5:
        return shade(tint, 0.8)
    return shade(tint, 1.0 if value_noise(x, y, 16, 64, 113) > 0.5 else 1.08)


def paint_shutter(x, y):
    """64 x 64 (a door): a roller shutter: slats every 4 texels, its guides at the sides, the bottom bar."""
    if x < 3 or x >= 61:
        return (70, 74, 78)
    if y < 4:
        return (60, 62, 64)
    k = 1.06 if y % 4 == 3 else (0.88 if y % 4 == 0 else 1.0)
    if leon_art.noise(x // 3, y // 4, 121) > 0.93:
        k *= 0.86
    return shade((132, 140, 128), k, 0.02)


GLYPHS = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
}


def paint_signs(x, y):
    """128 x 64: two blue steel plates with a white border, the letters A and B stencilled on them in white."""
    letter = "A" if x < 64 else "B"
    lx = x % 64
    if lx < 2 or lx >= 62 or y < 2 or y >= 62:
        return (50, 54, 60)
    if lx < 5 or lx >= 59 or y < 5 or y >= 59:
        return (226, 228, 222)
    gx, gy = (lx - 12) // 8, (56 - y) // 7
    if 0 <= gx < 5 and 0 <= gy < 7 and 12 <= lx < 52 and 7 <= y <= 56 and GLYPHS[letter][gy][gx] == "#":
        return (232, 234, 228) if leon_art.noise(x, y, 131) < 0.92 else (200, 204, 204)
    return (36, 74, 138) if leon_art.noise(x, y, 132) < 0.85 else (32, 66, 124)


# name: (painter, width, height, colour limit, tile in metres (u, v); None: each face maps the whole image)
TEXTURES = {
    "Asphalt": (paint_asphalt, 128, 128, 16, (2.0, 2.0)),
    "Concrete": (paint_concrete, 128, 128, 16, (2.0, 2.0)),
    "Panel": (paint_panel, 128, 128, 16, (2.0, 2.0)),
    "Corrugated": (paint_corrugated, 128, 128, 256, (2.0, 2.0)),
    "Girder": (paint_girder, 64, 64, 16, (1.0, 1.0)),
    "Crane": (paint_crane, 64, 64, 16, (1.0, 1.0)),
    "ContainerRed": (make_container_painter((168, 52, 36)), 64, 128, 16, (1.0, CONTAINER[2])),
    "ContainerBlue": (make_container_painter((40, 86, 150)), 64, 128, 16, (1.0, CONTAINER[2])),
    "ContainerGreen": (make_container_painter((58, 120, 72)), 64, 128, 16, (1.0, CONTAINER[2])),
    "Pallet": (paint_pallet, 64, 64, 16, None),
    "Water": (paint_water, 64, 64, 16, (4.0, 4.0)),
    "Shutter": (paint_shutter, 64, 64, 16, None),
    "Signs": (paint_signs, 128, 64, 16, None),
}
# Plain colours (linear RGB): the lamps, the bollards and the ship.
PLAIN = {"Lamp": (1.0, 0.86, 0.6), "Bollard": (0.05, 0.05, 0.06), "Hull": (0.24, 0.05, 0.04),
         "ShipWhite": (0.72, 0.72, 0.7)}
# Each material's physical material (N30f): the asphalt, the concrete and the panels concrete, every painted steel
# metal, the pallets wood, the water dirt (only a stray bullet reaches it: the quay's clip keeps players out).
SURFACES = {"Asphalt": "Concrete", "Concrete": "Concrete", "Panel": "Concrete", "Corrugated": "Metal",
            "Girder": "Metal", "Crane": "Metal", "ContainerRed": "Metal", "ContainerBlue": "Metal",
            "ContainerGreen": "Metal", "Pallet": "Wood", "Water": "Dirt", "Shutter": "Metal", "Signs": "Metal",
            "Lamp": "Metal", "Bollard": "Metal", "Hull": "Metal", "ShipWhite": "Metal"}
CONTAINER_PAINTS = {"Red": "ContainerRed", "Blue": "ContainerBlue", "Green": "ContainerGreen"}


def make_materials():
    materials = {}
    for name, (painter, width, height, limit, _tile) in TEXTURES.items():
        image = leon_art.paint_image(name + "_D", width, height, painter)
        colours = leon_art.count_colours(image)
        assert colours <= limit, "%s has %d colours, over %d" % (name, colours, limit)
        print("make_de_harbor: texture %-14s %3d x %3d, %3d colours" % (name, width, height, colours))
        materials[name] = leon_art.make_material(name, (1.0, 1.0, 1.0), 0.9, image, SURFACES[name])
    for name, rgb in PLAIN.items():
        materials[name] = leon_art.make_material(name, rgb, 0.6, surface=SURFACES[name])
    return materials


# Geometry: pieces made of axis-aligned boxes whose faces are cut for the bake (make_de_leon.py's).

def grid_breaks(lo, hi, step=STEP):
    """lo, the multiples of step between lo and hi (none closer than MIN_SEGMENT to either end), hi."""
    points = [lo]
    k = math.floor(lo / step) + 1
    while k * step < hi - 1.0e-6:
        g = k * step
        if g - lo > MIN_SEGMENT and hi - g > MIN_SEGMENT:
            points.append(g)
        k += 1
    points.append(hi)
    return points


def height_breaks(lo, hi):
    """A wall's rows: a cut FOOT above the floor, for the occlusion at its foot."""
    if lo < FOOT - MIN_SEGMENT and hi > FOOT + MIN_SEGMENT:
        return [lo, FOOT, hi]
    return [lo, hi]


# Each face of a box: its outward normal, its u axis (the viewer's right) and its v axis (up), engine axes.
FACES = {
    "+x": ((1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, 1.0)),
    "-x": ((-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)),
    "+y": ((0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    "-y": ((0.0, -1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    "+z": ((0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0)),
    "-z": ((0.0, 0.0, -1.0), (0.0, 1.0, 0.0), (-1.0, 0.0, 0.0)),
}


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


class Piece:
    """One mesh node: faces in world metres (the engine's axes) with a material each, UVs planar in the world (a
    material's tile) or over the whole image; placed at its bounds' centre."""

    def __init__(self, name, cell):
        self.name = name
        self.cell = cell
        self.positions = []
        self.faces = []
        self.material_names = []

    def triangles(self):
        return sum(len(indices) - 2 for indices, _uvs, _material in self.faces)

    def bounds(self):
        xs = [p[0] for p in self.positions]
        ys = [p[1] for p in self.positions]
        zs = [p[2] for p in self.positions]
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def _uv(self, point, face, material, whole):
        _normal, right, up = FACES[face]
        if whole is not None:
            (u0, u1), (v0, v1), region = whole
            u = (_dot(point, right) - u0) / (u1 - u0)
            v = (_dot(point, up) - v0) / (v1 - v0)
            x0, y0, x1, y1 = region
            return (x0 + (x1 - x0) * u, y0 + (y1 - y0) * v)
        if material in PLAIN:
            # A plain colour: no texture to tile (a quad's coordinates must not span more than 14 repeats).
            return (0.0, 0.0)
        tile = TEXTURES[material][4] if material in TEXTURES and TEXTURES[material][4] else (1.0, 1.0)
        return (_dot(point, right) / tile[0], _dot(point, up) / tile[1])

    def box(self, lo, hi, materials, skip=(), whole=None, cut=True):
        """An axis-aligned box from lo to hi (engine metres). materials: one name, or a dict by face ('+x' ... '-z',
        'side' for the four vertical faces, 'top', 'bottom'). skip leaves faces out. cut: the faces on the bake's
        grid; whole: map each face onto a region (x0, y0, x1, y1) of its image instead of the world tile."""
        for face in ("+x", "-x", "+y", "-y", "+z", "-z"):
            if face in skip:
                continue
            if isinstance(materials, str):
                material = materials
            else:
                material = materials.get(face) or materials.get(
                    "top" if face == "+z" else "bottom" if face == "-z" else "side")
            normal, right, up = FACES[face]
            axis = [i for i in range(3) if normal[i] != 0.0][0]
            fixed = hi[axis] if normal[axis] > 0.0 else lo[axis]
            u_axis = [i for i in range(3) if right[i] != 0.0][0]
            v_axis = [i for i in range(3) if up[i] != 0.0][0]
            if cut:
                u_cuts = grid_breaks(lo[u_axis], hi[u_axis])
                v_cuts = height_breaks(lo[v_axis], hi[v_axis]) if v_axis == 2 else grid_breaks(lo[v_axis], hi[v_axis])
            else:
                u_cuts = [lo[u_axis], hi[u_axis]]
                v_cuts = [lo[v_axis], hi[v_axis]]
            face_whole = None
            if whole is not None:
                sign_u = right[u_axis]
                sign_v = up[v_axis]
                u_range = sorted((lo[u_axis] * sign_u, hi[u_axis] * sign_u))
                v_range = sorted((lo[v_axis] * sign_v, hi[v_axis] * sign_v))
                face_whole = (u_range, v_range, whole)
            grid = {}
            for i, u in enumerate(u_cuts):
                for j, v in enumerate(v_cuts):
                    p = [0.0, 0.0, 0.0]
                    p[axis] = fixed
                    p[u_axis] = u
                    p[v_axis] = v
                    grid[(i, j)] = tuple(p)
            for i in range(len(u_cuts) - 1):
                for j in range(len(v_cuts) - 1):
                    corners = [grid[(i, j)], grid[(i + 1, j)], grid[(i + 1, j + 1)], grid[(i, j + 1)]]
                    self.quad(corners, face, material, normal, face_whole)

    def quad(self, corners, face, material, outward, whole=None):
        if material not in self.material_names:
            self.material_names.append(material)
        indices = []
        uvs = []
        for corner in corners:
            self.positions.append(corner)
            indices.append(len(self.positions) - 1)
            uvs.append(self._uv(corner, face, material, whole))
        # Wound to face `outward` in Blender's (mirrored) axes, so the engine sees it from the front.
        points = [to_blender(*self.positions[i]) for i in indices]
        normal = Vector((0.0, 0.0, 0.0))
        for a, b in zip(points, points[1:] + points[:1]):
            normal.x += (a.y - b.y) * (a.z + b.z)
            normal.y += (a.z - b.z) * (a.x + b.x)
            normal.z += (a.x - b.x) * (a.y + b.y)
        if normal.dot(to_blender(*outward)) < 0.0:
            indices.reverse()
            uvs.reverse()
        self.faces.append((indices, uvs, material))

    def build_mesh(self, name, materials, origin):
        """The Blender mesh around `origin` (engine metres), its vertices welded where position and UV agree."""
        verts = []
        vert_index = {}
        polygons = []
        for indices, _uvs, _material in self.faces:
            polygon = []
            for i in indices:
                p = self.positions[i]
                key = (round(p[0], 5), round(p[1], 5), round(p[2], 5))
                if key not in vert_index:
                    vert_index[key] = len(verts)
                    verts.append(to_blender(p[0] - origin[0], p[1] - origin[1], p[2] - origin[2]))
                polygon.append(vert_index[key])
            polygons.append(polygon)
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata([tuple(v) for v in verts], [], polygons)
        uv_layer = mesh.uv_layers.new(name="UVMap")
        for polygon, (_indices, uvs, material) in zip(mesh.polygons, self.faces):
            polygon.use_smooth = False
            polygon.material_index = self.material_names.index(material)
            for corner, loop in enumerate(polygon.loop_indices):
                uv_layer.data[loop].uv = uvs[corner]
        for material in self.material_names:
            mesh.materials.append(materials[material])
        mesh.update()
        return mesh


def box_mesh(name, lo, hi):
    """A plain box mesh (a UCX_ collision node's, a cell's), in Blender's axes."""
    a, b = to_blender(*lo), to_blender(*hi)
    xs, ys, zs = sorted((a.x, b.x)), sorted((a.y, b.y)), sorted((a.z, b.z))
    verts = [(x, y, z) for x in xs for y in ys for z in zs]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    return mesh


class MapBuilder:
    def __init__(self, scene, materials):
        self.scene = scene
        self.materials = materials
        self.collections = {}
        for name in (["Cell_" + cell for cell in CELLS + [OUTSIDE]] +
                     ["Collision", "Gameplay", "Navigation", "Cells", "Lights"]):
            collection = bpy.data.collections.new(name)
            scene.collection.children.link(collection)
            self.collections[name] = collection
        self.cell_triangles = {cell: 0 for cell in CELLS + [OUTSIDE]}
        self.pieces = 0
        # Footprints of what stands at walking height (x0, x1, y0, y1, name, z0, z1), for the waypoint checks.
        self.obstacles = []
        self.shared = {}

    def add_collision(self, target, lo, hi):
        obj = leon_art.add_object("UCX_%s_01" % target, box_mesh("UCX_%s_01" % target, lo, hi),
                                  self.collections["Collision"])
        obj.display_type = "WIRE"
        return obj

    def add_piece(self, piece, collision=None, obstacle=True):
        """The piece's mesh node at its bounds' centre and its UCX_ box (its bounds, or `collision` (lo, hi))."""
        lo, hi = piece.bounds()
        origin = ((lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5, (lo[2] + hi[2]) * 0.5)
        mesh = piece.build_mesh(piece.name, self.materials, origin)
        obj = leon_art.add_object(piece.name, mesh, self.collections["Cell_" + piece.cell])
        obj.location = to_blender(*origin)
        collision_lo, collision_hi = collision if collision is not None else (lo, hi)
        self.add_collision(piece.name, collision_lo, collision_hi)
        self.cell_triangles[piece.cell] += piece.triangles()
        self.pieces += 1
        if obstacle and collision_lo[2] < 1.9:
            self.obstacles.append((collision_lo[0], collision_hi[0], collision_lo[1], collision_hi[1], piece.name,
                                   collision_lo[2], collision_hi[2]))
        return obj

    def solid(self, cell, name, lo, hi, skip=(), materials=None):
        """A block (a wall, a shed, a building): corrugated sides and a steel top unless told otherwise, its bottom on
        the floor left out."""
        piece = Piece("%s_%s" % (cell, name), cell)
        if materials is None:
            materials = {"side": "Corrugated", "top": "Girder", "bottom": "Girder"}
        hidden = set(skip)
        if lo[2] <= 0.0:
            hidden.add("-z")
        piece.box(lo, hi, materials, skip=hidden)
        return self.add_piece(piece)

    def wall(self, cell, name, lo, hi, skip=()):
        """A boundary wall of precast panels, a concrete cap."""
        return self.solid(cell, name, lo, hi, skip, {"side": "Panel", "top": "Concrete"})

    def floor(self, cell, name, x0, x1, y0, y1, material):
        """A ground slab (0.2 m, its top at Z = 0) cut on the grid, its underside closed for the bake's rays; its UCX_
        box is the ground players walk on, with the slab's material (make_de_leon.py's floor)."""
        piece = Piece("%s_%s" % (cell, name), cell)
        piece.box((x0, y0, -0.2), (x1, y1, 0.0), material, skip=("+x", "-x", "+y", "-y", "-z"))
        piece.box((x0, y0, -0.2), (x1, y1, 0.0), material, skip=("+x", "-x", "+y", "-y", "+z"),
                  whole=(0.0, 0.0, 1.0, 1.0), cut=False)
        return self.add_piece(piece, collision=((x0, y0, -0.2), (x1, y1, 0.0)), obstacle=False)

    def shared_mesh(self, name, size, material, whole, cut=False):
        """A prop's mesh shared by its instances, its origin on the floor at its middle; size (x, y, z)."""
        if name not in self.shared:
            piece = Piece(name, OUTSIDE)
            half_x, half_y = size[0] * 0.5, size[1] * 0.5
            piece.box((-half_x, -half_y, 0.0), (half_x, half_y, size[2]), material, skip=("-z",), whole=whole,
                      cut=cut)
            self.shared[name] = (piece.build_mesh(name, self.materials, (0.0, 0.0, 0.0)), piece.triangles(), size)
        return self.shared[name]

    def instance(self, cell, name, mesh_name, mesh_info, x, y, z, obstacle=True):
        """An instance of a shared mesh at (x, y, z); the first instance of a mesh carries its UCX_ box."""
        mesh, triangles, size = mesh_info
        obj = leon_art.add_object(name, mesh, self.collections["Cell_" + cell])
        obj.location = to_blender(x, y, z)
        lo = (x - size[0] * 0.5, y - size[1] * 0.5, z)
        hi = (x + size[0] * 0.5, y + size[1] * 0.5, z + size[2])
        if not self.shared.get(mesh_name + ":collision"):
            self.shared[mesh_name + ":collision"] = True
            self.add_collision(name, lo, hi)
        self.cell_triangles[cell] += triangles
        self.pieces += 1
        if obstacle and z < 1.9:
            self.obstacles.append((lo[0], hi[0], lo[1], hi[1], name, lo[2], hi[2]))
        return obj

    def crate(self, cell, index, x, y, z=0.0, big=False):
        """A pine pallet crate (1.1 m, or 1.6 m) standing at (x, y, z)."""
        side = CRATE_BIG if big else CRATE
        mesh_name = "PalletBig" if big else "Pallet"
        info = self.shared_mesh(mesh_name, (side, side, side), "Pallet", (0.0, 0.0, 1.0, 1.0))
        return self.instance(cell, "%s_%s_%02d" % (cell, "CrateBig" if big else "Crate", index), mesh_name, info,
                             x, y, z)

    def container(self, cell, index, paint, axis, x0, y0, z=0.0):
        """A shipping container in `paint` (Red, Blue, Green), its length along `axis` ('x' or 'y'), its corner at
        (x0, y0) and its floor at z (2.6 m: on another). Its faces are cut on its own 3 m grid for the bake."""
        length, width, height = CONTAINER
        size = (length, width, height) if axis == "x" else (width, length, height)
        mesh_name = "Container_%s_%s" % (paint, axis.upper())
        info = self.shared_mesh(mesh_name, size, CONTAINER_PAINTS[paint], None, cut=True)
        return self.instance(cell, "%s_Container_%02d" % (cell, index), mesh_name, info,
                             x0 + size[0] * 0.5, y0 + size[1] * 0.5, z)

    def empty(self, collection, name, center, display="CUBE", scale=(1.0, 1.0, 1.0), yaw=0.0):
        """An empty at an engine location; yaw in the engine's sense (from +X toward +Y), i.e. minus Blender's."""
        obj = leon_art.add_object(name, None, self.collections[collection])
        obj.empty_display_type = display
        obj.location = to_blender(*center)
        obj.scale = Vector(scale)
        obj.rotation_euler = (0.0, 0.0, -math.radians(yaw))
        return obj

    def volume(self, collection, name, lo, hi):
        """A box volume from lo to hi: a cube empty, 2 m x its scale (the importer's box without a mesh)."""
        center = tuple((a + b) * 0.5 for a, b in zip(lo, hi))
        scale = tuple(max((b - a) * 0.5, 0.005) for a, b in zip(lo, hi))
        return self.empty(collection, name, center, "CUBE", scale)

    def point_light(self, name, position, energy, radius, colour=(1.0, 0.86, 0.6)):
        data = bpy.data.lights.new(name, "POINT")
        data.energy = energy
        data.color = colour
        data.use_custom_distance = True
        data.cutoff_distance = radius
        data.shadow_soft_size = 0.0
        light = leon_art.add_object(name, data, self.collections["Lights"])
        light.location = to_blender(*position)
        return light

    def lamp(self, cell, index, x, y, ceiling, energy, radius, name):
        """A lamp box (shared) under a ceiling at `ceiling` and its point light 25 cm below."""
        info = self.shared_mesh("Lamp", (0.6, 0.3, 0.15), "Lamp", None)
        self.instance(cell, "%s_Lamp_%02d" % (cell, index), "Lamp", info, x, y, ceiling - 0.15, obstacle=False)
        self.point_light(name, (x, y, ceiling - 0.4), energy, radius)


def build_plaque(builder, cell, name, lo, hi, face, material, region, collision):
    """A plaque (a sign, a shutter) on a wall: a thin box, its image region on its `face`, steel on its edges, its back
    against the wall left out; its UCX_ inside the wall."""
    piece = Piece("%s_%s" % (cell, name), cell)
    piece.box(lo, hi, material, skip=tuple(f for f in FACES if f != face), whole=region, cut=False)
    back = face.replace("+", "#").replace("-", "+").replace("#", "-")
    piece.box(lo, hi, "Girder", skip=(face, back), cut=False)
    return builder.add_piece(piece, collision=collision, obstacle=False)


def build_ladder(builder, cell, name, x, wall_y, facing, height):
    """A steel ladder on the face at Y = wall_y whose climbers stand on the `facing` side (+1: +Y): rails and rungs
    every 35 cm, the Ladder trigger volume (20 cm from the floor to the top, N30c) and a UCX_ box inside the wall
    (make_de_leon.py's ladder)."""
    piece = Piece("%s_%s" % (cell, name), cell)
    back = "-y" if facing > 0 else "+y"

    def span(depth0, depth1):
        a, b = wall_y + facing * depth0, wall_y + facing * depth1
        return min(a, b), max(a, b)

    y0, y1 = span(0.0, 0.08)
    for rail_x in (x - 0.42, x + 0.42):
        piece.box((rail_x - 0.04, y0, 0.0), (rail_x + 0.04, y1, height), "Girder", skip=(back, "-z"), cut=False)
    y0, y1 = span(0.01, 0.06)
    rung = 0.35
    while rung < height - 0.1:
        piece.box((x - 0.38, y0, rung - 0.025), (x + 0.38, y1, rung + 0.025), "Girder", skip=(back, "+x", "-x"),
                  cut=False)
        rung += 0.35
    inside0, inside1 = span(-0.15, -0.05)
    builder.add_piece(piece, collision=((x - 0.5, inside0, 0.0), (x + 0.5, inside1, height)), obstacle=False)
    v0, v1 = span(0.0, 0.2)
    builder.volume("Gameplay", "Ladder_%s" % name[-1], (x - 0.5, v0, 0.0), (x + 0.5, v1, height))


def build_geometry(b):
    """The map (the plan in Docs/LEVELS.md): a 64 x 56 m field (X -32 .. 32, Y -28 .. 28), the T yard south, the CT
    yard north, the quay and A east, the warehouse and B west; the boundary walls stand outside it, the water beyond
    the quay's edge at Y = 28."""
    asphalt, concrete = "Asphalt", "Concrete"
    height = CONTAINER[2]

    # --- T spawn: the truck yard south of X = -18, a gatehouse in its corner, containers.
    b.wall("TSpawn", "Wall_South", (-32.5, -28.5, 0.0), (-32.0, 28.0, 5.0), skip=("-x", "+y", "-y"))
    b.wall("TSpawn", "Wall_West", (-32.0, -28.5, 0.0), (-18.0, -28.0, 5.0), skip=("-y", "+x", "-x"))
    b.solid("TSpawn", "Gatehouse", (-32.0, -28.0, 0.0), (-27.0, -23.0, 3.5), skip=("-x", "-y"),
            materials={"side": "Panel", "top": "Girder"})
    b.floor("TSpawn", "Floor", -32.0, -18.0, -28.0, 28.0, asphalt)
    b.container("TSpawn", 1, "Blue", "y", -30.0, 8.0)
    b.container("TSpawn", 2, "Red", "x", -30.0, 22.0)
    b.container("TSpawn", 3, "Green", "x", -30.0, 22.0, height)
    b.container("TSpawn", 4, "Green", "x", -31.0, -14.44)
    b.crate("TSpawn", 1, -22.0, 6.5)
    b.crate("TSpawn", 2, -22.0, 7.6)

    # --- A long: the quay (Y 18 .. 28) between the customs shed and the water, entered across the T yard's apron
    # (X -18 .. -12) by the shed's corner, a security booth in its mouth.
    b.solid("LongA", "Customs", (-12.0, 6.0, 0.0), (8.0, 18.0, SKY))
    b.floor("LongA", "Floor_Apron", -18.0, -12.0, 6.0, 18.0, asphalt)
    build_plaque(b, "LongA", "Shutter_Long", (-10.0, 18.0, 0.0), (-6.5, 18.04, 3.2), "+y", "Shutter",
                 (0.0, 0.0, 1.0, 1.0), ((-10.0, 17.6, 0.0), (-6.5, 17.9, 3.2)))
    build_plaque(b, "LongA", "Shutter_Mid", (-6.0, 5.96, 0.0), (-2.5, 6.0, 3.2), "-y", "Shutter",
                 (0.0, 0.0, 1.0, 1.0), ((-6.0, 6.1, 0.0), (-2.5, 6.4, 3.2)))
    b.solid("LongA", "Booth", (-17.5, 23.0, 0.0), (-15.0, 25.5, 3.0), materials={"side": "Panel", "top": "Girder"})
    b.floor("LongA", "Floor", -18.0, 8.0, 18.0, 28.0, concrete)
    b.container("LongA", 1, "Red", "x", -9.0, 24.6)
    # The long's end: a chicane of two stacks (this one against the shed, the next from the water at A's edge), so
    # the quay's line never reaches into the site.
    b.container("LongA", 2, "Green", "y", 3.5, 18.0)
    b.container("LongA", 3, "Blue", "y", 3.5, 18.0, height)
    b.crate("LongA", 1, 0.5, 19.5)
    b.crate("LongA", 2, 1.6, 19.5)
    b.crate("LongA", 3, 1.05, 19.5, CRATE)
    b.crate("LongA", 4, -12.0, 26.5)

    # --- Mid: the container lane (Y -6 .. 6), a stack on each side of its southern mouth, the yard's gate at its
    # northern end (a 3.5 m gap in a 4 m wall: the courtyard's stack behind it hides the CT spawn from mid).
    b.floor("Mid", "Floor", -18.0, 8.0, -6.0, 6.0, asphalt)
    b.wall("Mid", "Gate_West", (7.0, -6.0, 0.0), (8.0, -GATE, GATE_HEIGHT), skip=("-y",))
    b.wall("Mid", "Gate_East", (7.0, GATE, 0.0), (8.0, 6.0, GATE_HEIGHT), skip=("+y",))
    b.container("Mid", 1, "Blue", "x", -18.0, -6.0)
    b.container("Mid", 2, "Red", "x", -18.0, -6.0, height)
    b.container("Mid", 3, "Green", "x", -18.0, 3.56)
    b.container("Mid", 4, "Blue", "x", -18.0, 3.56, height)
    b.crate("Mid", 1, -6.0, 2.5)
    b.crate("Mid", 2, -6.0, 3.6)
    b.crate("Mid", 3, 1.0, -4.5)

    # --- The connector: a roofed passage (X -4 .. -1) from mid to the alley between two blocks, two lamps; the T
    # yard's apron (X -18 .. -12) before the alley.
    b.solid("Connector", "Block_South", (-12.0, -22.0, 0.0), (-4.0, -6.0, SKY))
    b.floor("Connector", "Floor_Apron", -18.0, -12.0, -22.0, -6.0, asphalt)
    b.solid("Connector", "Block_North", (-1.0, -22.0, 0.0), (8.0, -6.0, SKY))
    b.solid("Connector", "Roof", (-4.0, -22.0, 3.2), (-1.0, -6.0, 3.6), skip=("+x", "-x"),
            materials={"side": "Girder", "top": "Girder", "bottom": "Girder"})
    b.floor("Connector", "Floor", -4.0, -1.0, -22.0, -6.0, concrete)
    b.lamp("Connector", 1, -2.5, -10.0, 3.2, 1.6, 7.0, "Light_Connector_01")
    b.lamp("Connector", 2, -2.5, -18.0, 3.2, 1.6, 7.0, "Light_Connector_02")

    # --- The alley (Y -28 .. -22) to the warehouse's front door, a shed roof over its southern end.
    b.wall("Alley", "Wall_West", (-18.0, -28.5, 0.0), (8.0, -28.0, 5.0), skip=("-y", "+x", "-x"))
    b.solid("Alley", "ShedRoof", (-18.0, -28.0, 3.2), (-10.0, -22.0, 3.6), skip=("-y",),
            materials={"side": "Girder", "top": "Corrugated", "bottom": "Girder"})
    b.floor("Alley", "Floor", -18.0, 8.0, -28.0, -22.0, asphalt)
    b.lamp("Alley", 1, -14.0, -25.0, 3.2, 1.6, 7.0, "Light_Alley_01")
    b.container("Alley", 1, "Red", "x", -9.0, -28.0 + 0.02)
    b.crate("Alley", 1, 2.0, -23.0)

    # --- The courtyard before the CT spawn: a stack across the line of mid, a fence to the CT spawn with a 6 m opening
    # behind the stack.
    b.floor("Courtyard", "Floor", 8.0, 20.0, -12.0, 12.0, concrete)
    b.wall("Courtyard", "Fence_North", (19.5, FENCE, 0.0), (20.0, 12.0, GATE_HEIGHT), skip=("+y",))
    b.wall("Courtyard", "Fence_South", (19.5, -12.0, 0.0), (20.0, -FENCE, GATE_HEIGHT), skip=("-y",))
    b.container("Courtyard", 1, "Red", "y", 13.0, -3.0)
    b.container("Courtyard", 2, "Green", "y", 13.0, -3.0, height)
    b.crate("Courtyard", 1, 10.0, -10.5)
    b.crate("Courtyard", 2, 11.1, -10.5)

    # --- The CT spawn: the harbour office against the north wall.
    b.wall("CTSpawn", "Wall_North", (32.0, -12.0, 0.0), (32.5, 12.0, 5.0), skip=("+x", "+y", "-y"))
    b.solid("CTSpawn", "Office", (29.5, -4.0, 0.0), (32.0, 4.0, 4.5), skip=("+x",),
            materials={"side": "Panel", "top": "Girder"})
    b.floor("CTSpawn", "Floor", 20.0, 32.0, -12.0, 12.0, concrete)
    b.crate("CTSpawn", 1, 23.0, 11.3)

    # --- Site A: the quay apron (X 8 .. 32, Y 12 .. 28) under the gantry crane, containers, the ladder up a stack; a
    # wall of stacked containers closes it to the courtyard and most of the CT spawn (A is entered from the quay and
    # by a 6 m gap from the CT spawn).
    b.wall("SiteA", "Wall_North", (32.0, 12.0, 0.0), (32.5, 28.0, 5.0), skip=("+x", "-y", "+y"))
    b.floor("SiteA", "Floor", 8.0, 32.0, 12.0, 28.0, concrete)
    b.container("SiteA", 1, "Red", "x", 8.0, 12.0)
    b.container("SiteA", 2, "Blue", "x", 8.0, 12.0, height)
    b.container("SiteA", 6, "Green", "x", 14.0, 12.0)
    b.container("SiteA", 7, "Red", "x", 14.0, 12.0, height)
    b.container("SiteA", 9, "Red", "x", 20.0, 12.0)
    b.container("SiteA", 10, "Green", "x", 20.0, 12.0, height)
    # The chicane's second stack: the way in is the 3.6 m gap by the customs shed's corner.
    b.container("SiteA", 8, "Blue", "y", 8.0, 21.6)
    b.container("SiteA", 11, "Red", "y", 8.0, 21.6, height)
    b.container("SiteA", 4, "Green", "x", 22.0, 22.0)
    b.container("SiteA", 5, "Red", "x", 22.0, 22.0, height)
    build_ladder(b, "SiteA", "Ladder_A", 25.0, 22.0, -1.0, 2.0 * height)
    build_plaque(b, "SiteA", "Sign", (21.96, 22.62, 1.2), (22.0, 23.82, 2.4), "-x", "Signs", (0.0, 0.0, 0.5, 1.0),
                 ((22.05, 22.62, 1.2), (22.4, 23.82, 2.4)))
    b.crate("SiteA", 1, 19.5, 26.2)
    b.crate("SiteA", 2, 20.6, 26.2)
    b.crate("SiteA", 3, 20.05, 26.2, CRATE)
    b.crate("SiteA", 4, 24.5, 16.8)
    b.crate("SiteA", 5, 17.0, 19.0, big=True)
    # The gantry crane: four legs on the rails (Y 15.2 and 26.8), to the cells' top; above it the legs go on to the
    # portal beams at 11 m, the boom over the water and the trolley's cab, in no cell.
    legs = [(16.5, 15.2), (30.5, 15.2), (16.5, 26.8), (30.5, 26.8)]
    leg_top = CELL_TOP + 0.1
    for index, (x, y) in enumerate(legs, start=1):
        leg = Piece("SiteA_CraneLeg_%02d" % index, "SiteA")
        leg.box((x - 0.4, y - 0.4, 0.0), (x + 0.4, y + 0.4, leg_top), "Crane", skip=("-z", "+z"))
        b.add_piece(leg)
    top = Piece("Outside_CraneTop", OUTSIDE)
    for x, y in legs:
        top.box((x - 0.4, y - 0.4, leg_top), (x + 0.4, y + 0.4, 11.0), "Crane", skip=("-z", "+z"), cut=False)
    for y in (15.2, 26.8):
        top.box((15.5, y - 0.5, 11.0), (31.5, y + 0.5, 12.2), "Crane")
    for x in (16.5, 30.5):
        top.box((x - 0.4, 15.7, 11.2), (x + 0.4, 26.3, 12.0), "Crane", skip=("+y", "-y"), cut=False)
    top.box((21.8, 8.0, 12.2), (23.2, 44.0, 13.0), "Crane")
    top.box((21.0, 19.0, 9.6), (24.0, 22.0, 11.0), {"side": "Girder", "top": "Girder", "bottom": "Crane"},
            cut=False)
    b.add_piece(top, collision=((21.0, 19.0, 9.6), (24.0, 22.0, 11.0)), obstacle=False)
    bollard = b.shared_mesh("Bollard", (0.4, 0.4, 0.7), "Bollard", None)
    for index, (cell, x) in enumerate((("TSpawn", -26.0), ("LongA", -14.0), ("LongA", -5.0), ("SiteA", 13.0),
                                       ("SiteA", 22.0)), start=1):
        b.instance(cell, "%s_Bollard_%02d" % (cell, index), "Bollard", bollard, x, 27.45, 0.0)

    # --- Site B: the warehouse (X 8 .. 32, Y -28 .. -12), roofed at 6.4 m, two doors: the front (from the alley) and
    # the CT spawn's; four lamps under the roof.
    roof = 6.4
    b.solid("SiteB", "Wall_West", (8.0, -28.5, 0.0), (32.5, -28.0, roof), skip=("-y", "+x"))
    b.solid("SiteB", "Wall_North", (32.0, -28.0, 0.0), (32.5, -12.0, roof), skip=("+x", "-y"))
    b.solid("SiteB", "Front_01", (8.0, -28.0, 0.0), (8.5, -27.0, roof), skip=("-y",))
    b.solid("SiteB", "Front_Lintel", (8.0, -27.0, 4.0), (8.5, -23.0, roof), skip=("+y", "-y"))
    b.solid("SiteB", "Front_02", (8.0, -23.0, 0.0), (8.5, -12.0, roof))
    b.solid("SiteB", "Side_01", (8.5, -12.5, 0.0), (23.5, -12.0, roof), skip=("-x",))
    b.solid("SiteB", "Side_Lintel", (23.5, -12.5, 3.5), (28.5, -12.0, roof), skip=("+x", "-x"))
    b.solid("SiteB", "Side_02", (28.5, -12.5, 0.0), (32.0, -12.0, roof), skip=("+x",))
    roof_piece = Piece("SiteB_Roof", "SiteB")
    roof_piece.box((8.0, -28.5, roof), (32.5, -12.0, roof + 0.4), "Girder", skip=("+x", "-y", "+z", "-z"))
    roof_piece.box((8.0, -28.5, roof), (32.5, -12.0, roof + 0.4), "Corrugated",
                   skip=("+x", "-x", "+y", "-y", "-z"), cut=False)
    roof_piece.box((8.0, -28.5, roof), (32.5, -12.0, roof + 0.4), "Corrugated", skip=("+x", "-x", "+y", "-y", "+z"))
    b.add_piece(roof_piece, obstacle=False)
    build_plaque(b, "SiteB", "Sign", (7.96, -25.6, 4.5), (8.0, -24.4, 5.7), "-x", "Signs", (0.5, 0.0, 1.0, 1.0),
                 ((8.05, -25.6, 4.5), (8.4, -24.4, 5.7)))
    b.floor("SiteB", "Floor", 8.0, 32.0, -28.0, -12.0, concrete)
    b.container("SiteB", 1, "Red", "x", 19.0, -26.0)
    # Facing the front door, 3.5 m inside: the alley sees nothing of the hall, the way in turns north.
    b.container("SiteB", 2, "Green", "y", 12.0, -27.8)
    b.crate("SiteB", 1, 30.6, -15.0)
    b.crate("SiteB", 2, 30.6, -16.1)
    b.crate("SiteB", 3, 30.6, -15.55, CRATE)
    b.crate("SiteB", 4, 30.6, -26.6, big=True)
    b.crate("SiteB", 5, 12.0, -14.0)
    for index, (x, y) in enumerate(((14.0, -20.0), (20.0, -15.5), (26.0, -20.0), (20.0, -25.0)), start=1):
        b.lamp("SiteB", index, x, y, roof, 2.6, 11.0, "Light_Warehouse_%02d" % index)

    # --- Outside the field: the quay's face down to the water, the harbour (in 40 m quads: a quad's texture spans at
    # most 14 repeats), a ship moored off the quay.
    # The faces nobody sees (the quay's back under the slabs, the water's underside) give each mesh a thickness on
    # every axis, which the PS2 mesh's quantization needs (a flat mesh's third axis is coarse).
    quay = Piece("Outside_QuayWall", OUTSIDE)
    quay.box((-32.0, 27.6, -1.8), (32.0, 28.0, 0.0), "Concrete", skip=("-x", "+x", "-y", "+z", "-z"))
    quay.box((-32.0, 27.6, -1.8), (32.0, 28.0, -0.2), "Concrete", skip=("-x", "+x", "+y", "+z", "-z"), cut=False,
             whole=(0.0, 0.0, 1.0, 1.0))
    b.add_piece(quay, obstacle=False)
    water = Piece("Outside_Water", OUTSIDE)
    for x0 in (-60.0, -20.0, 20.0):
        water.box((x0, 28.0, WATER_Z - 0.1), (x0 + 40.0, 80.0, WATER_Z), "Water",
                  skip=("+x", "-x", "+y", "-y", "-z"), cut=False)
    water.box((-60.0, 28.0, WATER_Z - 0.1), (60.0, 80.0, WATER_Z), "Water", skip=("+x", "-x", "+y", "-y", "+z"),
              cut=False, whole=(0.0, 0.0, 1.0, 1.0))
    b.add_piece(water, obstacle=False)
    hull = Piece("Outside_ShipHull", OUTSIDE)
    hull.box((-22.0, 46.0, WATER_Z - 0.3), (26.0, 54.0, 4.5), "Hull", skip=("+y", "-z"), cut=False)
    b.add_piece(hull, obstacle=False)
    bridge = Piece("Outside_ShipBridge", OUTSIDE)
    bridge.box((14.0, 47.0, 4.5), (21.0, 53.0, 11.0), "ShipWhite", skip=("+y", "-z"), cut=False)
    for index, x in enumerate((-16.0, -6.0, 4.0)):
        bridge.box((x, 47.0, 4.5), (x + 6.0, 49.44, 7.1), "ContainerBlue" if index % 2 else "ContainerRed",
                   skip=("+y", "-z"), cut=False)
    b.add_piece(bridge, obstacle=False)
    b.volume("Gameplay", "Clip_Quay", (-32.0, 28.0, -2.0), (32.0, 28.5, 8.0))


def build_cells(b):
    """The cells (VIS_<Cell>: boxes over the walkable space up to CELL_TOP) and the portals between them (PORTAL_<A>_<B>
    quads in the plane between two cells: the openings, and the sky over the blocks between them from SKY up). The
    warehouse is closed: its doors are its only portals."""
    cells = {
        "TSpawn": ((-32.0, -28.0), (-18.0, 28.0)),
        "LongA": ((-18.0, 6.0), (8.0, 28.0)),
        "Mid": ((-18.0, -6.0), (8.0, 6.0)),
        "Connector": ((-18.0, -22.0), (8.0, -6.0)),
        "Alley": ((-18.0, -28.0), (8.0, -22.0)),
        "Courtyard": ((8.0, -12.0), (20.0, 12.0)),
        "CTSpawn": ((20.0, -12.0), (32.0, 12.0)),
        "SiteA": ((8.0, 12.0), (32.0, 28.0)),
        "SiteB": ((8.0, -28.0), (32.0, -12.0)),
    }
    for name, ((x0, y0), (x1, y1)) in cells.items():
        obj = leon_art.add_object("VIS_" + name, box_mesh("VIS_" + name, (x0, y0, 0.0), (x1, y1, CELL_TOP)),
                                  b.collections["Cells"])
        obj.display_type = "WIRE"
    # (name, plane axis 'x' or 'y', the plane's coordinate, the span along the other axis, z0, z1)
    portals = [
        ("TSpawn_Mid", "x", -18.0, (-6.0, 6.0), 0.0, CELL_TOP),
        ("TSpawn_LongA", "x", -18.0, (6.0, 28.0), 0.0, CELL_TOP),
        ("TSpawn_Alley", "x", -18.0, (-28.0, -22.0), 0.0, CELL_TOP),
        ("TSpawn_Connector", "x", -18.0, (-22.0, -6.0), 0.0, CELL_TOP),
        ("Mid_LongA", "y", 6.0, (-18.0, 8.0), STACK, CELL_TOP),
        ("Mid_Connector", "y", -6.0, (-4.0, -1.0), 0.0, 3.2),
        ("Mid_Connector", "y", -6.0, (-18.0, 8.0), STACK, CELL_TOP),
        ("Alley_Connector", "y", -22.0, (-18.0, -12.0), 0.0, CELL_TOP),
        ("Alley_Connector", "y", -22.0, (-4.0, -1.0), 0.0, 3.2),
        ("Alley_Connector", "y", -22.0, (-12.0, 8.0), SKY, CELL_TOP),
        ("Mid_Courtyard", "x", 8.0, (-GATE, GATE), 0.0, GATE_HEIGHT),
        ("Mid_Courtyard", "x", 8.0, (-6.0, 6.0), GATE_HEIGHT, CELL_TOP),
        ("Connector_Courtyard", "x", 8.0, (-12.0, -6.0), SKY, CELL_TOP),
        ("LongA_Courtyard", "x", 8.0, (6.0, 12.0), SKY, CELL_TOP),
        ("LongA_SiteA", "x", 8.0, (18.0, 28.0), 0.0, CELL_TOP),
        ("LongA_SiteA", "x", 8.0, (12.0, 18.0), SKY, CELL_TOP),
        ("Alley_SiteB", "x", 8.0, (-27.0, -23.0), 0.0, 4.0),
        ("Courtyard_SiteA", "y", 12.0, (8.0, 20.0), STACK, CELL_TOP),
        ("Courtyard_CTSpawn", "x", 20.0, (-FENCE, FENCE), 0.0, GATE_HEIGHT),
        ("Courtyard_CTSpawn", "x", 20.0, (-12.0, 12.0), GATE_HEIGHT, CELL_TOP),
        ("CTSpawn_SiteA", "y", 12.0, (20.0, 32.0), 0.0, CELL_TOP),
        ("CTSpawn_SiteB", "y", -12.0, (23.5, 28.5), 0.0, 3.5),
    ]
    for name, axis, at, (a, c), z0, z1 in portals:
        if axis == "x":
            corners = [(at, a, z0), (at, c, z0), (at, c, z1), (at, a, z1)]
        else:
            corners = [(a, at, z0), (c, at, z0), (c, at, z1), (a, at, z1)]
        mesh = bpy.data.meshes.new("PORTAL_" + name)
        mesh.from_pydata([tuple(to_blender(*p)) for p in corners], [], [(0, 1, 2, 3)])
        mesh.update()
        obj = leon_art.add_object("PORTAL_" + name, mesh, b.collections["Cells"])
        obj.display_type = "WIRE"


# The waypoint graph: the routes linked by hand both ways (the import adds the links a player can walk,
# bAutoLinkWaypoints). The lookouts are the sites' watch spots, three a site off the lanes' line; the ladder's foot and
# top (on the container stack at A) are linked across the climb by the import (both flagged Ladder).
WAYPOINTS = {
    "TSpawn": (-26.0, 0.0), "TMid": (-20.0, 0.0), "ApronA": (-15.0, 11.0), "ApronB": (-15.0, -12.0), "AlleyCorner": (-13.5, -23.0),
    "LongAEntry": (-12.5, 21.0), "LongA": (-4.0, 20.5), "LongAEnd": (2.0, 25.8),
    "Chicane": (7.0, 25.8), "ChicaneSouth": (7.0, 19.8),
    "MidSouth": (-15.0, 0.0), "Mid": (-4.0, 0.0), "MidNorth": (4.0, 0.0),
    "ConnectorMid": (-2.5, -7.5), "ConnectorAlley": (-2.5, -20.5),
    "AlleyEntry": (-14.0, -25.0), "Alley": (-2.5, -25.0), "AlleyEnd": (5.0, -25.0),
    "Courtyard": (10.0, 0.0), "CourtyardA": (11.0, 6.0), "CourtyardB": (11.0, -6.0),
    "CTMidA": (17.0, 5.0), "CTMidB": (17.0, -5.0), "CourtyardEast": (17.5, 0.0), "CTGate": (22.0, 0.0),
    "CTSpawn": (26.0, 0.0), "CTA": (25.0, 9.0), "CTB": (25.0, -9.0),
    "ALong": (11.5, 19.8), "AMid": (19.0, 21.5), "SiteA": (22.0, 21.0),
    "AEast": (26.5, 19.0), "ACT": (29.0, 13.2),
    "BDoorSouth": (10.5, -25.0), "BEntry": (10.5, -19.0), "BMid": (15.0, -18.5), "SiteB": (22.5, -20.5), "BDoorCT": (26.25, -14.5),
    "LookoutA1": (21.0, 24.2), "LookoutA2": (26.0, 15.6), "LookoutA3": (19.0, 15.6),
    "LookoutB1": (28.5, -16.5), "LookoutB2": (26.5, -25.0), "LookoutB3": (20.0, -14.0),
    "LadderAFoot": (25.0, 21.2),
}
# On the container stack at A, up the ladder: (x, y, the stack's top).
ROOF_WAYPOINTS = {"LadderATop": (25.0, 22.8, 2.0 * CONTAINER[2]), "StackA": (23.0, 23.4, 2.0 * CONTAINER[2])}
WAYPOINT_FLAGS = {
    "LookoutA1": "Lookout", "LookoutA2": "Lookout", "LookoutA3": "Lookout",
    "LookoutB1": "Lookout", "LookoutB2": "Lookout", "LookoutB3": "Lookout",
    "LadderAFoot": "Ladder", "LadderATop": "Ladder",
}
LINKS = [
    ("TSpawn", "TMid"), ("TSpawn", "ApronA"), ("TSpawn", "ApronB"),
    ("TMid", "MidSouth"), ("MidSouth", "Mid"), ("Mid", "MidNorth"), ("MidNorth", "Courtyard"),
    ("Mid", "ConnectorMid"), ("ConnectorMid", "ConnectorAlley"), ("ConnectorAlley", "Alley"),
    ("ApronA", "LongAEntry"), ("LongAEntry", "LongA"), ("LongA", "LongAEnd"), ("LongAEnd", "Chicane"),
    ("Chicane", "ChicaneSouth"), ("ChicaneSouth", "ALong"), ("ALong", "AMid"), ("AMid", "SiteA"),
    ("ALong", "LookoutA3"), ("LookoutA3", "SiteA"), ("LookoutA1", "AMid"),
    ("ApronB", "AlleyEntry"), ("AlleyEntry", "Alley"), ("TSpawn", "AlleyCorner"), ("AlleyCorner", "Alley"), ("Alley", "AlleyEnd"), ("AlleyEnd", "BDoorSouth"),
    ("BDoorSouth", "BEntry"), ("BEntry", "BMid"), ("BMid", "SiteB"),
    ("Courtyard", "CourtyardA"), ("Courtyard", "CourtyardB"), ("CourtyardA", "CTMidA"), ("CourtyardB", "CTMidB"),
    ("CTMidA", "CourtyardEast"), ("CTMidB", "CourtyardEast"), ("CourtyardEast", "CTGate"), ("CTGate", "CTSpawn"),
    ("CTA", "CTSpawn"), ("CTB", "CTSpawn"),
    ("CTA", "ACT"), ("ACT", "AEast"), ("AEast", "SiteA"), ("LookoutA2", "AEast"),
    ("LadderAFoot", "SiteA"), ("LadderAFoot", "AEast"),
    ("BDoorCT", "CTB"), ("BDoorCT", "SiteB"),
    ("LookoutB1", "SiteB"), ("LookoutB2", "LookoutB1"),
    ("LookoutB3", "BDoorCT"), ("LookoutB3", "SiteB"),
]
ROOF_LINKS = [("LadderATop", "StackA")]
SITES = {"A": ((15.0, 15.0), (29.0, 27.0)), "B": ((15.0, -26.0), (30.0, -15.0))}
# The CT starts in the CT yard, A's side and B's side in turn (the bots' sites: A for the even, B for the odd of the
# team), out of mid's line (the courtyard's stack and the blocks hide them from the T yard).
CT_STARTS = [(27.0, 7.0), (27.0, -7.0), (29.0, 9.0), (29.0, -9.0), (24.5, 4.5)]
T_STARTS = [(-28.5, y) for y in (-4.0, -2.0, 0.0, 2.0, 4.0)]


def distance_to_box(px, py, box):
    x0, x1, y0, y1 = box[:4]
    dx = max(x0 - px, 0.0, px - x1)
    dy = max(y0 - py, 0.0, py - y1)
    return math.hypot(dx, dy)


def check_navigation(b):
    """Nothing at walking height stands inside something else, the hand links keep the capsule clear of everything at
    walking height, and the waypoints, the starts and the sites' middles (the bots' goals) stand in the open; the
    graph links every waypoint to every other."""
    for i, first in enumerate(b.obstacles):
        for second in b.obstacles[i + 1:]:
            overlap_x = min(first[1], second[1]) - max(first[0], second[0])
            overlap_y = min(first[3], second[3]) - max(first[2], second[2])
            overlap_z = min(first[6], second[6]) - max(first[5], second[5])
            if overlap_x > 0.01 and overlap_y > 0.01 and overlap_z > 0.01:
                raise ValueError("%s and %s overlap" % (first[4], second[4]))
    points = dict(("waypoint " + k, v) for k, v in WAYPOINTS.items())
    points.update(("CT start %d" % i, p) for i, p in enumerate(CT_STARTS))
    points.update(("T start %d" % i, p) for i, p in enumerate(T_STARTS))
    points.update(("site %s" % k, ((lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5)) for k, (lo, hi) in SITES.items())
    for label, (x, y) in points.items():
        for box in b.obstacles:
            if distance_to_box(x, y, box) < AGENT_CLEARANCE:
                raise ValueError("%s at (%.1f, %.1f) is inside or against %s" % (label, x, y, box[4]))
    for a, c in LINKS:
        (ax, ay), (cx, cy) = WAYPOINTS[a], WAYPOINTS[c]
        steps = int(math.hypot(cx - ax, cy - ay) / 0.1) + 1
        for step in range(steps + 1):
            t = step / steps
            x, y = ax + (cx - ax) * t, ay + (cy - ay) * t
            for box in b.obstacles:
                if distance_to_box(x, y, box) < AGENT_CLEARANCE:
                    raise ValueError("the link %s - %s runs into %s" % (a, c, box[4]))
    # Connected (the ladder's foot reaches its top through the import's climb link).
    neighbours = {name: set() for name in list(WAYPOINTS) + list(ROOF_WAYPOINTS)}
    for a, c in LINKS + ROOF_LINKS + [("LadderAFoot", "LadderATop")]:
        neighbours[a].add(c)
        neighbours[c].add(a)
    seen = {"TSpawn"}
    todo = ["TSpawn"]
    while todo:
        for other in sorted(neighbours[todo.pop()]):
            if other not in seen:
                seen.add(other)
                todo.append(other)
    missing = sorted(set(neighbours) - seen)
    if missing:
        raise ValueError("waypoints not linked to the rest: %s" % ", ".join(missing))


def build_gameplay(b):
    for site, ((x0, y0), (x1, y1)) in SITES.items():
        b.volume("Gameplay", "BombSite_" + site, (x0, y0, 0.0), (x1, y1, 3.0))
    b.volume("Gameplay", "BuyZone_CT", (22.0, -11.0, 0.0), (31.5, 11.0, 3.0))
    b.volume("Gameplay", "BuyZone_T", (-31.5, -8.0, 0.0), (-24.0, 8.0, 3.0))

    # Five starts a team, 2 m apart or more: the CTs face south, the Ts north.
    for index, ((ctx, cty), (tx, ty)) in enumerate(zip(CT_STARTS, T_STARTS)):
        suffix = "" if index == 0 else ".%03d" % index
        b.empty("Gameplay", "PlayerStart_CT" + suffix, (ctx, cty, START_HEIGHT), "ARROWS", yaw=180.0)
        b.empty("Gameplay", "PlayerStart_T" + suffix, (tx, ty, START_HEIGHT), "ARROWS", yaw=0.0)

    places = dict((name, (x, y, 0.0)) for name, (x, y) in WAYPOINTS.items())
    places.update(ROOF_WAYPOINTS)
    neighbours = {name: [] for name in places}
    for a, c in LINKS + ROOF_LINKS:
        neighbours[a].append(c)
        neighbours[c].append(a)
    for name, (x, y, height) in places.items():
        obj = b.empty("Navigation", "NavWaypoint_" + name, (x, y, height + 0.5), "SPHERE", (0.3, 0.3, 0.3))
        obj["links"] = ",".join("NavWaypoint_" + other for other in sorted(neighbours[name]))
        if name in WAYPOINT_FLAGS:
            obj["flags"] = WAYPOINT_FLAGS[name]


def build_sun(b):
    """The sun: low and warm, from the south-west over the warehouses (engine direction); it casts the shadows. The
    sky (the world settings' LightmassSettings, a cool coastal blue) fills the shade."""
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 1.0
    sun_data.color = (1.0, 0.88, 0.7)
    sun = leon_art.add_object("Sun", sun_data, b.collections["Lights"])
    direction = to_blender(*SUN_DIRECTION).normalized()
    sun.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    sun.location = to_blender(0.0, 0.0, 30.0)


# The map's world settings (the WorldSettings node's extras, Docs/LEVELS.md): the coast sky (Sky/make_sky.py's coast
# preset, imported as /Game/Sky/T_Sky_Coast), a hazier fog than de_leon's (from 25 m to 90 m, into the sky's horizon)
# and a cooler sky light for the bake's shade.
WORLD_SETTINGS = {
    "SkySettings.SkyCubemap": "/Game/Sky/T_Sky_Coast.T_Sky_Coast",
    "FogSettings.bEnableFog": "True",
    "FogSettings.StartDistance": "2500",
    "FogSettings.EndDistance": "9000",
    "LightmassSettings.EnvironmentColor": "(R=0.72,G=0.82,B=1.0,A=1.0)",
    "LightmassSettings.EnvironmentIntensity": "0.4",
}


def build_world_settings(b):
    """An empty named WorldSettings whose custom properties set the map's AWorldSettings, by property path."""
    obj = b.empty("Gameplay", "WorldSettings", (0.0, 0.0, 10.0), "PLAIN_AXES")
    for key, value in WORLD_SETTINGS.items():
        obj[key] = value


def build():
    scene = leon_art.reset_scene()
    builder = MapBuilder(scene, make_materials())
    build_geometry(builder)
    build_cells(builder)
    build_gameplay(builder)
    build_sun(builder)
    build_world_settings(builder)
    check_navigation(builder)
    total = 0
    for cell in CELLS + [OUTSIDE]:
        count = builder.cell_triangles[cell]
        total += count
        print("make_de_harbor: cell %-9s %5d triangles" % (cell, count))
        if count > CELL_BUDGET:
            raise ValueError("the cell %s has %d triangles, over its budget of %d" % (cell, count, CELL_BUDGET))
    print("make_de_harbor: %d triangles in %d pieces" % (total, builder.pieces))


def export(out):
    leon_art.save_blend(os.path.join(out, "de_harbor.blend"))
    leon_art.export_glb(os.path.join(out, "de_harbor.glb"), extras=True, lights=True, lighting_mode="RAW")


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    build()
    export(out)
    print("make_de_harbor: wrote de_harbor.blend and de_harbor.glb in %s" % out)
