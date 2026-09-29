"""leon_art: the shared Blender helpers of ShooterGame's source art scripts (Docs/ART_PIPELINE.md, ps2-shipping N26).

A `make_*.py` script next to its output imports it from this folder:

    import os, sys
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir))
    import leon_art

and runs headless, the same bytes on every run (plan decision D6: the scripts are the source of truth):

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Samples/make_art_samples.py [-- --out <folder>]

What it gives:

- `reset_scene`: the empty factory scene at 30 fps, metres, frame 0;
- `output_dir`: the folder a script writes to (`-- --out <folder>`, else the script's own folder);
- `make_material`, `paint_image`: a Principled material with a plain colour or a painted image whose sides are powers of
  two between 8 and 256 (the cook's P4 / P8 sizes);
- `add_boxes`, `mesh_from_boxes`, `triangle_count`, `check_triangles`: box blockouts and the triangle budgets;
- `MeshBuilder`: low-poly meshes of lofted rings, boxes and cylinders, UV-mapped to regions of a texture atlas and
  weighted to bones as they are made (the characters, the arms, the weapons);
- `noise`: a deterministic hash for painting textures;
- `build_armature` with `HUMANOID_BONES` (the CT / T bodies' shared skeleton) and `ARMS_BONES` (the first-person arms),
  `add_socket`: a `SOCKET_<Name>` empty on a bone, `hand_socket_frame`: the weapon sockets' frame on a hand;
- `skin_rigid`, `clamp_weights`: vertex groups, at most two weights a vertex;
- `Rig`, `Pose`: forward kinematics and two-bone IK over an armature's rest pose, turned into `add_action`'s keys;
- `add_action`: a clip keyed at 30 fps, its keys LINEAR, its notifies as pose markers and extras, pushed to its own NLA
  track;
- `export_glb`: the glTF 2.0 binary export with the fixed options (`GLTF_OPTIONS`); `save_blend`.

Positions are given in the engine's axes, metres (X forward, Y right, Z up) and placed in Blender's (x, -y, z)
(Docs/LEVELS.md). Only Blender's own modules are used.
"""

import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

# The engine's animation rate: the import samples every clip at 30 Hz (Docs/ASSET_FORMATS.md).
FPS = 30
# A vertex's bones: the skinned LPS2 v2 layout gives a vertex two palette indices and two weights.
MAX_INFLUENCES = 2
# A skinned batch's palette (LPS2 v2): a skeleton of at most 24 bones shares one palette across every batch.
MAX_PALETTE_BONES = 24
# The texture sides the cook keeps as they are (powers of two, 8 to 256; [/Script/LeonEd.CookSettings] MaxTextureSize).
TEXTURE_SIDES = (8, 16, 32, 64, 128, 256)

# The fixed glTF export options (Blender 5.2, io_scene_gltf2 5.2.39): every option that changes the file is named, so
# a script exports the same bytes whatever the exporter's defaults or the last settings saved in a .blend. glTF has no
# timestamp or UUID; the generator string names the exporter's version, so another Blender version is a new export.
GLTF_OPTIONS = {
    "export_format": "GLB",
    "export_copyright": "",
    "export_yup": True,
    "export_apply": True,
    "export_texcoords": True,
    "export_normals": True,
    "export_tangents": False,
    "export_gn_mesh": False,
    "export_attributes": False,
    "use_mesh_edges": False,
    "use_mesh_vertices": False,
    "export_vertex_color": "MATERIAL",
    "export_materials": "EXPORT",
    "export_image_format": "AUTO",
    "export_keep_originals": False,
    "export_unused_images": False,
    "export_unused_textures": False,
    "export_cameras": False,
    "export_gpu_instances": False,
    "export_shared_accessors": False,
    "export_use_gltfpack": False,
    "export_draco_mesh_compression_enable": False,
    "export_meshopt_compression_enable": False,
    "export_skins": True,
    "export_influence_nb": MAX_INFLUENCES,
    "export_all_influences": False,
    "export_def_bones": False,
    "export_leaf_bone": False,
    "export_hierarchy_flatten_bones": False,
    "export_hierarchy_flatten_objs": False,
    "export_armature_object_remove": False,
    "export_rest_position_armature": True,
    "export_reset_pose_bones": True,
    "export_morph": False,
    "export_animations": True,
    "export_animation_mode": "ACTIONS",
    "export_nla_strips": True,
    "export_force_sampling": True,
    "export_sampling_interpolation_fallback": "LINEAR",
    "export_frame_step": 1,
    "export_frame_range": False,
    "export_anim_slide_to_zero": True,
    "export_negative_frame": "SLIDE",
    "export_optimize_animation_size": True,
    "export_anim_single_armature": True,
    "export_bake_animation": False,
    "export_pointer_animation": False,
    "export_current_frame": False,
    "will_save_settings": False,
    "check_existing": False,
}


def to_blender(x, y, z):
    """Engine metres (X forward, Y right, Z up) to Blender's axes."""
    return Vector((x, -y, z))


