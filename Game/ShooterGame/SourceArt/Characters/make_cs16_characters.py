"""Builds ShooterGame's team bodies from the Counter-Strike 1.6 player models (Docs/ART_PIPELINE.md, "The characters"):
the SAS for the counter-terrorists and the Leet Krew for the terrorists, rigged to the shared 23-bone skeleton
SKEL_Body so that every third-person clip (make_characters.py, anim_body.py) plays on them unchanged.

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" --background --factory-startup ^
        --python Game/ShooterGame/SourceArt/Characters/make_cs16_characters.py [-- --out <folder>]

Reads CS16/<model>/ next to this script (the FBX files and their textures, as provided: LICENSES.md) and writes next
to this script (or to <folder>):

- Body_CT.glb: cs_sas.fbx, the SAS (the gas mask's goggles from Chrome1.png);
- Body_T.glb: cs_leet.fbx, the Leet Krew (the eyes from Chrome3.png);
- cs16_characters.blend: the scene the files were exported from (for looking; the script is the source).

Each body is skinned to leon_art.HUMANOID_BONES with the model's own weights (GoldSrc's: one bone a vertex) carried
over from its Valve biped (Bip01 ...): every Bip01 bone and helper ("-- L Elbow", the fingers, the twists) goes to the
SKEL_Body bone it moves with, or half and half to the two bones it sits between (the elbows, the wrists, the knees,
the ankles, the shoulders and the hips' helpers), at most two weights a vertex. The model already rests in a T pose,
palms down, facing -Y (Blender), 2.1 m tall; it is turned to face +X, scaled so its shoulders are at SKEL_Body's 1.45 m,
and its limbs are moved onto SKEL_Body's rest pose (the bind pose the clips are authored for): each arm and leg bone of
the model is turned and stretched along itself onto its SKEL_Body bone (the legs, which stand apart, come together and
straight; the arms move a few centimetres in), and the hands and the feet are moved without turning or stretching (the
boots keep their shape, their soles on the floor). The torso, the neck and the head stay as they are.

The textures become one 128 x 128 texture a body (P8 in the cook, as the painted bodies had), so a body is one
material and one texture switch: the 512 x 512 skin averaged 4 x 4 texels to 128 x 128; the skin's texture has no
free block, so the few chrome triangles (GoldSrc draws them with a reflection map: the SAS's goggles, the Leet's eyes)
take the skin's texel nearest the chrome texture's average colour. The C4 backpack and the defuse kit (GoldSrc body
groups, drawn only when the player carries them) and their textures are left out. Only Blender's own modules are used.
"""

import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, os.pardir))

import leon_art  # noqa: E402 (after the path)

# The characters' triangle budget (Docs/ART_PIPELINE.md).
BUDGET = 1000
# A body's texture side.
SIZE = 128

# The teams' models: the FBX, the skin and the chrome (CS16/<folder>/).
MODELS = {
    "CT": {"folder": "sas", "fbx": "cs_sas.fbx", "skin": "SAS_DMBASE1.png", "chrome": "Chrome1.png"},
    "T": {"folder": "leet", "fbx": "cs_leet.fbx", "skin": "Arab_dmbase1.png", "chrome": "Chrome3.png"},
}

# SKEL_Body's shoulder height: the model's is scaled to it.
SHOULDER_HEIGHT = 1.45
# The heights between which a vertex of the model's lower spine (Bip01 Spine) goes from spine_01 to spine_02.
SPINE_BLEND = (1.13, 1.27)


def bone_weights(source):
    """The SKEL_Body bones (bone, weight) a bone of the model's Valve biped maps to; None for one left out."""
    name = source.lower()
    side = None
    for prefix, letter in (("bip01 l ", "l"), ("bip01 r ", "r"), ("-- l ", "l"), ("-- r ", "r")):
        if name.startswith(prefix):
            side, name = letter, name[len(prefix):]
            break
    if side is None:
        table = {
            "bip01": [("root", 1.0)],
            "bip01 pelvis": [("pelvis", 1.0)],
            "bip01 spine": "spine",
            "bip01 spine1": [("spine_02", 1.0)],
            "bip01 spine2": [("spine_02", 1.0)],
            "bip01 spine3": [("spine_03", 1.0)],
            "bip01 neck": [("neck_01", 1.0)],
            "-- neck smooth": [("neck_01", 1.0)],
            "bip01 head": [("head", 1.0)],
            "bone01": [("head", 1.0)],
        }
        return table.get(name)
    s = "_" + side

    def half(a, b):
        return [(a + s, 0.5), (b + s, 0.5)]

    if name == "clavicle":
        return [("clavicle" + s, 1.0)]
    if name in ("shoulder outside", "shoulder inside"):
        return half("clavicle", "upperarm")
    if name in ("upperarm", "bicep twist"):
        return [("upperarm" + s, 1.0)]
    if name == "elbow":
        return half("upperarm", "lowerarm")
    if name in ("forearm", "forearm twist"):
        return [("lowerarm" + s, 1.0)]
    if name == "wrist":
        return half("lowerarm", "hand")
    if name == "hand" or name.startswith("finger") or name == "knuckle":
        return [("hand" + s, 1.0)]
    if name == "butt":
        return [("pelvis", 0.5), ("thigh" + s, 0.5)]
    if name == "thigh":
        return [("thigh" + s, 1.0)]
    if name == "knee":
        return half("thigh", "calf")
    if name == "calf":
        return [("calf" + s, 1.0)]
    if name == "ankle":
        return half("calf", "foot")
    if name == "foot":
        return [("foot" + s, 1.0)]
    if name == "toe0":
        return [("ball" + s, 1.0)]
    return None


