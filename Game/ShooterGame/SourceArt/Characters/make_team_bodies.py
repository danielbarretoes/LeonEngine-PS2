"""Builds the teams' placeholder bodies in Blender and exports one glTF per team for LeonEd's static mesh import.

    blender --background --factory-startup --python Game/ShooterGame/SourceArt/Characters/make_team_bodies.py

Writes team_bodies.blend and Body_CT.glb / Body_T.glb next to this script; Game/ShooterGame/SourceArt/ImportList.ini
imports them as /Game/Characters/SM_Body_CT and SM_Body_T with their materials. AShooterCharacter shows them on the
other players' pawns ([/Script/ShooterGame.ShooterCharacter] CTBodyMeshName / TBodyMeshName) until rigged T and CT
models, made in Blender, replace them.

A body stands on its origin (the character's feet), faces +X (a dark visor on the head shows the facing) and fits the
capsule (radius 40 cm, 183 cm tall): legs, a torso and a head, in the team's colour. Blender's axes, metres: X forward,
Blender -Y the engine's right, Z up. Only Blender's own modules are used (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import os

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))

TEAMS = {
    "CT": (0.18, 0.30, 0.62),
    "T": (0.70, 0.45, 0.18),
}

# (centre, size) in metres: two legs, the torso, the head.
BODY_BOXES = [
    ((0.0, 0.12, 0.45), (0.22, 0.18, 0.90)),
    ((0.0, -0.12, 0.45), (0.22, 0.18, 0.90)),
    ((0.0, 0.0, 1.20), (0.30, 0.52, 0.62)),
    ((0.0, 0.0, 1.66), (0.26, 0.24, 0.30)),
]
VISOR_BOX = ((0.12, 0.0, 1.68), (0.06, 0.20, 0.10))


def make_material(name, rgb, roughness=0.6):
    material = bpy.data.materials.new(name)
    if material.node_tree is None:  # Blender 5 makes the node tree itself (use_nodes goes away in 6.0)
        material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = 0.0
    return material


def add_boxes(bm, boxes, material_index):
    for center, size in boxes:
        result = bmesh.ops.create_cube(bm, size=1.0, calc_uvs=True)
        verts = result["verts"]
        bmesh.ops.scale(bm, vec=Vector(size), verts=verts)
        bmesh.ops.translate(bm, vec=Vector(center), verts=verts)
        for face in {face for vert in verts for face in vert.link_faces}:
            face.material_index = material_index


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    visor = make_material("Visor", (0.05, 0.05, 0.06), 0.3)
    bodies = {}
    for team, rgb in TEAMS.items():
        mesh = bpy.data.meshes.new("Body_" + team)
        bm = bmesh.new()
        bm.loops.layers.uv.new()
        add_boxes(bm, BODY_BOXES, 0)
        add_boxes(bm, [VISOR_BOX], 1)
        bm.to_mesh(mesh)
        bm.free()
        mesh.materials.append(make_material("Team" + team, rgb))
        mesh.materials.append(visor)
        obj = bpy.data.objects.new("Body_" + team, mesh)
        scene.collection.objects.link(obj)
        bodies[team] = obj
    return bodies


def export(bodies):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "team_bodies.blend"))
    for team, obj in bodies.items():
        for other in bodies.values():
            other.select_set(other is obj)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.gltf(
            filepath=os.path.join(HERE, "Body_%s.glb" % team),
            export_format="GLB",
            export_yup=True,
            export_apply=True,
            export_extras=False,
            export_lights=False,
            export_cameras=False,
            export_materials="EXPORT",
            use_selection=True,
        )


if __name__ == "__main__":
    export(build())
    print("make_team_bodies: wrote the team bodies in %s" % HERE)