def noise(x, y, seed=0):
    """A deterministic value in [0, 1) for integer texel coordinates (an integer hash: the same on every run and every
    machine, unlike a random generator's state)."""
    h = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFF) / 65536.0


def reset_scene(fps=FPS):
    """The empty factory scene: no objects, metres, `fps` frames a second, frame 0 current."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.render.fps = fps
    scene.render.fps_base = 1.0
    scene.frame_start = 0
    scene.frame_set(0)
    return scene


def output_dir(default):
    """The folder to write to: `--out <folder>` after Blender's `--` separator, else `default`; made if missing."""
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    folder = default
    if "--out" in argv and argv.index("--out") + 1 < len(argv):
        folder = os.path.abspath(argv[argv.index("--out") + 1])
    os.makedirs(folder, exist_ok=True)
    return folder


def check_texture_side(side):
    if side not in TEXTURE_SIDES:
        raise ValueError("a texture side must be a power of two from 8 to 256 (the cook's P4 / P8), not %d" % side)


def paint_image(name, width, height, painter):
    """A packed sRGB image of width x height texels, painter(x, y) -> (r, g, b) bytes 0-255 (y = 0 is the bottom row,
    Blender's). Keep an image to 16 colours for P4 or 256 for P8: the cook quantizes a larger count by median cut."""
    check_texture_side(width)
    check_texture_side(height)
    image = bpy.data.images.new(name, width, height, alpha=False)
    image.colorspace_settings.name = "sRGB"
    pixels = []
    for y in range(height):
        for x in range(width):
            r, g, b = painter(x, y)
            pixels.extend((r / 255.0, g / 255.0, b / 255.0, 1.0))
    image.pixels.foreach_set(pixels)
    image.pack()
    return image


def count_colours(image):
    """The distinct colours of an image (P4 holds 16, P8 256)."""
    values = [round(v * 255.0) for v in image.pixels[:]]
    return len({tuple(values[i:i + 4]) for i in range(0, len(values), 4)})


def phys_material(surface):
    """The package of the physical material of a surface (Concrete, Dirt, Metal, Wood, Tile, Glass, Computer, Flesh:
    Game/ShooterGame/SourceArt/ImportList.ini's PM_ sections)."""
    return "/Game/PhysicalMaterials/PM_" + surface


def make_material(name, rgb=(0.8, 0.8, 0.8), roughness=0.6, image=None, surface=None):
    """A Principled BSDF material: a base colour (linear RGB), or an image's texels times white. `surface` names its
    physical material (phys_material): the custom property `physMaterial`, which the export writes in the glTF
    material's extras and the import sets as the material's PhysMaterial (Docs/ART_PIPELINE.md)."""
    material = bpy.data.materials.new(name)
    if surface is not None:
        material["physMaterial"] = phys_material(surface)
    if material.node_tree is None:  # Blender 5 makes the node tree itself (use_nodes goes away in 6.0)
        material.use_nodes = True
    nodes = material.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = 0.0
    if image is not None:
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = image
        texture.location = (-400.0, 300.0)
        material.node_tree.links.new(texture.outputs["Color"], bsdf.inputs["Base Color"])
    return material


def add_boxes(bm, boxes, material_index=0):
    """Boxes (centre, size) in Blender metres into a bmesh with a UV layer; returns the new vertices."""
    made = []
    for center, size in boxes:
        result = bmesh.ops.create_cube(bm, size=1.0, calc_uvs=True)
        verts = result["verts"]
        bmesh.ops.scale(bm, vec=Vector(size), verts=verts)
        bmesh.ops.translate(bm, vec=Vector(center), verts=verts)
        for face in {face for vert in verts for face in vert.link_faces}:
            face.material_index = material_index
        made.extend(verts)
    return made


def mesh_from_boxes(name, boxes, materials=()):
    """A mesh of boxes (centre, size) in Blender metres, UVs from Blender's cube unwrap, with its materials."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bm.loops.layers.uv.new()
    add_boxes(bm, boxes)
    bm.to_mesh(mesh)
    bm.free()
    for material in materials:
        mesh.materials.append(material)
    return mesh


def add_object(name, data, collection=None):
    obj = bpy.data.objects.new(name, data)
    (collection or bpy.context.scene.collection).objects.link(obj)
    return obj


def triangle_count(obj):
    """The triangles an object's mesh exports as (every n-gon is n - 2)."""
    return sum(len(polygon.vertices) - 2 for polygon in obj.data.polygons)


def check_triangles(obj, budget):
    """Fails the script when a mesh is over its asset class's triangle budget (Docs/ART_PIPELINE.md)."""
    count = triangle_count(obj)
    if count > budget:
        raise ValueError("%s has %d triangles, over its budget of %d" % (obj.name, count, budget))
    return count


