"""Builds the de_leon blockout in Blender and exports it for LeonEd's map importer.

    blender --background --factory-startup --python Game/ShooterGame/SourceArt/Maps/make_de_leon.py

Writes de_leon.blend (the source to edit by hand) and de_leon.glb (glTF 2.0 binary, +Y up) next to this script; then
import the map (Game/ShooterGame/SourceArt/ImportList.ini):

    LeonCook Game/ShooterGame/ShooterGame.lproj -run=ImportAssets -importlist=Game/ShooterGame/SourceArt/ImportList.ini

The layout is written in the engine's axes, metres (X north, Y east, Z up) and placed in Blender's (X, -Y, Z): Blender is
right-handed with the same X and Z, so the engine's +Y is Blender's -Y (Docs/LEVELS.md; the importer turns metres into
centimetres). Node names follow the map importer's rules (Docs/LEVELS.md, Game/ShooterGame/Config/DefaultEditor.ini):

    PlayerStart_CT.001 ...    APlayerStart, PlayerStartTag CT (empties at the capsule's centre, 0.92 m up, UE's start)
    BombSite_A / _B           ATriggerVolume [BombSite, A|B] (cube empties: 2 m x their scale)
    BuyZone_CT / _T           ATriggerVolume [BuyZone, CT|T]
    Clip_*                    ABlockingVolume (an invisible wall)
    UCX_CrateStack_01         the box collision of the CrateStack mesh
    NavWaypoint_*             ANavigationWaypoint; the custom property "links" names the waypoints it leads to
    Sun                       the directional light (KHR_lights_punctual, intensity as it is: RAW lighting mode)

Every mesh is a 1 m cube (shared by the objects of one material) scaled into place, so the import makes one SM_ per
material. Only Blender's own modules are used; no external art (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import math
import os

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
BLEND_PATH = os.path.join(HERE, "de_leon.blend")
GLB_PATH = os.path.join(HERE, "de_leon.glb")

# Floor at Z = 0 (the character's floor plane), walls 4 m (the edge) and 3.5 m (inside), crates 1.1 m (a CS jump
# clears them), the capsule 1.83 m. Sizes in metres.
EDGE_WALL = 4.0
WALL = 3.5
CRATE = 1.1
START_HEIGHT = 0.92  # a player start's capsule centre above the floor (UE); the game lowers the pawn to its feet


def to_blender(x, y, z):
    """Engine metres (X north, Y east, Z up) to Blender's axes."""
    return Vector((x, -y, z))


def make_material(name, rgb, roughness=0.85):
    material = bpy.data.materials.new(name)
    if material.node_tree is None:  # Blender 5 makes the node tree itself (use_nodes goes away in 6.0)
        material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = 0.0
    return material


