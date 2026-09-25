"""Writes AxisTest.glb, the engine's map for spotting mirrored or swapped axes (Leon, plan phase P15).

Standard-library Python only, so the source art is reproducible without Blender:

    python Engine/SourceArt/Maps/MakeAxisTest.py

The scene, in glTF's own frame (right-handed, +Y up, metres). The map importer converts it to the engine's frame
(X forward, Y right, Z up, left-handed, cm): a glTF point (x, y, z) m lands at (x, z, y) * 100 cm.

    node           glTF position (m)   engine position (cm)   what it shows
    Floor          (0, 0, 0)           (0, 0, 0)              a grey 12 m x 12 m floor
    Origin_White   (0, 0.2, 0)         (0, 0, 20)             a white 40 cm cube on the origin
    AxisX_Red      (3, 0.5, 0)         (300, 0, 50)           a red 1 m cube on +X, 3 m away
    AxisY_Green    (0, 0.5, 2)         (0, 200, 50)           a green 1 m cube on +Y, 2 m away
    AxisZ_Blue     (0, 2.5, 0)         (0, 0, 250)            a blue 1 m cube 2.5 m up, on +Z
    Marker_1m      (1, 0, 0)           (100, 0, 0)            a yellow 20 cm cube exactly 1 m along +X
    PlayerStart    (-6, 1.7, 0)        (-600, 0, 170)         the start, facing +X (no rotation)
    Sun            (0, 10, 0)          (0, 1000, 0)           a white directional light, from behind the start

Every distance differs, so a swapped pair of axes moves a cube to another place, and a mirror puts it on the other
side: from the start, looking along +X with +Z up, red is straight ahead, green on the RIGHT and blue up.
"""
import json
import math
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))


class GltfWriter:
    """A tiny glTF 2.0 writer: box and plane meshes with normals and UVs, PBR materials, nodes, punctual lights."""

    def __init__(self):
        self.buffer = bytearray()
        self.accessors = []
        self.buffer_views = []
        self.meshes = []
        self.materials = []
        self.nodes = []
        self.lights = []

    def _view(self, data, target):
        while len(self.buffer) % 4:
            self.buffer.append(0)
        self.buffer_views.append({"buffer": 0, "byteOffset": len(self.buffer), "byteLength": len(data),
                                  "target": target})
        self.buffer.extend(data)
        return len(self.buffer_views) - 1

    def _accessor(self, view, component, count, kind, minimum=None, maximum=None):
        accessor = {"bufferView": view, "componentType": component, "count": count, "type": kind}
        if minimum is not None:
            accessor["min"] = minimum
            accessor["max"] = maximum
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def material(self, name, color, metallic=0.0, roughness=0.8):
        self.materials.append({"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": [color[0], color[1], color[2], 1.0], "metallicFactor": metallic,
            "roughnessFactor": roughness}})
        return len(self.materials) - 1

    def _mesh(self, name, faces, material):
        positions, normals, uvs, indices = [], [], [], []
        for center, normal, u, v in faces:
            base = len(positions)
            for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                positions.append([center[i] + su * u[i] + sv * v[i] for i in range(3)])
                normals.append(list(normal))
                uvs.append([(su + 1) * 0.5, (1 - sv) * 0.5])
            indices += [base, base + 1, base + 2, base, base + 2, base + 3]
        pos = self._view(b"".join(struct.pack("<3f", *p) for p in positions), 34962)
        nrm = self._view(b"".join(struct.pack("<3f", *n) for n in normals), 34962)
        uv = self._view(b"".join(struct.pack("<2f", *t) for t in uvs), 34962)
        idx = self._view(b"".join(struct.pack("<H", i) for i in indices), 34963)
        lo = [min(p[i] for p in positions) for i in range(3)]
        hi = [max(p[i] for p in positions) for i in range(3)]
        attributes = {"POSITION": self._accessor(pos, 5126, len(positions), "VEC3", lo, hi),
                      "NORMAL": self._accessor(nrm, 5126, len(normals), "VEC3"),
                      "TEXCOORD_0": self._accessor(uv, 5126, len(uvs), "VEC2")}
        self.meshes.append({"name": name, "primitives": [{
            "attributes": attributes, "indices": self._accessor(idx, 5123, len(indices), "SCALAR"),
            "material": material}]})
        return len(self.meshes) - 1

    def box(self, name, half, material):
        """A box of half sizes half = (x, y, z) m around the origin; each face's (u, v) turns counter-clockwise."""
        hx, hy, hz = half
        faces = [((hx, 0, 0), (1, 0, 0), (0, hy, 0), (0, 0, hz)), ((-hx, 0, 0), (-1, 0, 0), (0, 0, hz), (0, hy, 0)),
                 ((0, hy, 0), (0, 1, 0), (0, 0, hz), (hx, 0, 0)), ((0, -hy, 0), (0, -1, 0), (hx, 0, 0), (0, 0, hz)),
                 ((0, 0, hz), (0, 0, 1), (hx, 0, 0), (0, hy, 0)), ((0, 0, -hz), (0, 0, -1), (0, hy, 0), (hx, 0, 0))]
        return self._mesh(name, faces, material)

    def plane(self, name, half, material):
        """A floor of half sizes (x, z) m at y = 0, facing +Y."""
        return self._mesh(name, [((0, 0, 0), (0, 1, 0), (0, 0, half[1]), (half[0], 0, 0))], material)

    def light(self, light):
        self.lights.append(light)
        return len(self.lights) - 1

    def node(self, name, translation=None, rotation=None, scale=None, mesh=None, light=None, extras=None,
             children=None):
        node = {"name": name}
        if translation is not None:
            node["translation"] = list(translation)
        if rotation is not None:
            node["rotation"] = list(rotation)
        if scale is not None:
            node["scale"] = list(scale)
        if mesh is not None:
            node["mesh"] = mesh
        if light is not None:
            node["extensions"] = {"KHR_lights_punctual": {"light": light}}
        if extras is not None:
            node["extras"] = extras
        if children is not None:
            node["children"] = children
        self.nodes.append(node)
        return len(self.nodes) - 1

    def document(self, roots, buffer_uri=None):
        doc = {"asset": {"version": "2.0", "generator": "LeonEngine " + os.path.basename(__file__)},
               "scene": 0, "scenes": [{"nodes": roots}], "nodes": self.nodes, "meshes": self.meshes,
               "materials": self.materials, "accessors": self.accessors, "bufferViews": self.buffer_views,
               "buffers": [{"byteLength": len(self.buffer)}]}
        if buffer_uri is not None:
            doc["buffers"][0]["uri"] = buffer_uri
        if self.lights:
            doc["extensionsUsed"] = ["KHR_lights_punctual"]
            doc["extensions"] = {"KHR_lights_punctual": {"lights": self.lights}}
        return doc

    def write_glb(self, path, roots):
        text = json.dumps(self.document(roots), separators=(",", ":")).encode("utf-8")
        text += b" " * (-len(text) % 4)
        binary = bytes(self.buffer) + b"\0" * (-len(self.buffer) % 4)
        total = 12 + 8 + len(text) + 8 + len(binary)
        with open(path, "wb") as f:
            f.write(struct.pack("<III", 0x46546C67, 2, total))
            f.write(struct.pack("<II", len(text), 0x4E4F534A) + text)
            f.write(struct.pack("<II", len(binary), 0x004E4942) + binary)