def _newell_normal(points):
    normal = Vector((0.0, 0.0, 0.0))
    for a, b in zip(points, points[1:] + points[:1]):
        normal.x += (a.y - b.y) * (a.z + b.z)
        normal.y += (a.z - b.z) * (a.x + b.x)
        normal.z += (a.x - b.x) * (a.y + b.y)
    return normal


def _perpendicular(reference, direction):
    """`reference` with its part along `direction` removed, normalized (another axis when they are parallel)."""
    d = direction.normalized()
    for candidate in (Vector(reference), Vector((0.0, 0.0, 1.0)), Vector((1.0, 0.0, 0.0))):
        u = candidate - d * candidate.dot(d)
        if u.length > 1.0e-6:
            return u.normalized()
    return Vector((0.0, 1.0, 0.0))


class MeshBuilder:
    """A low-poly mesh made part by part in the engine's axes (metres) for one material whose image is width x height
    texels. Every part maps its UVs into a region of that image, (x0, y0, x1, y1) in texels with y = 0 the bottom row
    (as paint_image paints), and gives its vertices their bone weights: a bone name or a list of (bone, weight), None
    for a mesh without a skin. A face takes the winding that points it along its outward hint, so parts need no normal
    fix-up; lofts and cylinders are smooth-shaded, boxes flat."""

    def __init__(self, width, height, inset=0.5):
        self.width = width
        self.height = height
        self.inset = inset
        self.positions = []
        self.weights = []
        self.faces = []

    def region_uv(self, region, u, v):
        """The UV of (u, v) in [0, 1] across a region of texels, half a texel inside its edges."""
        x0, y0, x1, y1 = region
        x = x0 + self.inset + (x1 - x0 - 2.0 * self.inset) * u
        y = y0 + self.inset + (y1 - y0 - 2.0 * self.inset) * v
        return (x / self.width, y / self.height)

    def vertex(self, position, weights=None):
        p = Vector(position)
        self.positions.append(to_blender(p.x, p.y, p.z))
        self.weights.append(weights)
        return len(self.positions) - 1

    def face(self, indices, uvs, outward, smooth=False):
        """A polygon of vertex indices with a UV each, turned to face `outward` (an engine direction)."""
        indices = list(indices)
        uvs = list(uvs)
        o = Vector(outward)
        if _newell_normal([self.positions[i] for i in indices]).dot(to_blender(o.x, o.y, o.z)) < 0.0:
            indices.reverse()
            uvs.reverse()
        self.faces.append((tuple(indices), tuple(uvs), smooth))

    def loft(self, rings, sides, region, smooth=True, caps=(True, True), reference=(1.0, 0.0, 0.0), phase=0.0):
        """A tube through rings, each a dict: `c` the centre, `r` a radius or (radius along the first axis, along the
        second), `w` the weights, optional `shape` (a radius factor a side) and `off` ((first, second) axis offset).
        A ring lies across the tube's direction there; its first axis is `reference` made perpendicular to it (+X:
        the front of a limb), its second the direction times the first. Side k sits at the angle
        pi + 2 pi (k + phase) / sides from the first axis, so u is 0 at the back and 0.5 at the front; v runs along the
        tube by length. The ends are closed by flat polygons unless `caps` says not."""
        centres = [Vector(ring["c"]) for ring in rings]
        overall = centres[-1] - centres[0]
        if overall.length < 1.0e-9:
            overall = Vector((0.0, 0.0, 1.0))
        lengths = [0.0]
        for a, b in zip(centres, centres[1:]):
            lengths.append(lengths[-1] + (b - a).length)
        total = lengths[-1] if lengths[-1] > 1.0e-9 else float(len(rings) - 1)
        indices = []
        for i, ring in enumerate(rings):
            ahead = centres[min(i + 1, len(rings) - 1)] - centres[max(i - 1, 0)]
            direction = (ahead if ahead.length > 1.0e-9 else overall).normalized()
            first = _perpendicular(reference, direction)
            second = direction.cross(first)
            radius = ring["r"]
            ru, rv = (radius, radius) if isinstance(radius, (int, float)) else radius
            shape = ring.get("shape")
            du, dv = ring.get("off", (0.0, 0.0))
            row = []
            for k in range(sides):
                angle = math.pi + 2.0 * math.pi * (k + phase) / sides
                scale = shape[k] if shape is not None else 1.0
                p = centres[i] + first * (du + ru * scale * math.cos(angle)) + second * (dv + rv * scale * math.sin(angle))
                row.append(self.vertex(p, ring.get("w")))
            indices.append(row)
        for i in range(len(rings) - 1):
            v0 = lengths[i] / total if lengths[-1] > 1.0e-9 else i / total
            v1 = lengths[i + 1] / total if lengths[-1] > 1.0e-9 else (i + 1) / total
            axis_point = (centres[i] + centres[i + 1]) * 0.5
            for k in range(sides):
                k1 = (k + 1) % sides
                quad = [indices[i][k], indices[i][k1], indices[i + 1][k1], indices[i + 1][k]]
                uvs = [self.region_uv(region, k / sides, v0), self.region_uv(region, (k + 1) / sides, v0),
                       self.region_uv(region, (k + 1) / sides, v1), self.region_uv(region, k / sides, v1)]
                centroid = Vector((0.0, 0.0, 0.0))
                for index in quad:
                    b = self.positions[index]
                    centroid += Vector((b.x, -b.y, b.z)) * 0.25
                self.face(quad, uvs, centroid - axis_point, smooth)
        for end, (row, v, outward) in enumerate(((indices[0], 0.0, -overall), (indices[-1], 1.0, overall))):
            if caps[end]:
                self.face(row, [self.region_uv(region, k / sides, v) for k in range(sides)], outward, False)
        return indices

    def cylinder(self, p0, p1, radius, sides, region, weights=None, caps=(True, True), smooth=True,
                 reference=(0.0, 0.0, 1.0), radius1=None):
        """A cylinder (a cone when radius1 differs) from p0 to p1."""
        return self.loft([{"c": p0, "r": radius, "w": weights},
                          {"c": p1, "r": radius if radius1 is None else radius1, "w": weights}],
                         sides, region, smooth, caps, reference)

    def box(self, center, size, region, weights=None, axes=None, taper=(1.0, 1.0), regions=None, skip=()):
        """A flat-shaded box: centre, size (along its axes), axes the (x, y, z) unit vectors (the engine's by
        default); taper scales the +x end's y and z. Every face maps the whole region (`regions` by face name,
        '+x' '-x' '+y' '-y' '+z' '-z', overrides it); `skip` leaves faces out."""
        c = Vector(center)
        ax, ay, az = [Vector(a) for a in (axes or ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)))]
        hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
        corners = {}
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    ty, tz = taper if sx > 0 else (1.0, 1.0)
                    p = c + ax * (sx * hx) + ay * (sy * hy * ty) + az * (sz * hz * tz)
                    corners[(sx, sy, sz)] = self.vertex(p, weights)
        faces = {
            "+x": ([(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)], ax),
            "-x": ([(-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)], -ax),
            "+y": ([(1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)], ay),
            "-y": ([(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)], -ay),
            "+z": ([(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)], az),
            "-z": ([(-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)], -az),
        }
        for name in ("+x", "-x", "+y", "-y", "+z", "-z"):
            if name in skip:
                continue
            keys, outward = faces[name]
            face_region = (regions or {}).get(name, region)
            uvs = [self.region_uv(face_region, u, v) for u, v in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))]
            self.face([corners[key] for key in keys], uvs, outward, False)

    def box_between(self, p0, p1, width, height, region, weights=None, up=(0.0, 0.0, 1.0), taper=(1.0, 1.0),
                    regions=None, extend=(0.0, 0.0)):
        """A box along the segment p0 -> p1 (its x), `width` across (y) and `height` along `up` made perpendicular
        (z); extend lengthens it past p0 and p1."""
        a, b = Vector(p0), Vector(p1)
        x = (b - a).normalized()
        z = _perpendicular(up, x)
        y = z.cross(x)
        start, end = a - x * extend[0], b + x * extend[1]
        self.box((start + end) * 0.5, ((end - start).length, width, height), region, weights, (x, y, z), taper,
                 regions)

    def prism(self, start, end, profile, region, weights=None, up=(0.0, 0.0, 1.0), taper=(1.0, 1.0)):
        """A flat-shaded prism from start to end: `profile` a closed polygon of (y, z) points (metres, counter-clockwise
        seen from the end) across the axis, its z along `up` made perpendicular; taper scales the end's profile. Each
        side face maps the whole region (its edges read as bevels), the ends map the profile's extent."""
        a, b = Vector(start), Vector(end)
        x = (b - a).normalized()
        z = _perpendicular(up, x)
        y = z.cross(x)
        rows = []
        for base, (ty, tz) in ((a, (1.0, 1.0)), (b, taper)):
            rows.append([self.vertex(base + y * (py * ty) + z * (pz * tz), weights) for py, pz in profile])
        count = len(profile)
        full = [self.region_uv(region, u, v) for u, v in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))]
        for k in range(count):
            k1 = (k + 1) % count
            quad = [rows[0][k], rows[0][k1], rows[1][k1], rows[1][k]]
            mid = Vector((0.0, 0.0, 0.0))
            for index in quad:
                p = self.positions[index]
                mid += Vector((p.x, -p.y, p.z)) * 0.25
            axis_point = a + x * (mid - a).dot(x)
            self.face(quad, full, mid - axis_point, False)
        ys = [py for py, _pz in profile]
        zs = [pz for _py, pz in profile]
        span_y = max(max(ys) - min(ys), 1.0e-9)
        span_z = max(max(zs) - min(zs), 1.0e-9)
        cap_uvs = [self.region_uv(region, (py - min(ys)) / span_y, (pz - min(zs)) / span_z) for py, pz in profile]
        self.face(rows[0], cap_uvs, -x, False)
        self.face(rows[1], cap_uvs, x, False)

    def vertex_bones(self):
        return list(self.weights)

    def build(self, name, material, collection=None):
        """The mesh object: the faces in the order made, their UVs and shading, the material."""
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata([tuple(p) for p in self.positions], [], [face[0] for face in self.faces])
        uv_layer = mesh.uv_layers.new(name="UVMap")
        for polygon, (_indices, uvs, smooth) in zip(mesh.polygons, self.faces):
            polygon.use_smooth = smooth
            for corner, loop in enumerate(polygon.loop_indices):
                uv_layer.data[loop].uv = uvs[corner]
        mesh.materials.append(material)
        mesh.update()
        return add_object(name, mesh, collection)