def make_cube_mesh(name, material=None, boxes=None):
    """A mesh of boxes (default: one 1 m cube centred on the origin) with UVs; boxes are (center, size) in metres."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new()
    for center, size in boxes or [((0.0, 0.0, 0.0), (1.0, 1.0, 1.0))]:
        result = bmesh.ops.create_cube(bm, size=1.0, calc_uvs=True)
        verts = result["verts"]
        bmesh.ops.scale(bm, vec=Vector(size), verts=verts)
        bmesh.ops.translate(bm, vec=Vector(center), verts=verts)
    del uv_layer
    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    return mesh


def add_object(name, data, collection):
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    return obj


def add_box(collection, name, mesh, center, size):
    """A cube mesh object at an engine centre, scaled to an engine size (metres)."""
    obj = add_object(name, mesh, collection)
    obj.location = to_blender(*center)
    obj.scale = Vector(size)
    return obj


def add_empty(collection, name, center, display="CUBE", scale=(1.0, 1.0, 1.0), yaw=0.0):
    """An empty at an engine location; yaw in the engine's sense (from +X toward +Y), i.e. minus Blender's."""
    obj = add_object(name, None, collection)
    obj.empty_display_type = display
    obj.location = to_blender(*center)
    obj.scale = Vector(scale)
    obj.rotation_euler = (0.0, 0.0, -math.radians(yaw))
    return obj


def add_volume(collection, name, center, size):
    """A trigger or blocking volume: a cube empty, 2 m x its scale (the importer's box without a mesh)."""
    return add_empty(collection, name, center, "CUBE", (size[0] * 0.5, size[1] * 0.5, size[2] * 0.5))


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0

    geometry = bpy.data.collections.new("Geometry")
    gameplay = bpy.data.collections.new("Gameplay")
    navigation = bpy.data.collections.new("Navigation")
    for collection in (geometry, gameplay, navigation):
        scene.collection.children.link(collection)

    floor = make_cube_mesh("Floor", make_material("Floor", (0.60, 0.53, 0.40)))
    wall = make_cube_mesh("Wall", make_material("Wall", (0.78, 0.70, 0.56)))
    crate = make_cube_mesh("Crate", make_material("Crate", (0.46, 0.31, 0.16), 0.7))
    pad_a = make_cube_mesh("PadA", make_material("SiteA", (0.72, 0.28, 0.20)))
    pad_b = make_cube_mesh("PadB", make_material("SiteB", (0.25, 0.42, 0.72)))
    pad_ct = make_cube_mesh("PadCT", make_material("SpawnCT", (0.30, 0.38, 0.55)))
    pad_t = make_cube_mesh("PadT", make_material("SpawnT", (0.62, 0.45, 0.25)))

    # The ground: 60 x 48 m, its top at Z = 0.
    add_box(geometry, "Floor", floor, (0.0, 0.0, -0.1), (60.0, 48.0, 0.2))

    # The edge of the map.
    add_box(geometry, "Wall_North", wall, (30.25, 0.0, EDGE_WALL * 0.5), (0.5, 49.0, EDGE_WALL))
    add_box(geometry, "Wall_South", wall, (-30.25, 0.0, EDGE_WALL * 0.5), (0.5, 49.0, EDGE_WALL))
    add_box(geometry, "Wall_East", wall, (0.0, 24.25, EDGE_WALL * 0.5), (61.0, 0.5, EDGE_WALL))
    add_box(geometry, "Wall_West", wall, (0.0, -24.25, EDGE_WALL * 0.5), (61.0, 0.5, EDGE_WALL))

    # The lanes between the T plaza (south) and the CT plaza (north): A long (east), mid, B long (west). The blocks
    # between them leave the short connectors at X = -2 .. 2.
    for side, sign in (("East", 1.0), ("West", -1.0)):
        add_box(geometry, "Block%s_South" % side, wall, (-8.0, 9.0 * sign, WALL * 0.5), (12.0, 6.0, WALL))
        add_box(geometry, "Block%s_North" % side, wall, (5.5, 9.0 * sign, WALL * 0.5), (7.0, 6.0, WALL))
        add_box(geometry, "Block%s_Outer" % side, wall, (-2.5, 22.0 * sign, WALL * 0.5), (23.0, 4.0, WALL))
        # The site's wall toward the CT spawn, open at both ends.
        add_box(geometry, "SiteWall_%s" % ("A" if sign > 0 else "B"), wall, (18.0, 9.0 * sign, WALL * 0.5),
                (8.0, 1.0, WALL))
    # Mid doors at the north end of mid: a 3 m doorway.
    add_box(geometry, "MidDoor_West", wall, (9.5, -3.75, WALL * 0.5), (1.0, 4.5, WALL))
    add_box(geometry, "MidDoor_East", wall, (9.5, 3.75, WALL * 0.5), (1.0, 4.5, WALL))
    # A low wall at B, and the player clip that keeps players from jumping over it.
    add_box(geometry, "BLowWall", wall, (14.0, -20.0, 0.5), (0.5, 5.0, 1.0))
    add_volume(gameplay, "Clip_BLowWall", (14.0, -20.0, 2.25), (0.5, 5.0, 2.5))

    # Cover: 1.1 m crates (one stacked) and a crate stack whose collision is one box (UCX_).
    crates = [
        (0.0, -2.5, 0.0), (-6.0, 3.0, 0.0),  # mid
        (18.0, 15.0, 0.0), (19.2, 15.0, 0.0), (18.6, 15.0, CRATE), (24.0, 20.0, 0.0),  # A site
        (17.0, -15.0, 0.0), (22.0, -19.0, 0.0), (22.0, -20.2, 0.0),  # B site
        (-20.0, -10.0, 0.0), (-20.0, 10.0, 0.0),  # T plaza
        (21.0, 5.0, 0.0),  # CT spawn
        (-4.0, 15.0, 0.0), (-4.0, -16.0, 0.0),  # the longs
    ]
    for index, (x, y, z) in enumerate(crates, start=1):
        add_box(geometry, "Crate_%02d" % index, crate, (x, y, z + CRATE * 0.5), (CRATE, CRATE, CRATE))
    stack = make_cube_mesh("CrateStack", bpy.data.materials["Crate"],
                           [((0.0, 0.0, 0.55), (1.2, 1.2, 1.1)), ((0.1, -0.05, 1.6), (1.0, 1.0, 1.0))])
    stack_object = add_object("CrateStack", stack, geometry)
    stack_object.location = to_blender(14.0, 2.0, 0.0)
    ucx = add_object("UCX_CrateStack_01", make_cube_mesh("UCX_CrateStack_01", None,
                                                         [((0.0, 0.0, 1.05), (1.2, 1.2, 2.1))]), geometry)
    ucx.location = to_blender(14.0, 2.0, 0.0)

    # The floor pads: the sites and the spawns in colour (1 cm thick).
    add_box(geometry, "Pad_SiteA", pad_a, (20.0, 17.0, 0.005), (14.0, 10.0, 0.01))
    add_box(geometry, "Pad_SiteB", pad_b, (20.0, -17.0, 0.005), (14.0, 10.0, 0.01))
    add_box(geometry, "Pad_SpawnCT", pad_ct, (25.5, 0.0, 0.005), (7.0, 12.0, 0.01))
    add_box(geometry, "Pad_SpawnT", pad_t, (-25.5, 0.0, 0.005), (7.0, 16.0, 0.01))

    # The bomb sites and the buy zones (trigger volumes, 3 m high).
    add_volume(gameplay, "BombSite_A", (20.0, 17.0, 1.5), (14.0, 10.0, 3.0))
    add_volume(gameplay, "BombSite_B", (20.0, -17.0, 1.5), (14.0, 10.0, 3.0))
    add_volume(gameplay, "BuyZone_CT", (25.5, 0.0, 1.5), (7.0, 12.0, 3.0))
    add_volume(gameplay, "BuyZone_T", (-25.5, 0.0, 1.5), (7.0, 16.0, 3.0))

    # Five starts a team, 2 m apart: the CTs face south, the Ts north.
    for index, y in enumerate((-4.0, -2.0, 0.0, 2.0, 4.0)):
        suffix = "" if index == 0 else ".%03d" % index
        add_empty(gameplay, "PlayerStart_CT" + suffix, (26.5, y, START_HEIGHT), "ARROWS", yaw=180.0)
        add_empty(gameplay, "PlayerStart_T" + suffix, (-26.5, y, START_HEIGHT), "ARROWS", yaw=0.0)

    # The waypoint graph: the main routes, linked by hand both ways; the import adds the links a player can walk
    # (bAutoLinkWaypoints in Config/DefaultEditor.ini).
    waypoints = {
        "TSpawn": (-24.0, 0.0), "TMid": (-17.0, 0.0), "TPlazaA": (-18.0, 16.0), "TPlazaB": (-18.0, -16.0),
        "Mid": (-4.0, 0.0), "ShortA": (0.0, 9.0), "ShortB": (0.0, -9.0), "LongA": (0.0, 16.0), "LongB": (0.0, -16.0),
        "MidDoors": (9.5, 0.0), "CTMid": (12.0, -1.0), "AConnector": (11.5, 11.0), "BConnector": (11.5, -11.0),
        "SiteA": (19.0, 18.0), "SiteB": (19.0, -17.0), "CTSpawn": (25.0, 0.0), "CTA": (26.0, 10.0),
        "CTB": (26.0, -10.0),
    }
    links = [
        ("TSpawn", "TMid"), ("TSpawn", "TPlazaA"), ("TSpawn", "TPlazaB"), ("TMid", "Mid"), ("TPlazaA", "LongA"),
        ("TPlazaB", "LongB"), ("Mid", "ShortA"), ("Mid", "ShortB"), ("Mid", "MidDoors"), ("ShortA", "LongA"),
        ("ShortB", "LongB"), ("LongA", "AConnector"), ("LongB", "BConnector"), ("MidDoors", "CTMid"),
        ("CTMid", "AConnector"), ("CTMid", "BConnector"), ("AConnector", "SiteA"), ("BConnector", "SiteB"),
        ("CTMid", "CTSpawn"), ("CTSpawn", "CTA"), ("CTSpawn", "CTB"), ("CTA", "SiteA"), ("CTB", "SiteB"),
    ]
    neighbours = {name: [] for name in waypoints}
    for a, b in links:
        neighbours[a].append(b)
        neighbours[b].append(a)
    for name, (x, y) in waypoints.items():
        obj = add_empty(navigation, "NavWaypoint_" + name, (x, y, 0.5), "SPHERE", (0.3, 0.3, 0.3))
        obj["links"] = ",".join("NavWaypoint_" + other for other in sorted(neighbours[name]))

    # The sun: high, shining down toward the south-east (engine direction), so the walls facing the CT side and the
    # sites are lit; it casts the shadows.
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 1.0
    sun_data.color = (1.0, 0.96, 0.88)
    sun = add_object("Sun", sun_data, scene.collection)
    direction = to_blender(-0.45, 0.30, -0.84).normalized()
    sun.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    sun.location = to_blender(0.0, 0.0, 30.0)


def export():
    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
    bpy.ops.export_scene.gltf(
        filepath=GLB_PATH,
        export_format="GLB",
        export_yup=True,
        export_apply=True,
        export_extras=True,
        export_lights=True,
        export_cameras=False,
        export_materials="EXPORT",
        export_import_convert_lighting_mode="RAW",
        use_selection=False,
    )


if __name__ == "__main__":
    build()
    export()
    print("make_de_leon: wrote %s and %s" % (BLEND_PATH, GLB_PATH))
