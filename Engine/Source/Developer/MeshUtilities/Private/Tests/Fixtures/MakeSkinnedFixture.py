"""Writes Cube.glb and SkinnedArm.glb, the glTF import's test fixtures (Leon, ps2-shipping N21).

Standard-library Python only, deterministic (the same bytes on every run; the PNG is stored, not deflated, so no zlib
version changes it):

    python Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/MakeSkinnedFixture.py

Both are in glTF's frame (right-handed, +Y up, metres; the engine's position is (x, z, y) * 100 cm).

Cube.glb: a 2 m cube centred on the origin, 24 vertices (4 a face, each face's normal), 12 triangles counter-clockwise
around their outward normals, no material: the static import's identity fixture (the one Cube.obj was).

SkinnedArm.glb: a 3-bone arm along +X, skinned:

    Armature                 (0.1, 0, 0), not a joint: its transform is part of the root bone's
      Shoulder               (0, 1, 0)
        Elbow                (0.5, 0, 0)
          Hand               (0.4, 0, 0)
            SOCKET_Grip      (0.1, 0, 0.05), 90 degrees about +Z
    ArmMesh                  the mesh, skinned with the skin (joints listed Hand, Shoulder, Elbow: not parents first)

  The mesh is a square tube of 5 rings (x = 0.1, 0.35, 0.6, 0.8, 1.0 in the world, 0.1 m wide) with UVs and outward
  normals; the rings' weights (JOINTS_0 / WEIGHTS_0, skin joint indices):
    ring 0: Shoulder 1
    ring 1: Shoulder 0.6, Elbow 0.4
    ring 2: Elbow 0.5, Shoulder 0.3, Hand 0.2               -> the two largest: Elbow 0.625, Shoulder 0.375
    ring 3: Elbow 0.5, Hand 0.3, Shoulder 0.1, Shoulder 0.1 -> Elbow 0.625, Hand 0.375 (Shoulder merges to 0.2)
    ring 4: Hand 2 (not normalized)                          -> Hand 1
  Its material ArmSkin samples an 8x8 PNG embedded in the binary chunk (image "ArmSkin_D").
  The inverse bind matrices are the inverse of each joint's world matrix at rest.

  Animations:
    Wave (LINEAR): Elbow's rotation 0, 90, 0 degrees about +Z at 0, 0.5, 1 s; Shoulder's translation from (0, 1, 0)
                   to (0, 1.2, 0) over 1 s. Its extras carry two notifies (ps2-shipping N25, Docs/ASSET_FORMATS.md):
                   Footstep_L at 0.25 s and Footstep_R at 0.75 s.
    Grip (STEP): Hand's rotation identity, 45 degrees about +X, identity at 0, 0.5, 1 s; (LINEAR) Hand's scale 1 to
                 1.5 over 1 s. Its extras say it does not loop (ps2-shipping N27: {"loop": 0}); Wave, without the
                 flag, loops.
"""
import json
import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

FLOAT = 5126
UNSIGNED_BYTE = 5121
UNSIGNED_SHORT = 5123
ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963


class GlbWriter:
    """Accessors and buffer views over one binary chunk."""

    def __init__(self):
        self.buffer = bytearray()
        self.buffer_views = []
        self.accessors = []

    def view(self, data, target=None):
        while len(self.buffer) % 4:
            self.buffer.append(0)
        view = {"buffer": 0, "byteOffset": len(self.buffer), "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        self.buffer += data
        self.buffer_views.append(view)
        return len(self.buffer_views) - 1

    def accessor(self, values, kind, component, target=None, minmax=False):
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[kind]
        fmt = {FLOAT: "<f", UNSIGNED_BYTE: "<B", UNSIGNED_SHORT: "<H"}[component]
        flat = [v for value in values for v in (value if isinstance(value, (list, tuple)) else [value])]
        data = b"".join(struct.pack(fmt, v) for v in flat)
        accessor = {"bufferView": self.view(data, target), "componentType": component, "count": len(values),
                    "type": kind}
        if minmax:
            columns = [[flat[i * width + c] for i in range(len(values))] for c in range(width)]
            accessor["min"] = [min(column) for column in columns]
            accessor["max"] = [max(column) for column in columns]
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def write(self, path, document):
        document["buffers"] = [{"byteLength": len(self.buffer)}]
        document["bufferViews"] = self.buffer_views
        document["accessors"] = self.accessors
        text = json.dumps(document, separators=(",", ":"), sort_keys=True).encode("utf-8")
        text += b" " * ((4 - len(text) % 4) % 4)
        binary = bytes(self.buffer) + b"\0" * ((4 - len(self.buffer) % 4) % 4)
        glb = struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(binary))
        glb += struct.pack("<I4s", len(text), b"JSON") + text
        glb += struct.pack("<I4s", len(binary), b"BIN\0") + binary
        with open(path, "wb") as f:
            f.write(glb)