def hand_socket_frame(bones, hand, thumb, along=0.07, palm=0.02):
    """The weapon socket of a hand (Docs/ART_PIPELINE.md, "Sockets"): its x along the hand bone (wrist to knuckles),
    its z to the thumb's side (`thumb`, an engine direction), at the palm: `along` metres from the wrist and `palm`
    metres towards the palm (-y). Returns (location, (x, z)) in engine metres, as add_socket takes them; a weapon's
    grip sits at its origin, its barrel along +x and its top along +z."""
    table = {entry[0]: entry for entry in bones}
    head, tail = Vector(table[hand][2]), Vector(table[hand][3])
    x = (tail - head).normalized()
    z = _perpendicular(thumb, x)
    y = z.cross(x)
    location = head + x * along - y * palm
    return tuple(location), (tuple(x), tuple(z))


# The CT and T bodies' shared skeleton (name, parent, head, tail): engine metres, the character standing on its origin
# and facing +X, arms out (T pose), UE's mannequin names. 23 bones: one palette for the whole body.
HUMANOID_BONES = [
    ("root", None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.2)),
    ("pelvis", "root", (0.0, 0.0, 0.95), (0.0, 0.0, 1.05)),
    ("spine_01", "pelvis", (0.0, 0.0, 1.05), (0.0, 0.0, 1.20)),
    ("spine_02", "spine_01", (0.0, 0.0, 1.20), (0.0, 0.0, 1.35)),
    ("spine_03", "spine_02", (0.0, 0.0, 1.35), (0.0, 0.0, 1.50)),
    ("neck_01", "spine_03", (0.0, 0.0, 1.50), (0.0, 0.0, 1.58)),
    ("head", "neck_01", (0.0, 0.0, 1.58), (0.0, 0.0, 1.83)),
    ("clavicle_l", "spine_03", (0.0, -0.03, 1.45), (0.0, -0.18, 1.45)),
    ("upperarm_l", "clavicle_l", (0.0, -0.18, 1.45), (0.0, -0.46, 1.45)),
    ("lowerarm_l", "upperarm_l", (0.0, -0.46, 1.45), (0.0, -0.72, 1.45)),
    ("hand_l", "lowerarm_l", (0.0, -0.72, 1.45), (0.0, -0.86, 1.45)),
    ("clavicle_r", "spine_03", (0.0, 0.03, 1.45), (0.0, 0.18, 1.45)),
    ("upperarm_r", "clavicle_r", (0.0, 0.18, 1.45), (0.0, 0.46, 1.45)),
    ("lowerarm_r", "upperarm_r", (0.0, 0.46, 1.45), (0.0, 0.72, 1.45)),
    ("hand_r", "lowerarm_r", (0.0, 0.72, 1.45), (0.0, 0.86, 1.45)),
    ("thigh_l", "pelvis", (0.0, -0.10, 0.95), (0.0, -0.10, 0.50)),
    ("calf_l", "thigh_l", (0.0, -0.10, 0.50), (0.0, -0.10, 0.08)),
    ("foot_l", "calf_l", (0.0, -0.10, 0.08), (0.12, -0.10, 0.02)),
    ("ball_l", "foot_l", (0.12, -0.10, 0.02), (0.20, -0.10, 0.02)),
    ("thigh_r", "pelvis", (0.0, 0.10, 0.95), (0.0, 0.10, 0.50)),
    ("calf_r", "thigh_r", (0.0, 0.10, 0.50), (0.0, 0.10, 0.08)),
    ("foot_r", "calf_r", (0.0, 0.10, 0.08), (0.12, 0.10, 0.02)),
    ("ball_r", "foot_r", (0.12, 0.10, 0.02), (0.20, 0.10, 0.02)),
]
# Where the third-person weapon goes: SOCKET_Weapon_R on hand_r (the socket Weapon_R), the grip at the palm, x along
# the hand and z to the thumb (forward in the T pose, the palm down): (name, bone, location, axes).
HUMANOID_SOCKETS = [("Weapon_R", "hand_r") + hand_socket_frame(HUMANOID_BONES, "hand_r", (1.0, 0.0, 0.0))]

