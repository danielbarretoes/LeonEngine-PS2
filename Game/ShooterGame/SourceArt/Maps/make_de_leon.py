"""Builds de_leon, ShooterGame's bomb defusal map, in Blender and exports it for LeonEd's map importer.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Maps/make_de_leon.py [-- --out <folder>]

Writes de_leon.blend (to look at and try things; the script is the source, plan decision D6) and de_leon.glb (glTF 2.0
binary, +Y up) next to this script; then import the map (Game/ShooterGame/SourceArt/ImportList.ini):

    LeonCook Game/ShooterGame/ShooterGame.lproj -run=ImportAssets -importlist=Game/ShooterGame/SourceArt/ImportList.ini

A desert town in the style of Counter-Strike 1.6's de_dust (ps2-shipping N28, Docs/LEVELS.md has the plan of it):
sandstone walls and houses, wooden crates and doors, an arch into mid, a gate on A long, a tunnel on B long, the mid
doors, two bomb sites and the teams' spawns. Low-poly and textured: every texture is painted texel by texel here
(paletted: 16 colours for P4, up to 256 for P8; 64 or 128 texels, 64 texels a metre), the lighting is baked by LeonCook
into the vertices (N22, decision D5) from the sun, the sky and the point lights exported here, so the large faces are
cut on a 3 m grid (and the walls at 1.2 m, for the occlusion at their foot) to give the bake vertices.

The layout is written in the engine's axes, metres (X north, Y east, Z up) and placed in Blender's (X, -Y, Z): Blender is
right-handed with the same X and Z, so the engine's +Y is Blender's -Y (Docs/LEVELS.md; the importer turns metres into
centimetres). Node names follow the map importer's rules (Docs/LEVELS.md, Game/ShooterGame/Config/DefaultEditor.ini):

    <Cell>_<Piece>            a static mesh actor: one mesh of its own (SM_<Cell>_<Piece>), its triangles in the
                              world's axes around its centre, with its UCX_ box; crates share SM_Crate / SM_CrateBig
    UCX_<Node>_01             the box collision of that mesh node (one box a mesh: Leon merges a mesh's boxes into one)
    PlayerStart_CT.001 ...    APlayerStart, PlayerStartTag CT (empties at the capsule's centre, 0.92 m up, UE's start)
    BombSite_A / _B           ATriggerVolume [BombSite, A|B] (cube empties: 2 m x their scale)
    BuyZone_CT / _T           ATriggerVolume [BuyZone, CT|T]
    Ladder_A / _B             ATriggerVolume [Ladder]: a 20 cm box against its wall, floor to roof (N30c)
    Clip_*                    ABlockingVolume (an invisible wall)
    NavWaypoint_*             ANavigationWaypoint; the custom property "links" names the waypoints it leads to, "flags"
                              what the bots make of it: Lookout (a spot they watch a site from), Ladder (a ladder's
                              foot and top: the import links them across the climb)
    Sun, Light_<Place>_NN     the directional light and the point lights (KHR_lights_punctual, RAW intensities)
    WorldSettings             an empty whose custom properties set the map's AWorldSettings (the sky, the fog)
    VIS_<Cell>, PORTAL_<A>_<B>  the cells (box meshes: AVisibilityCellVolume) and the openings between them (quad
                              meshes: AVisibilityPortal), N15's rules (Engine/Config/BaseEditor.ini)

Every cell stays within ART_PIPELINE.md's 1 500 triangles (the script fails otherwise and prints the counts). Only
Blender's own modules are used, through the shared leon_art (Docs/ART_PIPELINE.md); no external art
(Game/ShooterGame/SourceArt/LICENSES.md). `-- --out <folder>` writes there instead.
"""

import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir))

import leon_art  # noqa: E402 (after the path)

HERE = os.path.dirname(os.path.abspath(__file__))

# Heights (metres): the floor's top at Z = 0 (the character's floor plane); crates 1.1 m (a CS jump clears them, the
# character jumps 1.14 m), the capsule 1.83 m, a player start's capsule centre 0.92 m up (UE; the game lowers the pawn to
# its feet).
CRATE = 1.1
CRATE_BIG = 1.6
START_HEIGHT = 0.92
# The bake's vertices: the faces are cut on a world grid of STEP metres and the walls again FOOT above the floor, so the
# occlusion at a wall's foot and the shadows on the ground have vertices (N22: 3 m keeps the EE's frame).
STEP = 3.0
FOOT = 1.2
MIN_SEGMENT = 0.5
# ART_PIPELINE.md: a cell's triangles.
CELL_BUDGET = 1500
# The top of the cells' volumes (above every roof a player can reach, and the eyes on it).
CELL_TOP = 6.0
# The lowest top of a wall that stands on a boundary between two cells: the sky's portals start there.
SKY = 3.5
# The capsule's radius and a margin: what the waypoint links must keep clear of.
AGENT_CLEARANCE = 0.45
# Where the sun's light travels (engine axes, not normalized): high, from the north-west. The sky's generator
# (Sky/make_sky.py) reads this line to put its sun where the bake's is.
SUN_DIRECTION = (-0.45, 0.30, -0.84)

CELLS = ["TSpawn", "Mid", "LongA", "LongB", "CTSpawn", "SiteA", "SiteB"]

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