def png_bytes(size):
    """A size x size RGB checker, its IDAT stored (deflate level 0)."""
    colors = [(200, 60, 40), (240, 220, 180)]
    rows = b""
    for y in range(size):
        rows += b"\0" + b"".join(bytes(colors[((x // 2) + (y // 2)) % 2]) for x in range(size))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 0)) + chunk(b"IEND", b""))


# Cube.glb ------------------------------------------------------------------------------------------------------------

CUBE_FACES = [
    ((0, 0, -1), [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)]),
    ((0, 0, 1), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
    ((-1, 0, 0), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
    ((1, 0, 0), [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
    ((0, -1, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
    ((0, 1, 0), [(-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)]),
]


def write_cube():
    glb = GlbWriter()
    positions, normals, indices = [], [], []
    for face, (normal, corners) in enumerate(CUBE_FACES):
        base = face * 4
        positions += [list(map(float, corner)) for corner in corners]
        normals += [list(map(float, normal))] * 4
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    document = {
        "asset": {"version": "2.0", "generator": "MakeSkinnedFixture.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Cube", "mesh": 0}],
        "meshes": [{"name": "Cube", "primitives": [{
            "attributes": {"POSITION": glb.accessor(positions, "VEC3", FLOAT, ARRAY_BUFFER, minmax=True),
                           "NORMAL": glb.accessor(normals, "VEC3", FLOAT, ARRAY_BUFFER)},
            "indices": glb.accessor(indices, "SCALAR", UNSIGNED_SHORT, ELEMENT_ARRAY_BUFFER)}]}],
    }
    glb.write(os.path.join(HERE, "Cube.glb"), document)


# SkinnedArm.glb ------------------------------------------------------------------------------------------------------

def quat_axis_angle(axis, degrees):
    half = math.radians(degrees) * 0.5
    s = math.sin(half)
    return [axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half)]


def translation_matrix(x, y, z):
    """Column-major, as glTF stores matrices."""
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1]


RING_X = [0.1, 0.35, 0.6, 0.8, 1.0]
# (joint, weight) x 4 per ring; skin joints: 0 Hand, 1 Shoulder, 2 Elbow.
RING_WEIGHTS = [
    [(1, 1.0), (0, 0.0), (0, 0.0), (0, 0.0)],
    [(1, 0.6), (2, 0.4), (0, 0.0), (0, 0.0)],
    [(2, 0.5), (1, 0.3), (0, 0.2), (0, 0.0)],
    [(2, 0.5), (0, 0.3), (1, 0.1), (1, 0.1)],
    [(0, 2.0), (0, 0.0), (0, 0.0), (0, 0.0)],
]
HALF_WIDTH = 0.05
SHOULDER_Y = 1.0


def write_arm():
    glb = GlbWriter()
    corners = [(1, 1), (-1, 1), (-1, -1), (1, -1)]  # (y, z) signs around +X, counter-clockwise seen from +X
    positions, normals, uvs, joints, weights = [], [], [], [], []
    for ring, x in enumerate(RING_X):
        for corner, (sy, sz) in enumerate(corners):
            positions.append([x, SHOULDER_Y + sy * HALF_WIDTH, sz * HALF_WIDTH])
            length = math.sqrt(2.0)
            normals.append([0.0, sy / length, sz / length])
            uvs.append([corner / 4.0, ring / 4.0])
            joints.append([joint for joint, _ in RING_WEIGHTS[ring]])
            weights.append([weight for _, weight in RING_WEIGHTS[ring]])
    indices = []
    for ring in range(len(RING_X) - 1):
        for corner in range(4):
            a = ring * 4 + corner
            b = ring * 4 + (corner + 1) % 4
            c = a + 4
            d = b + 4
            # Counter-clockwise seen from outside the tube.
            indices += [a, b, c, b, d, c]

    image_view = glb.view(png_bytes(8))
    position_accessor = glb.accessor(positions, "VEC3", FLOAT, ARRAY_BUFFER, minmax=True)
    normal_accessor = glb.accessor(normals, "VEC3", FLOAT, ARRAY_BUFFER)
    uv_accessor = glb.accessor(uvs, "VEC2", FLOAT, ARRAY_BUFFER)
    joint_accessor = glb.accessor(joints, "VEC4", UNSIGNED_BYTE, ARRAY_BUFFER)
    weight_accessor = glb.accessor(weights, "VEC4", FLOAT, ARRAY_BUFFER)
    index_accessor = glb.accessor(indices, "SCALAR", UNSIGNED_SHORT, ELEMENT_ARRAY_BUFFER)

    # Joint worlds at rest (translations only): Hand, Shoulder, Elbow.
    shoulder = (0.1, SHOULDER_Y, 0.0)
    elbow = (0.6, SHOULDER_Y, 0.0)
    hand = (1.0, SHOULDER_Y, 0.0)
    inverse_bind = [translation_matrix(-p[0], -p[1], -p[2]) for p in (hand, shoulder, elbow)]
    inverse_bind_accessor = glb.accessor(inverse_bind, "MAT4", FLOAT)

    def times(values):
        return glb.accessor(values, "SCALAR", FLOAT, minmax=True)

    wave_times = times([0.0, 0.5, 1.0])
    wave_elbow = glb.accessor([quat_axis_angle((0, 0, 1), a) for a in (0.0, 90.0, 0.0)], "VEC4", FLOAT)
    wave_shoulder_times = times([0.0, 1.0])
    wave_shoulder = glb.accessor([[0.0, 1.0, 0.0], [0.0, 1.2, 0.0]], "VEC3", FLOAT)
    grip_times = times([0.0, 0.5, 1.0])
    grip_hand = glb.accessor([quat_axis_angle((1, 0, 0), a) for a in (0.0, 45.0, 0.0)], "VEC4", FLOAT)
    grip_scale_times = times([0.0, 1.0])
    grip_scale = glb.accessor([[1.0, 1.0, 1.0], [1.5, 1.5, 1.5]], "VEC3", FLOAT)

    # Nodes: 0 Armature, 1 Shoulder, 2 Elbow, 3 Hand, 4 SOCKET_Grip, 5 ArmMesh.
    document = {
        "asset": {"version": "2.0", "generator": "MakeSkinnedFixture.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, 5]}],
        "nodes": [
            {"name": "Armature", "translation": [0.1, 0.0, 0.0], "children": [1]},
            {"name": "Shoulder", "translation": [0.0, SHOULDER_Y, 0.0], "children": [2]},
            {"name": "Elbow", "translation": [0.5, 0.0, 0.0], "children": [3]},
            {"name": "Hand", "translation": [0.4, 0.0, 0.0], "children": [4]},
            {"name": "SOCKET_Grip", "translation": [0.1, 0.0, 0.05], "rotation": quat_axis_angle((0, 0, 1), 90.0)},
            {"name": "ArmMesh", "mesh": 0, "skin": 0},
        ],
        "skins": [{"name": "Armature", "joints": [3, 1, 2], "skeleton": 1,
                   "inverseBindMatrices": inverse_bind_accessor}],
        "meshes": [{"name": "Arm", "primitives": [{
            "attributes": {"POSITION": position_accessor, "NORMAL": normal_accessor, "TEXCOORD_0": uv_accessor,
                           "JOINTS_0": joint_accessor, "WEIGHTS_0": weight_accessor},
            "indices": index_accessor, "material": 0}]}],
        "materials": [{"name": "ArmSkin", "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0], "baseColorTexture": {"index": 0},
            "metallicFactor": 0.0, "roughnessFactor": 1.0}}],
        "textures": [{"source": 0}],
        "images": [{"name": "ArmSkin_D", "mimeType": "image/png", "bufferView": image_view}],
        "animations": [
            {"name": "Wave",
             "extras": {"notifies": [{"name": "Footstep_L", "time": 0.25}, {"name": "Footstep_R", "time": 0.75}]},
             "samplers": [{"input": wave_times, "output": wave_elbow, "interpolation": "LINEAR"},
                          {"input": wave_shoulder_times, "output": wave_shoulder, "interpolation": "LINEAR"}],
             "channels": [{"sampler": 0, "target": {"node": 2, "path": "rotation"}},
                          {"sampler": 1, "target": {"node": 1, "path": "translation"}}]},
            {"name": "Grip",
             "extras": {"loop": 0},
             "samplers": [{"input": grip_times, "output": grip_hand, "interpolation": "STEP"},
                          {"input": grip_scale_times, "output": grip_scale, "interpolation": "LINEAR"}],
             "channels": [{"sampler": 0, "target": {"node": 3, "path": "rotation"}},
                          {"sampler": 1, "target": {"node": 3, "path": "scale"}}]},
        ],
    }
    glb.write(os.path.join(HERE, "SkinnedArm.glb"), document)


def main():
    write_cube()
    write_arm()
    print("wrote Cube.glb and SkinnedArm.glb in", HERE)


if __name__ == "__main__":
    main()