def import_model(path):
    """Imports an FBX; returns (armature, meshes) of the new objects."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, use_custom_normals=False, use_image_search=False, global_scale=1.0,
                             ignore_leaf_bones=False, automatic_bone_orientation=False, use_anim=False)
    new = sorted((obj for obj in bpy.data.objects if obj not in before), key=lambda obj: obj.name)
    armatures = [obj for obj in new if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        raise ValueError("%s: expected one armature, found %d" % (path, len(armatures)))
    return armatures[0], [obj for obj in new if obj.type == "MESH"], new


def image_names(obj):
    names = []
    for material in obj.data.materials:
        if material is not None and material.node_tree is not None:
            names += [node.image.name.lower() for node in material.node_tree.nodes
                      if node.type == "TEX_IMAGE" and node.image is not None]
    return names


def find_mesh(meshes, word):
    found = [obj for obj in meshes if any(word in name for name in image_names(obj))]
    if len(found) != 1:
        raise ValueError("expected one mesh textured with %s, found %d" % (word, len(found)))
    return found[0]


class Model:
    """The model's rest pose in SKEL_Body's frame: Blender's axes, the model turned to face +X and scaled to
    SKEL_Body's shoulders."""

    def __init__(self, armature):
        self.armature = armature
        bones = armature.data.bones
        shoulder = (armature.matrix_world @ bones["Bip01 R UpperArm"].head_local).z
        self.scale = SHOULDER_HEIGHT / shoulder
        # The model faces -Y: a quarter turn about Z faces it +X (its left, +X, goes to +Y: the engine's -Y, left).
        self.matrix = Matrix.Scale(self.scale, 4) @ Matrix.Rotation(math.radians(90.0), 4, "Z") @ armature.matrix_world

    def joint(self, bone):
        return self.matrix @ self.armature.data.bones[bone].head_local

    def point(self, local, obj):
        return self.matrix @ self.armature.matrix_world.inverted() @ obj.matrix_world @ local


def target_bones():
    return {name: (leon_art.to_blender(*head), leon_art.to_blender(*tail))
            for name, _parent, head, tail in leon_art.HUMANOID_BONES}


def fit_bones(model, lowest):
    """Each SKEL_Body bone's place on the model (head, tail; Blender, the model in SKEL_Body's frame), for the bones
    that move the model onto SKEL_Body's rest pose; lowest[side] is the lowest point of that side's boot."""
    target = target_bones()
    fit = {}
    for side, letter in (("L", "l"), ("R", "r")):
        s = "_" + letter
        clavicle = model.joint("Bip01 %s Clavicle" % side)
        upperarm = model.joint("Bip01 %s UpperArm" % side)
        forearm = model.joint("Bip01 %s Forearm" % side)
        hand = model.joint("Bip01 %s Hand" % side)
        fit["clavicle" + s] = (clavicle, upperarm)
        fit["upperarm" + s] = (upperarm, forearm)
        fit["lowerarm" + s] = (forearm, hand)
        # The hand moves without turning or stretching.
        fit["hand" + s] = (hand, hand + (target["hand" + s][1] - target["hand" + s][0]))
        thigh = model.joint("Bip01 %s Thigh" % side)
        calf = model.joint("Bip01 %s Calf" % side)
        toe = model.joint("Bip01 %s Toe0" % side)
        # The foot and the ball move without turning or stretching: SKEL_Body's foot bone is placed on the model so
        # that its ball is at the model's toe and the boot's sole lands on the floor.
        foot_vector = target["foot" + s][1] - target["foot" + s][0]
        foot = toe - foot_vector
        foot.z = target["foot" + s][0].z + lowest[letter]
        fit["thigh" + s] = (thigh, calf)
        fit["calf" + s] = (calf, foot)
        fit["foot" + s] = (foot, foot + foot_vector)
        ball = foot + foot_vector
        fit["ball" + s] = (ball, ball + (target["ball" + s][1] - target["ball" + s][0]))
    return fit