def paint_sandstone(x, y):
    """128 x 128 (2 m): ashlar blocks 1 m x 0.5 m in staggered courses, mortar joints, a bevel of light on top."""
    course = y // 32
    yy = y % 32
    shifted = (x + (32 if course % 2 else 0)) % 128
    column = shifted // 64
    xx = shifted % 64
    tints = [(214, 186, 140), (205, 176, 130), (222, 197, 153), (198, 169, 124)]
    tint = tints[int(leon_art.noise(column, course, 11) * 4.0)]
    if yy < 2 or xx < 2:
        return shade((150, 128, 96), 0.92 if leon_art.noise(x, y, 4) > 0.5 else 1.0)
    k = 1.0 + 0.12 * (value_noise(x, y, 8, 128, 3) - 0.5) + 0.06 * (leon_art.noise(x, y, 5) - 0.5)
    if yy >= 30:
        k += 0.08
    elif yy == 2:
        k -= 0.08
    if xx == 2:
        k += 0.04
    if leon_art.noise(x, y, 9) > 0.97:
        k -= 0.12
    return shade(tint, max(0.72, min(1.12, k)))


def paint_trim(x, y):
    """64 x 64 (1 m): the darker stone of the caps, lintels and arches: a lit top edge, a shadowed bottom, joints."""
    tint = (170, 140, 102)
    if y < 4 or x % 32 < 2:
        return shade(tint, 0.78)
    if y >= 60:
        return shade(tint, 1.1)
    if leon_art.noise(x, y, 17) > 0.95:
        return shade(tint, 0.86)
    return shade(tint, 1.0 if value_noise(x, y, 8, 64, 6) > 0.5 else 0.94)


def paint_sand(x, y):
    """128 x 128 (2 m): packed sand, drifts of a redder sand, pebbles."""
    tint = (206, 178, 128) if value_noise(x, y, 32, 128, 5) < 0.62 else (200, 166, 116)
    if leon_art.noise(x, y, 21) > 0.988:
        return shade(tint, 0.8)
    if leon_art.noise(x - 1 if x > 0 else 127, y, 21) > 0.988:
        return shade(tint, 1.08)
    k = 1.0 + 0.12 * (value_noise(x, y, 16, 128, 1) - 0.5) + 0.08 * (value_noise(x, y, 4, 128, 2) - 0.5)
    k += 0.05 * (leon_art.noise(x, y, 3) - 0.5)
    return shade(tint, max(0.88, min(1.12, k)))


def paint_paving(x, y):
    """128 x 128 (2 m): 0.5 m stone slabs in staggered rows, grout, a bevel."""
    row = y // 32
    yy = y % 32
    shifted = (x + (16 if row % 2 else 0)) % 128
    column = shifted // 32
    xx = shifted % 32
    if yy < 2 or xx < 2:
        return shade((140, 128, 106), 1.0)
    tints = [(190, 176, 150), (180, 165, 138), (198, 185, 160)]
    tint = tints[int(leon_art.noise(column, row, 13) * 3.0)]
    k = 1.0 + 0.08 * (value_noise(x, y, 8, 128, 7) - 0.5) + 0.05 * (leon_art.noise(x, y, 8) - 0.5)
    if yy >= 30 or xx == 2:
        k += 0.06
    elif yy == 2 or xx == 31:
        k -= 0.08
    return shade(tint, max(0.92, min(1.08, k)))