# The first-person arms (name, parent, head, tail): engine metres from the camera (the eye) looking along +X. 11 bones.
ARMS_BONES = [
    ("root", None, (0.0, 0.0, 0.0), (0.1, 0.0, 0.0)),
    ("upperarm_l", "root", (-0.05, -0.18, -0.30), (0.20, -0.16, -0.26)),
    ("lowerarm_l", "upperarm_l", (0.20, -0.16, -0.26), (0.40, -0.06, -0.20)),
    ("hand_l", "lowerarm_l", (0.40, -0.06, -0.20), (0.48, -0.03, -0.18)),
    ("thumb_01_l", "hand_l", (0.42, -0.03, -0.17), (0.48, 0.00, -0.14)),
    ("fingers_01_l", "hand_l", (0.48, -0.03, -0.18), (0.54, -0.02, -0.18)),
    ("upperarm_r", "root", (-0.05, 0.18, -0.30), (0.20, 0.16, -0.26)),
    ("lowerarm_r", "upperarm_r", (0.20, 0.16, -0.26), (0.44, 0.10, -0.18)),
    ("hand_r", "lowerarm_r", (0.44, 0.10, -0.18), (0.52, 0.08, -0.16)),
    ("thumb_01_r", "hand_r", (0.46, 0.07, -0.16), (0.52, 0.04, -0.13)),
    ("fingers_01_r", "hand_r", (0.52, 0.08, -0.16), (0.58, 0.07, -0.16)),
]
# The view model's weapon: SOCKET_Weapon_R on hand_r, as on the body (the thumb up at rest).
ARMS_SOCKETS = [("Weapon_R", "hand_r") + hand_socket_frame(ARMS_BONES, "hand_r", (0.0, 0.0, 1.0), along=0.05)]