def bone_deform(fit, target):
    """The deformation of a bone's vertices from the model onto SKEL_Body: turned from the fitted bone's direction to
    the target's, stretched along it by their lengths' ratio, its head on the target's head."""
    (fh, ft), (th, tt) = fit, target
    fd, td = ft - fh, tt - th
    rotation = fd.normalized().rotation_difference(td.normalized()).to_matrix()
    stretch = td.length / fd.length
    axis = fd.normalized()

    def deform(p):
        local = p - fh
        local = local + axis * (local.dot(axis) * (stretch - 1.0))
        return th + rotation @ local

    return deform


def vertex_weights(obj, model):
    """Per vertex the SKEL_Body weights [(bone, weight)], from the model's vertex groups."""
    groups = {group.index: group.name for group in obj.vertex_groups}
    result = []
    for vertex in obj.data.vertices:
        z = model.point(vertex.co, obj).z
        total = {}
        for element in vertex.groups:
            if element.weight <= 0.0:
                continue
            mapped = bone_weights(groups[element.group])
            if mapped is None:
                raise ValueError("%s: no SKEL_Body bone for the model's bone %s" % (obj.name, groups[element.group]))
            if mapped == "spine":
                t = min(max((z - SPINE_BLEND[0]) / (SPINE_BLEND[1] - SPINE_BLEND[0]), 0.0), 1.0)
                mapped = [("spine_01", 1.0 - t), ("spine_02", t)]
            for bone, weight in mapped:
                if weight > 0.0:
                    total[bone] = total.get(bone, 0.0) + weight * element.weight
        if not total:
            raise ValueError("%s: vertex %d has no weight" % (obj.name, vertex.index))
        order = [entry[0] for entry in leon_art.HUMANOID_BONES]
        kept = sorted(total.items(), key=lambda item: (-item[1], order.index(item[0])))[:leon_art.MAX_INFLUENCES]
        norm = sum(weight for _bone, weight in kept)
        result.append([(bone, weight / norm) for bone, weight in kept])
    return result


def read_pixels(path):
    """An image file's texels as rows of (r, g, b) bytes, y = 0 the bottom row (Blender's)."""
    image = bpy.data.images.load(path)
    width, height = image.size
    values = [round(v * 255.0) for v in image.pixels[:]]
    channels = image.channels
    bpy.data.images.remove(image)
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            i = (y * width + x) * channels
            if channels >= 3:
                row.append((values[i], values[i + 1], values[i + 2]))
            else:
                row.append((values[i], values[i], values[i]))
        rows.append(row)
    return rows