def rotation_from_to(a, b):
    """The unit quaternion (x, y, z, w) that turns the unit vector a onto the unit vector b."""
    cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    w = 1.0 + sum(a[i] * b[i] for i in range(3))
    length = math.sqrt(sum(c * c for c in cross) + w * w)
    return [cross[0] / length, cross[1] / length, cross[2] / length, w / length]


def normalized(v):
    length = math.sqrt(sum(c * c for c in v))
    return [c / length for c in v]


def main():
    gltf = GltfWriter()
    grey = gltf.material("Grey", (0.45, 0.45, 0.45))
    white = gltf.material("White", (0.95, 0.95, 0.95))
    red = gltf.material("Red", (0.85, 0.1, 0.1))
    green = gltf.material("Green", (0.1, 0.75, 0.15))
    blue = gltf.material("Blue", (0.1, 0.25, 0.9))
    yellow = gltf.material("Yellow", (0.95, 0.85, 0.1))
    floor = gltf.plane("Floor", (6.0, 6.0), grey)
    origin = gltf.box("OriginCube", (0.2, 0.2, 0.2), white)
    red_cube = gltf.box("RedCube", (0.5, 0.5, 0.5), red)
    green_cube = gltf.box("GreenCube", (0.5, 0.5, 0.5), green)
    blue_cube = gltf.box("BlueCube", (0.5, 0.5, 0.5), blue)
    marker = gltf.box("MarkerCube", (0.1, 0.1, 0.1), yellow)
    # The sun shines along its node's -Z: down, toward +X and +Y of the engine (glTF +X and +Z).
    sun_direction = normalized((0.45, -1.0, 0.3))
    sun = gltf.light({"name": "Sun", "type": "directional", "color": [1.0, 0.97, 0.92], "intensity": 1.0})
    roots = [
        gltf.node("Floor", mesh=floor),
        gltf.node("Origin_White", translation=(0, 0.2, 0), mesh=origin),
        gltf.node("AxisX_Red", translation=(3, 0.5, 0), mesh=red_cube),
        gltf.node("AxisY_Green", translation=(0, 0.5, 2), mesh=green_cube),
        gltf.node("AxisZ_Blue", translation=(0, 2.5, 0), mesh=blue_cube),
        gltf.node("Marker_1m", translation=(1, 0, 0), mesh=marker),
        gltf.node("PlayerStart", translation=(-6, 1.7, 0)),
        gltf.node("Sun", translation=(0, 10, 0), rotation=rotation_from_to((0, 0, -1), sun_direction), light=sun),
    ]
    path = os.path.join(HERE, "AxisTest.glb")
    gltf.write_glb(path, roots)
    print("wrote", path, os.path.getsize(path), "bytes")


if __name__ == "__main__":
    main()