def build_armature(name, bones, collection=None):
    """An armature object of `bones` (name, parent, head, tail; engine metres), parents before children, no roll."""
    if len(bones) > MAX_PALETTE_BONES:
        raise ValueError("%s has %d bones, over one palette's %d" % (name, len(bones), MAX_PALETTE_BONES))
    names = [bone[0] for bone in bones]
    for index, (bone_name, parent, _head, _tail) in enumerate(bones):
        if names.count(bone_name) != 1 or (parent is not None and parent not in names[:index]):
            raise ValueError("%s: bone %s must be unique and come after its parent" % (name, bone_name))
    data = bpy.data.armatures.new(name)
    obj = add_object(name, data, collection)
    view_layer = bpy.context.view_layer
    for other in view_layer.objects:
        other.select_set(other is obj)
    view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    for bone_name, parent, head, tail in bones:
        edit_bone = data.edit_bones.new(bone_name)
        edit_bone.head = to_blender(*head)
        edit_bone.tail = to_blender(*tail)
        edit_bone.roll = 0.0
        edit_bone.use_deform = True
        if parent is not None:
            edit_bone.parent = data.edit_bones[parent]
            edit_bone.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    return obj


def _frame_matrix(x_axis, z_axis):
    """The rotation whose x and z axes are these Blender directions (z made perpendicular to x)."""
    x = Vector(x_axis).normalized()
    z = _perpendicular(z_axis, x)
    y = z.cross(x)
    return Matrix((x, y, z)).transposed()


def add_socket(armature, name, bone, location, collection=None, axes=None):
    """A `SOCKET_<name>` empty on `bone` at an engine location (metres, the armature at rest): the import makes it the
    skeleton's socket <name> on that bone (Docs/ASSET_FORMATS.md). axes, (x, z) engine directions, turns it (else it
    keeps the armature's axes)."""
    empty = add_object("SOCKET_" + name, None, collection)
    empty.empty_display_type = "ARROWS"
    empty.empty_display_size = 0.05
    empty.parent = armature
    empty.parent_type = "BONE"
    empty.parent_bone = bone
    bpy.context.view_layer.update()
    placement = Matrix.Translation(to_blender(*location))
    if axes is not None:
        placement = placement @ _frame_matrix(to_blender(*axes[0]), to_blender(*axes[1])).to_4x4()
    empty.matrix_world = armature.matrix_world @ placement
    return empty



def skin_rigid(mesh_obj, armature, vertex_bones):
    """Skins a mesh to an armature: vertex_bones[i] is a bone name, or a list of (bone, weight). The object is parented
    to the armature with an Armature modifier, and the weights clamped to two a vertex."""
    mesh_obj.parent = armature
    modifier = mesh_obj.modifiers.new("Armature", "ARMATURE")
    modifier.object = armature
    groups = {}
    for bone in armature.data.bones:
        groups[bone.name] = mesh_obj.vertex_groups.new(name=bone.name)
    for index, entry in enumerate(vertex_bones):
        weights = [(entry, 1.0)] if isinstance(entry, str) else entry
        for bone, weight in weights:
            groups[bone].add([index], weight, "ADD")
    clamp_weights(mesh_obj)