def box_downscale(rows, side):
    """Averages blocks of texels down to side x side."""
    height, width = len(rows), len(rows[0])
    fx, fy = width // side, height // side
    out = []
    for y in range(side):
        row = []
        for x in range(side):
            acc = [0, 0, 0]
            for yy in range(y * fy, (y + 1) * fy):
                for xx in range(x * fx, (x + 1) * fx):
                    texel = rows[yy][xx]
                    acc[0] += texel[0]
                    acc[1] += texel[1]
                    acc[2] += texel[2]
            n = fx * fy
            row.append(tuple((c + n // 2) // n for c in acc))
        out.append(row)
    return out


def nearest_texel(texels, colour):
    """The texel (x, y) whose 3 x 3 neighbourhood is nearest a colour (the first on a tie, rows from the bottom), so
    a triangle mapped on its centre shows that colour at every mip level."""
    best, best_cost = None, None
    side = len(texels)
    for y in range(1, side - 1):
        for x in range(1, side - 1):
            cost = 0
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    texel = texels[y + dy][x + dx]
                    cost += sum((a - b) * (a - b) for a, b in zip(texel, colour))
            if best_cost is None or cost < best_cost:
                best, best_cost = (x, y), cost
    return best


def average(texels):
    flat = [texel for row in texels for texel in row]
    return tuple((sum(texel[c] for texel in flat) + len(flat) // 2) // len(flat) for c in range(3))


def build_body(team, armature):
    spec = MODELS[team]
    folder = os.path.join(HERE, "CS16", spec["folder"])
    source, meshes, imported = import_model(os.path.join(folder, spec["fbx"]))
    skin_obj = find_mesh(meshes, "dmbase")
    chrome_obj = find_mesh(meshes, "chrome")
    model = Model(source)
    name = "Body_" + team

    # One mesh: the skin's triangles (material 0) and the chrome's (material 1), in the model's rest pose turned and
    # scaled to SKEL_Body's frame, with their SKEL_Body weights.
    positions, weights, faces, uvs, face_material = [], [], [], [], []
    for material_index, obj in enumerate((skin_obj, chrome_obj)):
        base = len(positions)
        positions += [model.point(vertex.co, obj) for vertex in obj.data.vertices]
        weights += vertex_weights(obj, model)
        uv = obj.data.uv_layers.active.data
        for polygon in obj.data.polygons:
            faces.append(tuple(base + index for index in polygon.vertices))
            uvs.append(tuple((uv[loop].uv.x, uv[loop].uv.y) for loop in polygon.loop_indices))
            face_material.append(material_index)

    # The lowest point of each boot, from the floor (the feet move so the soles touch it).
    lowest = {}
    for letter in ("l", "r"):
        zs = [p.z for p, w in zip(positions, weights) if any(b in ("foot_" + letter, "ball_" + letter) for b, _ in w)]
        lowest[letter] = min(zs)
    # Onto SKEL_Body's rest pose: the limbs' bones moved, the torso, the neck and the head as they are.
    target = target_bones()
    fit = fit_bones(model, lowest)
    deforms = {bone: bone_deform(fit[bone], target[bone]) for bone in fit}
    posed = []
    for p, w in zip(positions, weights):
        q = Vector((0.0, 0.0, 0.0))
        for bone, weight in w:
            q += (deforms[bone](p) if bone in deforms else p) * weight
        posed.append(q)
    for letter in ("l", "r"):
        zs = [p.z for p, w in zip(posed, weights) if any(b in ("foot_" + letter, "ball_" + letter) for b, _ in w)]
        print("make_cs16_characters: %s's %s sole at %.4f m" % (name, letter, min(zs)))

    # The texture: the skin averaged to SIZE; the chrome's triangles take the skin's texel nearest the chrome's
    # average colour (the skin's texture has no free block for it).
    skin = box_downscale(read_pixels(os.path.join(folder, spec["skin"])), SIZE)
    chrome_colour = average(read_pixels(os.path.join(folder, spec["chrome"])))
    cx, cy = nearest_texel(skin, chrome_colour)
    print("make_cs16_characters: %s's chrome %s on the skin's texel (%d, %d) %s"
          % (name, chrome_colour, cx, cy, skin[cy][cx]))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([tuple(p) for p in posed], [], faces)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon, corner_uvs, material_index in zip(mesh.polygons, uvs, face_material):
        polygon.use_smooth = True
        for corner, loop in enumerate(polygon.loop_indices):
            if material_index == 0:
                uv_layer.data[loop].uv = corner_uvs[corner]
            else:
                uv_layer.data[loop].uv = ((cx + 0.5) / SIZE, (cy + 0.5) / SIZE)
    body = leon_art.add_object(name, mesh)

    def painter(x, y):
        return skin[y][x]

    image = leon_art.paint_image(name + "_D", SIZE, SIZE, painter)
    material = leon_art.make_material(name, roughness=0.8, image=image, surface="Flesh")
    mesh.materials.append(material)
    mesh.update()
    count = leon_art.check_triangles(body, BUDGET)
    leon_art.skin_rigid(body, armature, weights)
    print("make_cs16_characters: %s has %d triangles, %d vertices, %d colours" %
          (name, count, len(mesh.vertices), leon_art.count_colours(image)))

    # The imported model goes: its objects, meshes, materials, images and armature.
    for obj in imported:
        data = obj.data
        bpy.data.objects.remove(obj)
        if isinstance(data, bpy.types.Mesh) and data.users == 0:
            bpy.data.meshes.remove(data)
        elif isinstance(data, bpy.types.Armature) and data.users == 0:
            bpy.data.armatures.remove(data)
    for material in list(bpy.data.materials):
        if material.users == 0:
            bpy.data.materials.remove(material)
    for image_data in list(bpy.data.images):
        if image_data.users == 0:
            bpy.data.images.remove(image_data)
    return body


def build():
    leon_art.reset_scene()
    armature = leon_art.build_armature("Body", leon_art.HUMANOID_BONES)
    sockets = [leon_art.add_socket(armature, name, bone, location, axes=axes)
               for name, bone, location, axes in leon_art.HUMANOID_SOCKETS]
    bodies = {team: build_body(team, armature) for team in MODELS}
    return armature, sockets, bodies


def export(armature, sockets, bodies, out):
    for team, body in bodies.items():
        leon_art.export_glb(os.path.join(out, "Body_%s.glb" % team), objects=[armature] + sockets + [body],
                            extras=True, animations=False)
    leon_art.save_blend(os.path.join(out, "cs16_characters.blend"))


if __name__ == "__main__":
    out = leon_art.output_dir(HERE)
    export(*build(), out)
    print("make_cs16_characters: wrote the characters in %s" % out)