def paint_wood(x, y):
    """64 x 128 (1 m x 2 m): vertical planks with dark gaps, grain and nails (doors, the ladders, the tunnel's roof)."""
    plank = x // 16
    px = x % 16
    if px == 0:
        return (70, 46, 26)
    if px in (3, 12) and y in (8, 9, 118, 119):
        return (58, 56, 54)
    k = (0.92, 1.0, 1.06, 0.96)[plank % 4]
    if leon_art.noise(px * 3 + plank, y // 5, 7) > 0.84:
        k *= 0.85
    return shade((132, 92, 54), k, 0.01)


def paint_crate(x, y):
    """64 x 64 (a face): CS's crate, a dark frame, three light planks and a diagonal brace, nails at the corners."""
    frame = (112, 78, 40)
    if (x in (2, 3, 60, 61)) and (y in (2, 3, 60, 61)):
        return (72, 70, 66)
    if x < 6 or x >= 58 or y < 6 or y >= 58:
        return shade(frame, 0.86 if (x in (0, 63) or y in (0, 63)) else 1.0)
    if abs(x - y) < 4:
        return shade(frame, 1.08 if x - y == -3 else 1.0)
    inner = y - 6
    if inner % 17 == 16:
        return (86, 58, 30)
    k = (0.94, 1.0, 1.05)[inner // 17 % 3]
    if leon_art.noise(x // 2, y, 23) > 0.9:
        k *= 0.9
    return shade((178, 132, 76), k, 0.01)


GLYPHS = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
}


def paint_signs(x, y):
    """128 x 64: two plastered panels, the letters A and B painted on them (a bomb site's sign)."""
    letter = "A" if x < 64 else "B"
    lx = x % 64
    if lx < 3 or lx >= 61 or y < 3 or y >= 61:
        return (150, 130, 100)
    # The glyph: 5 x 7 cells of 8 texels, its top row at the image's top.
    gx, gy = (lx - 12) // 8, (58 - y) // 8
    if 0 <= gx < 5 and 0 <= gy < 7 and 12 <= lx < 52 and y <= 58 and GLYPHS[letter][gy][gx] == "#":
        return (156, 40, 28) if leon_art.noise(x, y, 29) < 0.9 else (132, 34, 24)
    return (214, 204, 178) if leon_art.noise(x, y, 31) < 0.8 else (204, 194, 168)


# name: (painter, width, height, colour limit, tile in metres (u, v); None: each face maps the whole image)
TEXTURES = {
    "Sandstone": (paint_sandstone, 128, 128, 256, (2.0, 2.0)),
    "Trim": (paint_trim, 64, 64, 16, (1.0, 1.0)),
    "Sand": (paint_sand, 128, 128, 16, (2.0, 2.0)),
    "Paving": (paint_paving, 128, 128, 16, (2.0, 2.0)),
    "Wood": (paint_wood, 64, 128, 16, (1.0, 2.0)),
    "Crate": (paint_crate, 64, 64, 16, None),
    "Signs": (paint_signs, 128, 64, 16, None),
}
# Plain colours (linear RGB): the tunnel's lamps.
PLAIN = {"Lamp": (1.0, 0.82, 0.45)}
# What each material is made of (its physical material, Counter-Strike's texture types; ps2-shipping N30f): the
# sandstone, its trim and the signs concrete, the sand dirt, the paving tile, the planks and the crates wood, the lamps
# metal.
SURFACES = {"Sandstone": "Concrete", "Trim": "Concrete", "Sand": "Dirt", "Paving": "Tile", "Wood": "Wood",
            "Crate": "Wood", "Signs": "Concrete", "Lamp": "Metal"}


def make_materials():
    materials = {}
    for name, (painter, width, height, limit, _tile) in TEXTURES.items():
        image = leon_art.paint_image(name + "_D", width, height, painter)
        colours = leon_art.count_colours(image)
        assert colours <= limit, "%s has %d colours, over %d" % (name, colours, limit)
        print("make_de_leon: texture %-9s %3d x %3d, %3d colours" % (name, width, height, colours))
        materials[name] = leon_art.make_material(name, (1.0, 1.0, 1.0), 0.9, image, SURFACES[name])
    for name, rgb in PLAIN.items():
        materials[name] = leon_art.make_material(name, rgb, 0.6, surface=SURFACES[name])
    return materials


# Geometry: pieces made of axis-aligned boxes whose faces are cut for the bake.

def grid_breaks(lo, hi, step=STEP):
    """lo, the multiples of step between lo and hi (none closer than MIN_SEGMENT to either end), hi: every face's cuts
    fall on one world grid, so the pieces' vertices meet."""
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


# Each face of a box: (axis, side), its outward normal, its u axis (the viewer's right) and its v axis (up); the engine
# is left-handed (X forward, Y right, Z up), so facing -n, the right is Z x -n.
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
        tile = TEXTURES[material][4] if material in TEXTURES else (1.0, 1.0)
        return (_dot(point, right) / tile[0], _dot(point, up) / tile[1])

    def box(self, lo, hi, materials, skip=(), whole=None, cut=True):
        """An axis-aligned box from lo to hi (engine metres). materials: one name, or a dict by face ('+x' ... '-z',
        'side' for the four vertical faces, 'top', 'bottom'). skip leaves faces out (the hidden ones). cut: the faces
        on the bake's grid; whole: map each face onto a region (x0, y0, x1, y1) of its image (0..1) instead of the
        world tile."""
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
    """A plain box mesh (a UCX_ collision node's), in Blender's axes."""
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
        for name in ["Cell_" + cell for cell in CELLS] + ["Collision", "Gameplay", "Navigation", "Cells", "Lights"]:
            collection = bpy.data.collections.new(name)
            scene.collection.children.link(collection)
            self.collections[name] = collection
        self.cell_triangles = {cell: 0 for cell in CELLS}
        self.pieces = 0
        # Footprints of what stands at walking height (x0, x1, y0, y1), for the waypoint checks.
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
            self.obstacles.append((collision_lo[0], collision_hi[0], collision_lo[1], collision_hi[1], piece.name))
        return obj

    def solid(self, cell, name, lo, hi, skip=(), materials=None):
        """A block of sandstone with a trim cap (a wall, a house, a pillar), its bottom on the floor left out."""
        piece = Piece("%s_%s" % (cell, name), cell)
        if materials is None:
            materials = {"side": "Sandstone", "top": "Trim", "bottom": "Trim"}
        hidden = set(skip)
        if lo[2] <= 0.0:
            hidden.add("-z")
        piece.box(lo, hi, materials, skip=hidden)
        return self.add_piece(piece)

    def floor(self, cell, name, x0, x1, y0, y1, material):
        """A ground slab (0.2 m, its top at Z = 0) cut on the grid; its underside closes it for the bake's rays. Its UCX_
        box is the ground players walk on, with the slab's material (the sand's dirt, the paving's tile: footsteps and
        impacts, N30f); a capsule walks across the seam between two slabs, whose tops differ by a hair in float, since
        the engine takes a box whose top is at the feet for floor (N29)."""
        piece = Piece("%s_%s" % (cell, name), cell)
        piece.box((x0, y0, -0.2), (x1, y1, 0.0), material, skip=("+x", "-x", "+y", "-y", "-z"))
        # The underside is never seen: one quad, its UVs over one repeat (a batch spans at most 14).
        piece.box((x0, y0, -0.2), (x1, y1, 0.0), material, skip=("+x", "-x", "+y", "-y", "+z"),
                  whole=(0.0, 0.0, 1.0, 1.0), cut=False)
        return self.add_piece(piece, collision=((x0, y0, -0.2), (x1, y1, 0.0)), obstacle=False)

    def shared_mesh(self, name, size, material, whole):
        """A prop's mesh shared by its instances (a crate): its origin on the floor at its middle."""
        if name not in self.shared:
            piece = Piece(name, "Mid")
            piece.box((-size * 0.5, -size * 0.5, 0.0), (size * 0.5, size * 0.5, size), material, skip=("-z",),
                      whole=whole, cut=False)
            self.shared[name] = (piece.build_mesh(name, self.materials, (0.0, 0.0, 0.0)), piece.triangles(), size)
        return self.shared[name]

    def prop(self, cell, kind, index, x, y, z=0.0, big=False):
        """A crate (1.1 m) or a big crate (1.6 m) standing at (x, y, z); the first instance of a mesh carries its UCX_."""
        mesh_name = "CrateBig" if big else "Crate"
        mesh, triangles, size = self.shared_mesh(mesh_name, CRATE_BIG if big else CRATE, "Crate", (0.0, 0.0, 1.0, 1.0))
        name = "%s_%s_%02d" % (cell, kind, index)
        obj = leon_art.add_object(name, mesh, self.collections["Cell_" + cell])
        obj.location = to_blender(x, y, z)
        if not self.shared.get(mesh_name + ":collision"):
            self.shared[mesh_name + ":collision"] = True
            self.add_collision(name, (x - size * 0.5, y - size * 0.5, z), (x + size * 0.5, y + size * 0.5, z + size))
        self.cell_triangles[cell] += triangles
        self.pieces += 1
        if z < 1.9:
            self.obstacles.append((x - size * 0.5, x + size * 0.5, y - size * 0.5, y + size * 0.5, name))
        return obj

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

    def point_light(self, name, position, energy, radius, colour=(1.0, 0.8, 0.5)):
        data = bpy.data.lights.new(name, "POINT")
        data.energy = energy
        data.color = colour
        data.use_custom_distance = True
        data.cutoff_distance = radius
        data.shadow_soft_size = 0.0
        light = leon_art.add_object(name, data, self.collections["Lights"])
        light.location = to_blender(*position)
        return light


def build_textured_quad(builder, cell, name, lo, hi, face, material, region, collision):
    """A plaque (a sign) on a wall: a thin box, its image region on its `face`, trim on its edges, its back against
    the wall left out (the thickness also keeps every axis of the mesh's LPS2 quantization non-degenerate); its UCX_
    inside the wall."""
    piece = Piece("%s_%s" % (cell, name), cell)
    piece.box(lo, hi, material, skip=tuple(f for f in FACES if f != face), whole=region, cut=False)
    back = face.replace("+", "#").replace("-", "+").replace("#", "-")
    piece.box(lo, hi, "Trim", skip=(face, back), cut=False)
    return builder.add_piece(piece, collision=collision, obstacle=False)


def build_ladder(builder, cell, name, x, wall_y, facing, height):
    """A wooden ladder on the wall face at Y = wall_y whose climbers stand on the `facing` side (+1: +Y): two rails and
    rungs every 35 cm (their backs against the wall left out), the Ladder trigger volume (a 20 cm box from the floor
    to the roof, N30c) and a UCX_ box inside the wall, so the rungs do not stop the climber."""
    piece = Piece("%s_%s" % (cell, name), cell)
    back = "-y" if facing > 0 else "+y"

    def span(depth0, depth1):
        a, b = wall_y + facing * depth0, wall_y + facing * depth1
        return min(a, b), max(a, b)

    y0, y1 = span(0.0, 0.08)
    for rail_x in (x - 0.42, x + 0.42):
        piece.box((rail_x - 0.04, y0, 0.0), (rail_x + 0.04, y1, height), "Wood", skip=(back, "-z"), cut=False)
    y0, y1 = span(0.01, 0.06)
    rung = 0.35
    while rung < height - 0.1:
        piece.box((x - 0.38, y0, rung - 0.025), (x + 0.38, y1, rung + 0.025), "Wood", skip=(back, "+x", "-x"),
                  cut=False)
        rung += 0.35
    inside0, inside1 = span(-0.15, -0.05)
    builder.add_piece(piece, collision=((x - 0.5, inside0, 0.0), (x + 0.5, inside1, height)), obstacle=False)
    v0, v1 = span(0.0, 0.2)
    builder.volume("Gameplay", "Ladder_%s" % name[-1], (x - 0.5, v0, 0.0), (x + 0.5, v1, height))


def build_geometry(b):
    """The map (the plan in Docs/LEVELS.md): walls 0.5 m outside the 60 x 48 m playing field, T south, CT north, A east,
    B west."""
    sand, paving = "Sand", "Paving"

    # --- T spawn: the plaza south of X = -14, two houses at its corners.
    b.solid("TSpawn", "Wall_South", (-30.5, -24.5, 0.0), (-30.0, 24.5, 5.0), skip=("-x", "+y", "-y"))
    b.solid("TSpawn", "Wall_West", (-30.0, -24.5, 0.0), (-14.0, -24.0, 5.0), skip=("-y", "+x", "-x"))
    b.solid("TSpawn", "Wall_East", (-30.0, 24.0, 0.0), (-14.0, 24.5, 5.0), skip=("+y", "+x", "-x"))
    b.solid("TSpawn", "House_West", (-30.0, -24.0, 0.0), (-25.0, -18.0, 4.5), skip=("-x", "-y"))
    b.solid("TSpawn", "House_East", (-30.0, 18.0, 0.0), (-25.0, 24.0, 4.5), skip=("-x", "+y"))
    b.floor("TSpawn", "Floor_01", -30.0, -14.0, -18.0, 18.0, sand)
    b.floor("TSpawn", "Floor_02", -25.0, -14.0, 18.0, 24.0, sand)
    b.floor("TSpawn", "Floor_03", -25.0, -14.0, -24.0, -18.0, sand)
    b.prop("TSpawn", "Crate", 1, -21.5, 11.0)
    b.prop("TSpawn", "Crate", 2, -21.5, -11.0)
    b.prop("TSpawn", "Crate", 3, -21.5, -12.1)
    b.prop("TSpawn", "CrateBig", 1, -28.0, 14.0, big=True)

    # --- Mid: X -14 .. 9 between the blocks, an arch at its south end.
    b.solid("Mid", "Arch_West", (-14.5, -6.0, 0.0), (-13.5, -4.5, 4.0), skip=("-y",))
    b.solid("Mid", "Arch_East", (-14.5, 4.5, 0.0), (-13.5, 6.0, 4.0), skip=("+y",))
    b.solid("Mid", "Arch_Top", (-14.5, -4.5, 3.2), (-13.5, 4.5, 4.0), materials="Trim")
    b.floor("Mid", "Floor", -14.0, 9.0, -6.0, 6.0, sand)
    b.prop("Mid", "Crate", 1, 0.0, -2.5)
    b.prop("Mid", "Crate", 2, -6.0, 3.0)
    b.prop("Mid", "Crate", 3, -6.0, 4.1)

    # --- Long A and short A: the east lane (Y 12 .. 20), the gate at its south end, the blocks between it and mid.
    b.solid("LongA", "Block_South", (-14.0, 6.0, 0.0), (-2.0, 12.0, 4.0))
    b.solid("LongA", "Block_North", (2.0, 6.0, 0.0), (9.0, 12.0, 3.5))
    b.solid("LongA", "House_Outer", (-14.0, 20.0, 0.0), (9.0, 24.0, 4.5), skip=("+y",))
    b.solid("LongA", "Gate_West", (-14.5, 12.0, 0.0), (-13.5, 14.5, 4.2), skip=("-y",))
    b.solid("LongA", "Gate_East", (-14.5, 17.5, 0.0), (-13.5, 20.0, 4.2), skip=("+y",))
    b.solid("LongA", "Gate_Top", (-14.5, 14.5, 2.8), (-13.5, 17.5, 4.2), materials="Trim")
    b.floor("LongA", "Floor", -14.0, 9.0, 12.0, 20.0, sand)
    b.floor("LongA", "Floor_Short", -2.0, 2.0, 6.0, 12.0, sand)
    b.prop("LongA", "Crate", 1, -4.0, 13.0)
    b.prop("LongA", "Crate", 2, 6.5, 19.2)
    build_ladder(b, "LongA", "Ladder_A", 5.5, 12.0, 1.0, 3.5)

    # --- Long B, short B and the B tunnel (X -14 .. -6, 4 m wide, roofed at 3 m, two lamps).
    b.solid("LongB", "Block_South", (-14.0, -12.0, 0.0), (-2.0, -6.0, 4.0))
    b.solid("LongB", "Block_North", (2.0, -12.0, 0.0), (9.0, -6.0, 3.5))
    b.solid("LongB", "House_Outer", (-14.0, -24.0, 0.0), (9.0, -20.0, 4.5), skip=("-y",))
    b.solid("LongB", "Tunnel_WallNorth", (-14.0, -14.0, 0.0), (-6.0, -12.0, 4.0), skip=("+y",))
    b.solid("LongB", "Tunnel_WallSouth", (-14.0, -20.0, 0.0), (-6.0, -18.0, 4.5), skip=("-y",))
    b.solid("LongB", "Tunnel_Roof", (-14.0, -18.0, 3.0), (-6.0, -14.0, 3.5),
            materials={"side": "Trim", "top": "Trim", "bottom": "Wood"}, skip=("+y", "-y"))
    b.floor("LongB", "Floor_Tunnel", -14.0, -6.0, -18.0, -14.0, sand)
    b.floor("LongB", "Floor", -6.0, 9.0, -20.0, -12.0, sand)
    b.floor("LongB", "Floor_Short", -2.0, 2.0, -12.0, -6.0, sand)
    for index, x in enumerate((-12.0, -8.0), start=1):
        lamp = Piece("LongB_Lamp_%02d" % index, "LongB")
        lamp.box((x - 0.15, -14.3, 2.35), (x + 0.15, -14.0, 2.65), "Lamp", skip=("+y",), cut=False)
        b.add_piece(lamp, obstacle=False)
        b.point_light("Light_Tunnel_%02d" % index, (x, -14.9, 2.4), 1.6, 7.0)
    b.prop("LongB", "Crate", 1, -3.0, -19.2)
    b.prop("LongB", "Crate", 2, 6.5, -19.2)
    build_ladder(b, "LongB", "Ladder_B", 5.5, -12.0, -1.0, 3.5)

    # --- CT spawn and the mid doors (X 9 .. 10, a 3 m doorway, its two wooden wings open against the north face).
    b.solid("CTSpawn", "Wall_North", (30.0, -8.5, 0.0), (30.5, 8.5, 5.0), skip=("+x", "+y", "-y"))
    b.solid("CTSpawn", "MidDoor_West", (9.0, -6.0, 0.0), (10.0, -1.5, 3.5), skip=())
    b.solid("CTSpawn", "MidDoor_East", (9.0, 1.5, 0.0), (10.0, 6.0, 3.5), skip=())
    b.solid("CTSpawn", "MidDoor_Top", (9.0, -1.5, 2.7), (10.0, 1.5, 3.5), materials="Trim")
    for suffix, y0, y1 in (("West", -3.1, -1.6), ("East", 1.6, 3.1)):
        wing = Piece("CTSpawn_Door_%s" % suffix, "CTSpawn")
        wing.box((10.0, y0, 0.0), (10.1, y1, 2.6), "Wood", skip=("-x", "-z"), cut=False)
        b.add_piece(wing)
    b.point_light("Light_MidDoors_01", (8.2, 0.0, 3.0), 1.0, 6.0)
    b.floor("CTSpawn", "Floor", 9.0, 30.0, -8.5, 8.5, paving)
    b.prop("CTSpawn", "Crate", 1, 21.0, 5.0)
    b.prop("CTSpawn", "CrateBig", 1, 14.0, 2.5, big=True)

    # --- Site A (north-east) and site B (north-west): the site wall toward the CT spawn (open at both ends), a house in
    # the far corner, crates, the site's letter.
    for site, sign in (("A", 1.0), ("B", -1.0)):
        cell = "Site" + site

        def ys(a, c, s=sign):
            return (min(a * s, c * s), max(a * s, c * s))

        outer = "+y" if sign > 0 else "-y"
        y0, y1 = ys(8.5, 9.5)
        b.solid(cell, "SiteWall", (14.0, y0, 0.0), (22.0, y1, 3.5))
        y0, y1 = ys(8.5, 24.5)
        b.solid(cell, "Wall_North", (30.0, y0, 0.0), (30.5, y1, 5.0), skip=("+x", "+y", "-y"))
        y0, y1 = ys(24.0, 24.5)
        b.solid(cell, "Wall_Side", (9.0, y0, 0.0), (30.0, y1, 5.0), skip=("+x", "-x", outer))
        y0, y1 = ys(18.0, 24.0)
        b.solid(cell, "House", (27.0, y0, 0.0), (30.0, y1, 5.5), skip=("+x", outer))
        y0, y1 = ys(8.5, 18.0)
        b.floor(cell, "Floor_01", 9.0, 30.0, y0, y1, paving)
        y0, y1 = ys(18.0, 24.0)
        b.floor(cell, "Floor_02", 9.0, 27.0, y0, y1, paving)
        # The letter on the site wall's site face.
        sign_y0, sign_y1 = ys(9.5, 9.52)
        in_y0, in_y1 = ys(9.0, 9.4)
        region = (0.0, 0.0, 0.5, 1.0) if site == "A" else (0.5, 0.0, 1.0, 1.0)
        build_textured_quad(b, cell, "Sign", (17.4, sign_y0, 1.4), (18.6, sign_y1, 2.6), outer, "Signs", region,
                            ((17.4, in_y0, 1.4), (18.6, in_y1, 2.6)))

    b.prop("SiteA", "Crate", 1, 18.0, 15.0)
    b.prop("SiteA", "Crate", 2, 19.2, 15.0)
    b.prop("SiteA", "Crate", 3, 18.6, 15.0, CRATE)
    b.prop("SiteA", "Crate", 4, 24.0, 20.0)
    b.prop("SiteA", "CrateBig", 1, 25.5, 13.5, big=True)
    b.prop("SiteB", "Crate", 1, 16.0, -19.5)
    b.prop("SiteB", "Crate", 2, 22.0, -19.0)
    b.prop("SiteB", "Crate", 3, 22.0, -20.2)
    b.prop("SiteB", "Crate", 4, 22.0, -19.6, CRATE)
    b.prop("SiteB", "CrateBig", 1, 25.5, -13.5, big=True)
    # A low wall at B, and the player clip that keeps players from jumping over it.
    b.solid("SiteB", "LowWall", (13.75, -22.5, 0.0), (14.25, -17.5, 1.0))
    b.volume("Gameplay", "Clip_BLowWall", (13.75, -22.5, 1.0), (14.25, -17.5, 3.5))


def build_cells(b):
    """The cells and the openings between them, as N15's import reads them (Engine/Config/BaseEditor.ini; the fixture
    Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/MakeCellsFixture.py): VIS_<Cell> a box mesh over the
    walkable space and what stands on it, up to CELL_TOP; PORTAL_<CellA>_<CellB> a quad mesh in the plane between two
    cells, at the doors and the lanes' ends, and over the walls between them from SKY up (the lowest top of a wall
    that stands on a boundary: a sightline over it crosses the plane above it). Several quads between the same two cells
    are Blender's copies (PORTAL_Mid_LongA.001: the import drops the copy number)."""
    cells = {
        "TSpawn": ((-30.0, -24.0), (-14.0, 24.0)),
        "Mid": ((-14.0, -6.0), (9.0, 6.0)),
        "LongA": ((-14.0, 6.0), (9.0, 24.0)),
        "LongB": ((-14.0, -24.0), (9.0, -6.0)),
        "CTSpawn": ((9.0, -8.5), (30.0, 8.5)),
        "SiteA": ((9.0, 8.5), (30.0, 24.0)),
        "SiteB": ((9.0, -24.0), (30.0, -8.5)),
    }
    for name, ((x0, y0), (x1, y1)) in cells.items():
        obj = leon_art.add_object("VIS_" + name, box_mesh("VIS_" + name, (x0, y0, 0.0), (x1, y1, CELL_TOP)),
                                  b.collections["Cells"])
        obj.display_type = "WIRE"
    # (name, plane axis 'x' or 'y', the plane's coordinate, the span along the other axis, z0, z1)
    portals = [
        ("TSpawn_Mid", "x", -14.0, (-4.5, 4.5), 0.0, 3.2),
        ("TSpawn_Mid", "x", -14.0, (-6.0, 6.0), SKY, CELL_TOP),
        ("TSpawn_LongA", "x", -14.0, (14.5, 17.5), 0.0, 2.8),
        ("TSpawn_LongA", "x", -14.0, (6.0, 24.0), SKY, CELL_TOP),
        ("TSpawn_LongB", "x", -14.0, (-18.0, -14.0), 0.0, 3.0),
        ("TSpawn_LongB", "x", -14.0, (-24.0, -6.0), SKY, CELL_TOP),
        ("Mid_LongA", "y", 6.0, (-2.0, 2.0), 0.0, SKY),
        ("Mid_LongA", "y", 6.0, (-14.0, 9.0), SKY, CELL_TOP),
        ("Mid_LongB", "y", -6.0, (-2.0, 2.0), 0.0, SKY),
        ("Mid_LongB", "y", -6.0, (-14.0, 9.0), SKY, CELL_TOP),
        ("Mid_CTSpawn", "x", 9.0, (-1.5, 1.5), 0.0, 2.7),
        ("Mid_CTSpawn", "x", 9.0, (-6.0, 6.0), SKY, CELL_TOP),
        ("LongA_SiteA", "x", 9.0, (12.0, 20.0), 0.0, SKY),
        ("LongA_SiteA", "x", 9.0, (8.5, 24.0), SKY, CELL_TOP),
        ("LongB_SiteB", "x", 9.0, (-20.0, -12.0), 0.0, SKY),
        ("LongB_SiteB", "x", 9.0, (-24.0, -8.5), SKY, CELL_TOP),
        ("LongA_CTSpawn", "x", 9.0, (6.0, 8.5), SKY, CELL_TOP),
        ("LongB_CTSpawn", "x", 9.0, (-8.5, -6.0), SKY, CELL_TOP),
        ("SiteA_CTSpawn", "y", 8.5, (9.0, 14.0), 0.0, SKY),
        ("SiteA_CTSpawn", "y", 8.5, (22.0, 30.0), 0.0, SKY),
        ("SiteA_CTSpawn", "y", 8.5, (9.0, 30.0), SKY, CELL_TOP),
        ("SiteB_CTSpawn", "y", -8.5, (9.0, 14.0), 0.0, SKY),
        ("SiteB_CTSpawn", "y", -8.5, (22.0, 30.0), 0.0, SKY),
        ("SiteB_CTSpawn", "y", -8.5, (9.0, 30.0), SKY, CELL_TOP),
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


# The waypoint graph: the main routes, linked by hand both ways; the import adds the links a player can walk
# (bAutoLinkWaypoints in Config/DefaultEditor.ini). The lookouts are the sites' watch spots (ps2-polish P3: the bots
# watch the ways in from them), out of the long lanes' sight from the terrorists' side (behind the crates, off the
# lanes' line, in the corners by the houses); the ladders' feet and tops (on the roofs, ROOF_WAYPOINTS) are linked
# across the climb by the import (both flagged Ladder).
WAYPOINTS = {
    "TSpawn": (-24.0, 0.0), "TMid": (-17.0, 0.0), "TPlazaA": (-18.0, 16.0), "TPlazaB": (-18.0, -16.0),
    "LongAGate": (-14.0, 16.0), "TunnelB": (-10.0, -16.0),
    "Mid": (-4.0, 0.0), "ShortA": (0.0, 9.0), "ShortB": (0.0, -9.0), "LongA": (0.0, 16.0), "LongB": (0.0, -16.0),
    "ALongEnd": (7.5, 15.5), "BLongEnd": (7.5, -15.5),
    "MidDoors": (9.5, 0.0), "CTMid": (12.0, -1.0), "AConnector": (11.5, 11.0), "BConnector": (11.5, -11.0),
    "SiteA": (19.0, 18.0), "SiteB": (19.0, -17.0), "CTSpawn": (25.0, 0.0), "CTA": (26.0, 10.0),
    "CTB": (26.0, -10.0),
    "LookoutA1": (24.5, 22.5), "LookoutA2": (20.6, 15.0), "LookoutA3": (21.0, 10.1),
    "LookoutB1": (19.0, -22.5), "LookoutB2": (27.2, -13.5), "LookoutB3": (21.0, -10.1),
    "LadderAFoot": (5.5, 12.8), "LadderBFoot": (5.5, -12.8),
}
# On the roofs of the blocks north of the longs, up the ladders: (x, y, the roof's height).
ROOF_WAYPOINTS = {
    "LadderATop": (5.5, 11.2, 3.5), "RoofA": (4.0, 8.0, 3.5),
    "LadderBTop": (5.5, -11.2, 3.5), "RoofB": (4.0, -8.0, 3.5),
}
WAYPOINT_FLAGS = {
    "LookoutA1": "Lookout", "LookoutA2": "Lookout", "LookoutA3": "Lookout",
    "LookoutB1": "Lookout", "LookoutB2": "Lookout", "LookoutB3": "Lookout",
    "LadderAFoot": "Ladder", "LadderATop": "Ladder", "LadderBFoot": "Ladder", "LadderBTop": "Ladder",
}
LINKS = [
    ("TSpawn", "TMid"), ("TSpawn", "TPlazaA"), ("TSpawn", "TPlazaB"), ("TMid", "Mid"), ("TPlazaA", "LongAGate"),
    ("LongAGate", "LongA"), ("TPlazaB", "TunnelB"), ("TunnelB", "LongB"), ("Mid", "ShortA"), ("Mid", "ShortB"),
    ("Mid", "MidDoors"), ("ShortA", "LongA"), ("ShortB", "LongB"), ("LongA", "ALongEnd"), ("LongB", "BLongEnd"),
    ("ALongEnd", "AConnector"), ("BLongEnd", "BConnector"), ("MidDoors", "CTMid"), ("CTMid", "AConnector"),
    ("CTMid", "BConnector"), ("AConnector", "SiteA"), ("BConnector", "SiteB"), ("CTMid", "CTSpawn"),
    ("CTSpawn", "CTA"), ("CTSpawn", "CTB"), ("CTA", "SiteA"), ("CTB", "SiteB"),
    ("LookoutA1", "SiteA"), ("LookoutA2", "SiteA"), ("LookoutA3", "AConnector"), ("LookoutA3", "CTA"),
    ("LookoutB1", "SiteB"), ("LookoutB2", "CTB"), ("LookoutB3", "BConnector"), ("LookoutB3", "CTB"),
    ("LadderAFoot", "LongA"), ("LadderAFoot", "ALongEnd"), ("LadderBFoot", "LongB"), ("LadderBFoot", "BLongEnd"),
]
# The roofs' links (a roof's waypoints stand over the block, which check_navigation's floor plan does not see).
ROOF_LINKS = [("LadderATop", "RoofA"), ("LadderBTop", "RoofB")]
SITES = {"A": ((13.0, 12.0), (27.0, 22.0)), "B": ((13.0, -22.0), (27.0, -12.0))}
# The CT starts stand out of the line of the mid doors and the mid arch (ps2-polish P3: at Y within 4 m of the middle
# the terrorists' spawn saw them across the whole map), A's side and B's side in turn (the bots' sites: A for the even,
# B for the odd of the team).
CT_STARTS = [(26.5, 6.0), (26.5, -6.0), (28.5, 6.5), (28.5, -6.5), (24.5, 7.0)]
T_STARTS = [(-26.5, y) for y in (-4.0, -2.0, 0.0, 2.0, 4.0)]


def distance_to_box(px, py, box):
    x0, x1, y0, y1 = box[:4]
    dx = max(x0 - px, 0.0, px - x1)
    dy = max(y0 - py, 0.0, py - y1)
    return math.hypot(dx, dy)


def check_navigation(b):
    """The hand links keep the capsule clear of everything at walking height, and the waypoints, the starts and the
    sites' middles (the bots' goals) stand in the open."""
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


def build_gameplay(b):
    # The bomb sites and the buy zones (trigger volumes, 3 m high).
    for site, ((x0, y0), (x1, y1)) in SITES.items():
        b.volume("Gameplay", "BombSite_" + site, (x0, y0, 0.0), (x1, y1, 3.0))
    b.volume("Gameplay", "BuyZone_CT", (22.0, -8.0, 0.0), (29.5, 8.0, 3.0))
    b.volume("Gameplay", "BuyZone_T", (-29.0, -8.0, 0.0), (-22.0, 8.0, 3.0))

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
    """The sun: high, warm, shining down toward the south-east (engine direction), so the faces toward the CT side and
    the sites are lit; it casts the shadows. The sky (the world settings' LightmassSettings) fills the shade."""
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 1.0
    sun_data.color = (1.0, 0.95, 0.84)
    sun = leon_art.add_object("Sun", sun_data, b.collections["Lights"])
    direction = to_blender(*SUN_DIRECTION).normalized()
    sun.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    sun.location = to_blender(0.0, 0.0, 30.0)


# The map's world settings (the WorldSettings node's extras, Docs/LEVELS.md): the sky (Sky/make_sky.py's desert, imported
# as /Game/Sky/T_Sky_Desert) and the distance fog, fading into the sky's horizon from 30 m to the far plane's 100 m so
# only the far end of a long view is tinted.
WORLD_SETTINGS = {
    "SkySettings.SkyCubemap": "/Game/Sky/T_Sky_Desert.T_Sky_Desert",
    "FogSettings.bEnableFog": "True",
    "FogSettings.StartDistance": "3000",
    "FogSettings.EndDistance": "10000",
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
    for cell in CELLS:
        count = builder.cell_triangles[cell]
        total += count
        print("make_de_leon: cell %-8s %5d triangles" % (cell, count))
        if count > CELL_BUDGET:
            raise ValueError("the cell %s has %d triangles, over its budget of %d" % (cell, count, CELL_BUDGET))
    print("make_de_leon: %d triangles in %d pieces" % (total, builder.pieces))


def export(out):
    leon_art.save_blend(os.path.join(out, "de_leon.blend"))
    leon_art.export_glb(os.path.join(out, "de_leon.glb"), extras=True, lights=True, lighting_mode="RAW")


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    build()
    export(out)
    print("make_de_leon: wrote de_leon.blend and de_leon.glb in %s" % out)