def clamp_weights(mesh_obj, max_influences=MAX_INFLUENCES):
    """Keeps each vertex's `max_influences` largest weights (the lower group first on a tie), normalized to 1: what the
    import would keep, decided in the source so that Blender shows what the PS2 draws."""
    groups = mesh_obj.vertex_groups
    for vertex in mesh_obj.data.vertices:
        weights = sorted(((g.weight, g.group) for g in vertex.groups if g.weight > 0.0), key=lambda w: (-w[0], w[1]))
        kept = weights[:max_influences]
        total = sum(weight for weight, _group in kept)
        for element in [g.group for g in vertex.groups]:
            groups[element].remove([vertex.index])
        for weight, group in kept:
            groups[group].add([vertex.index], weight / total, "REPLACE")


def frame_rotation(from_x, from_z, to_x, to_z):
    """The rotation (Blender's axes) taking the frame of directions (from_x, from_z) to (to_x, to_z)."""
    return (_frame_matrix(to_x, to_z) @ _frame_matrix(from_x, from_z).transposed()).to_quaternion()


class Rig:
    """An armature's rest pose for posing by code: `bones` as build_armature took them, the rest rotations read from
    the armature. Every pose is in Blender's axes, the armature's space at rest."""

    def __init__(self, armature, bones):
        self.names = [entry[0] for entry in bones]
        self.parent = {entry[0]: entry[1] for entry in bones}
        self.head = {entry[0]: to_blender(*entry[2]) for entry in bones}
        self.tail = {entry[0]: to_blender(*entry[3]) for entry in bones}
        self.rest = {name: armature.data.bones[name].matrix_local.to_quaternion() for name in self.names}

    def pose(self):
        return Pose(self)


class Pose:
    """A pose of a Rig: per bone a rotation in the armature's rest axes (the bone and its children turn about its
    head, after its parents' rotations) and an offset of its head. `solve` gives every bone's rotation from rest and
    head in the armature's space; `keys` the local keys add_action takes."""

    def __init__(self, rig):
        self.rig = rig
        self.rot = {}
        self.offset = {}

    def copy(self):
        other = Pose(self.rig)
        other.rot = {name: q.copy() for name, q in self.rot.items()}
        other.offset = {name: v.copy() for name, v in self.offset.items()}
        return other

    def rotate(self, bone, axis, degrees):
        """Turns a bone by degrees about an axis (Blender's), after the rotation it has."""
        if degrees != 0.0:
            self.rot[bone] = Quaternion(Vector(axis), math.radians(degrees)) @ self.rot.get(bone, Quaternion())
        return self

    def move(self, bone, offset):
        """Moves a bone's head by offset (Blender metres, before its parents' rotations)."""
        self.offset[bone] = self.offset.get(bone, Vector()) + Vector(offset)
        return self

    def solve(self):
        rig = self.rig
        world, heads = {}, {}
        for name in rig.names:
            parent = rig.parent[name]
            rotation = self.rot.get(name, Quaternion())
            offset = self.offset.get(name, Vector())
            if parent is None:
                world[name] = rotation.copy()
                heads[name] = rig.head[name] + offset
            else:
                world[name] = world[parent] @ rotation
                heads[name] = heads[parent] + world[parent] @ (rig.head[name] - rig.head[parent] + offset)
        return world, heads

    def point(self, bone, rest_point):
        """Where a point that moves with a bone (Blender, at rest) is in this pose."""
        world, heads = self.solve()
        return heads[bone] + world[bone] @ (Vector(rest_point) - self.rig.head[bone])

    def orient(self, bone, rotation):
        """Sets a bone's rotation from rest (its parents' included) to `rotation`."""
        world, _heads = self.solve()
        parent = self.rig.parent[bone]
        base = world[parent] if parent is not None else Quaternion()
        self.rot[bone] = base.inverted() @ rotation
        return self

    def reach(self, upper, lower, end, target, pole):
        """Two-bone IK: turns upper and lower so that end's head reaches target (Blender), the joint between them bent
        towards pole (a direction); a target out of reach is met as near as the chain stretches."""
        rig = self.rig
        world, heads = self.solve()
        shoulder = heads[upper]
        l1 = (rig.head[lower] - rig.head[upper]).length
        l2 = (rig.head[end] - rig.head[lower]).length
        to = Vector(target) - shoulder
        distance = min(max(to.length, abs(l1 - l2) + 1.0e-4), l1 + l2 - 1.0e-4)
        e = to.normalized()
        a = (l1 * l1 - l2 * l2 + distance * distance) / (2.0 * distance)
        h = math.sqrt(max(l1 * l1 - a * a, 0.0))
        p = _perpendicular(pole, e)
        elbow = shoulder + e * a + p * h
        wrist = shoulder + e * distance
        parent = rig.parent[upper]
        base = world[parent] if parent is not None else Quaternion()
        carried = base @ (rig.head[lower] - rig.head[upper]).normalized()
        upper_world = carried.rotation_difference((elbow - shoulder).normalized()) @ base
        self.rot[upper] = base.inverted() @ upper_world
        carried = upper_world @ (rig.head[end] - rig.head[lower]).normalized()
        lower_world = carried.rotation_difference((wrist - elbow).normalized()) @ upper_world
        self.rot[lower] = upper_world.inverted() @ lower_world
        return self

    def keys(self, bones=None):
        """{bone: (local quaternion (w, x, y, z), local location)} for add_action, every bone (or `bones`)."""
        rig = self.rig
        keys = {}
        for name in bones or rig.names:
            rest = rig.rest[name]
            local = rest.inverted() @ self.rot.get(name, Quaternion()) @ rest
            location = rest.inverted() @ self.offset.get(name, Vector())
            keys[name] = (tuple(local), tuple(location))
        return keys


def _channelbag_fcurves(action, slot):
    from bpy_extras import anim_utils

    channelbag = anim_utils.action_get_channelbag_for_slot(action, slot)
    return channelbag.fcurves if channelbag is not None else []


def add_action(armature, name, poses, notifies=(), loop=True):
    """A clip `name` on the armature: poses is a list of (frame, {bone: (rotation, location)}) at 30 fps. A rotation is
    (axis, degrees) about an axis of the armature's space (Blender's axes) through the bone's head, or a (w, x, y, z)
    quaternion in the bone's own rest space; a location is the bone's offset in its rest space (either may be None).
    Every key is LINEAR, and a bone's quaternion keys stay on one hemisphere (the shorter way between keys); notifies
    are (frame, name) pose markers. The action's custom properties `loop` (1 or 0) and `notifies` (a list of
    {"name", "time"}, the time in seconds from the clip's start) reach the glTF animation's extras when exported with
    extras, in the shape the import reads (Docs/ART_PIPELINE.md). The clip goes to an NLA track of its own, so the
    export writes one glTF animation per clip."""
    armature.animation_data_create()
    action = bpy.data.actions.new(name)
    armature.animation_data.action = action
    previous = {}
    for frame, pose in poses:
        for bone_name, (rotation, location) in pose.items():
            pose_bone = armature.pose.bones[bone_name]
            pose_bone.rotation_mode = "QUATERNION"
            if rotation is not None:
                if len(rotation) == 2:
                    axis, degrees = rotation
                    rest = pose_bone.bone.matrix_local.to_quaternion()
                    quat = rest.inverted() @ Quaternion(Vector(axis), math.radians(degrees)) @ rest
                else:
                    quat = Quaternion(rotation)
                if bone_name in previous and previous[bone_name].dot(quat) < 0.0:
                    quat.negate()
                previous[bone_name] = quat.copy()
                pose_bone.rotation_quaternion = quat
                pose_bone.keyframe_insert("rotation_quaternion", frame=frame, group=bone_name)
            if location is not None:
                pose_bone.location = Vector(location)
                pose_bone.keyframe_insert("location", frame=frame, group=bone_name)
    slot = armature.animation_data.action_slot
    for fcurve in _channelbag_fcurves(action, slot):
        for key in fcurve.keyframe_points:
            key.interpolation = "LINEAR"
    for frame, marker_name in notifies:
        marker = action.pose_markers.new(marker_name)
        marker.frame = frame
    # The export's extras carry what glTF has no field for (export_extras): the loop flag and the notifies.
    action["loop"] = 1 if loop else 0
    if notifies:
        action["notifies"] = [{"name": marker_name, "time": round(frame / FPS, 6)} for frame, marker_name in notifies]
    track = armature.animation_data.nla_tracks.new()
    track.name = name
    strip = track.strips.new(name, int(action.frame_range[0]), action)
    strip.name = name
    armature.animation_data.action = None
    for pose_bone in armature.pose.bones:
        pose_bone.rotation_quaternion = Quaternion()
        pose_bone.location = Vector()
    return action


def select_only(objects):
    view_layer = bpy.context.view_layer
    # Objects linked since the last update are not in the view layer's list yet.
    view_layer.update()
    chosen = set(objects)
    for obj in view_layer.objects:
        obj.select_set(obj in chosen)
    if objects:
        view_layer.objects.active = objects[0]


def export_glb(path, objects=None, extras=False, lights=False, lighting_mode="SPEC", animations=True):
    """Exports the scene (or only `objects`) to a .glb with GLTF_OPTIONS: custom properties as extras and punctual
    lights only when asked (a map's `links` and its lights, the materials' `physMaterial`; RAW keeps the lights'
    intensities as they are), and the armatures' clips unless `animations` is off (a skinned mesh's file without the
    clips its armature carries)."""
    if objects is not None:
        select_only(objects)
    options = dict(GLTF_OPTIONS, export_animations=animations)
    bpy.ops.export_scene.gltf(
        filepath=path,
        use_selection=objects is not None,
        export_extras=extras,
        export_lights=lights,
        export_import_convert_lighting_mode=lighting_mode,
        **options,
    )


def save_blend(path):
    """Saves the .blend to edit by hand (its bytes change on every save: the .glb is what is compared)."""
    bpy.ops.wm.save_as_mainfile(filepath=path)
