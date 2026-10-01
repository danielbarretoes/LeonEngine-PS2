"""Generates the maps' skies as high dynamic range environments for LeonEd's cube map import: de_leon's desert day
and de_harbor's hazy coast at the end of the afternoon.

    python Game/ShooterGame/SourceArt/Sky/make_sky.py [--out <folder>]

Writes Sky_Desert.hdr and Sky_Coast.hdr next to this script (`--out` writes there instead), which ImportList.ini
imports as the cube maps /Game/Sky/T_Sky_Desert and /Game/Sky/T_Sky_Coast (UTextureCubeFactory: six faces, tone-mapped;
Docs/ART_PIPELINE.md, "The sky"). The maps' world settings name them (Maps/make_de_leon.py's and
Maps/make_de_harbor.py's WorldSettings nodes). Each sky is a preset (PRESETS) of the same painting: its colours, its
haze, its clouds and the map script whose sun it shows.

The image is a long-lat (equirectangular) panorama of WIDTH x HEIGHT texels in the engine's axes (X north, Y east, Z up;
Docs/ASSET_FORMATS.md, "Cube maps"): column x looks at the yaw (x + 0.5) / WIDTH x 360 - 180 degrees (the middle column
along +X, the yaw growing toward +Y), row y (the top row first, as Radiance stores it) at the pitch 90 - (y + 0.5) /
HEIGHT x 180 degrees. Its texels are linear radiance, 1.0 a sky at the horizon's brightness, stored as Radiance RGBE
(`.hdr`, run-length encoded scanlines, no date or software line in the header).

What it paints:

- the sky's gradient: a deep blue zenith fading to a pale haze at the horizon (warm in the desert, a cool grey on the
  coast, where it climbs higher), brighter toward the sun's side;
- below the horizon, the haze darkening to the colour of distant sand, or of the sea (the map's floor hides most of it);
- the sun, where the map's baked sun is (SUN_DIRECTION, read from the preset's map script): a disc 1.5 degrees in
  radius, far brighter than the sky (the tone mapping clips it to white), and its glow;
- soft clouds from seeded value noise (a fixed integer hash, five octaves) on a plane over the map, thinning toward the
  horizon and lit from the sun's side.

Every value comes from integer hashes and fixed formulas, so two runs write the same bytes
(Game/ShooterGame/SourceArt/check_art_determinism.py compares them with the committed file). Only the Python standard
library is used; no external art (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import ast
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# The panorama: 1024 x 512 gives the cube map's 256-texel faces a texel of the source for each of theirs.
WIDTH = 1024
HEIGHT = 512

# The desert's preset (de_leon). Linear radiance (1.0: the horizon's haze on the side away from the sun).
ZENITH = (0.10, 0.24, 0.62)
HORIZON = (0.92, 0.88, 0.80)
# How fast the zenith's blue gives way to the haze (a larger power keeps the haze lower).
HAZE_POWER = 3.2
# The horizon toward the sun is this much brighter.
SUN_SIDE_BRIGHTNESS = 0.35
SAND = (0.46, 0.36, 0.25)

SUN_COLOR = (1.0, 0.92, 0.78)
SUN_RADIUS_DEGREES = 1.5
SUN_RADIANCE = 60.0
# The glow around the sun: two falloffs by the angle to it (radians).
GLOW_NEAR = (2.4, 0.07)
GLOW_FAR = (0.45, 0.45)

CLOUD_SEED = 1609
CLOUD_OCTAVES = 5
# The noise's cells a unit of the cloud plane (the plane one unit above the eye).
CLOUD_FREQUENCY = 3.5
# The noise's point straight above the map: a clear patch, so the sun's side of the zenith is open sky.
CLOUD_OFFSET = (0.5, 1.5)
# The noise's values that start and fill a cloud.
CLOUD_COVER = (0.56, 0.80)
CLOUD_LIT = (1.30, 1.26, 1.20)
CLOUD_SHADE = (0.62, 0.64, 0.70)

# The skies: (file, map script, the values above by name). The coast (de_harbor): a greyer zenith, a cool haze that
# climbs higher, a warm glow toward the low sun, the sea below the horizon, more clouds from another seed.
DESERT = {
    "ZENITH": ZENITH, "HORIZON": HORIZON, "HAZE_POWER": HAZE_POWER, "SUN_SIDE_BRIGHTNESS": SUN_SIDE_BRIGHTNESS,
    "GROUND": SAND, "SUN_COLOR": SUN_COLOR, "SUN_RADIANCE": SUN_RADIANCE, "GLOW_NEAR": GLOW_NEAR, "GLOW_FAR": GLOW_FAR,
    "CLOUD_SEED": CLOUD_SEED, "CLOUD_FREQUENCY": CLOUD_FREQUENCY, "CLOUD_OFFSET": CLOUD_OFFSET,
    "CLOUD_COVER": CLOUD_COVER, "CLOUD_LIT": CLOUD_LIT, "CLOUD_SHADE": CLOUD_SHADE,
}
COAST = dict(DESERT, **{
    "ZENITH": (0.14, 0.27, 0.52), "HORIZON": (0.84, 0.86, 0.88), "HAZE_POWER": 2.2, "SUN_SIDE_BRIGHTNESS": 0.7,
    "GROUND": (0.16, 0.25, 0.28), "SUN_COLOR": (1.0, 0.82, 0.6), "GLOW_NEAR": (2.8, 0.08), "GLOW_FAR": (0.6, 0.5),
    "CLOUD_SEED": 2711, "CLOUD_FREQUENCY": 3.0, "CLOUD_OFFSET": (2.5, -1.5), "CLOUD_COVER": (0.50, 0.78),
    "CLOUD_LIT": (1.30, 1.16, 1.0), "CLOUD_SHADE": (0.58, 0.61, 0.68),
})
PRESETS = [
    ("Sky_Desert.hdr", "make_de_leon.py", DESERT),
    ("Sky_Coast.hdr", "make_de_harbor.py", COAST),
]


def read_sun_direction(map_script):
    """SUN_DIRECTION of a map script in Maps/ (where the sun's light travels, engine axes), normalized."""
    with open(os.path.join(HERE, os.pardir, "Maps", map_script), encoding="utf-8") as source:
        tree = ast.parse(source.read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", None) == "SUN_DIRECTION" for t in node.targets):
            x, y, z = ast.literal_eval(node.value)
            length = math.sqrt(x * x + y * y + z * z)
            return (x / length, y / length, z / length)
    raise ValueError("Maps/%s has no SUN_DIRECTION" % map_script)


def hash01(x, y, seed):
    """A value in [0, 1) for integer coordinates (leon_art.noise's hash)."""
    h = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFF) / 65536.0


def value_noise(x, y, seed):
    """Smooth value noise in [0, 1) at a point of the plane (cells of one unit, smoothstep between the corners)."""
    cx = math.floor(x)
    cy = math.floor(y)
    fx = x - cx
    fy = y - cy
    fx = fx * fx * (3.0 - 2.0 * fx)
    fy = fy * fy * (3.0 - 2.0 * fy)
    cx = int(cx)
    cy = int(cy)
    a = hash01(cx, cy, seed)
    b = hash01(cx + 1, cy, seed)
    c = hash01(cx, cy + 1, seed)
    d = hash01(cx + 1, cy + 1, seed)
    top = a + (b - a) * fx
    bottom = c + (d - c) * fx
    return top + (bottom - top) * fy


def fbm(x, y, seed):
    """Five octaves of value noise, each twice the frequency and half the weight of the one before, in [0, 1)."""
    total = 0.0
    weight = 0.5
    norm = 0.0
    for octave in range(CLOUD_OCTAVES):
        total += weight * value_noise(x, y, seed + octave * 97)
        norm += weight
        x = x * 2.03 + 17.1
        y = y * 2.03 - 5.3
        weight *= 0.5
    return total / norm


def smoothstep(edge0, edge1, x):
    t = min(max((x - edge0) / (edge1 - edge0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def radiance(direction, to_sun, p):
    """The sky's linear radiance (r, g, b) along a unit direction, with the preset p's values."""
    dx, dy, dz = direction
    cos_sun = dx * to_sun[0] + dy * to_sun[1] + dz * to_sun[2]
    angle = math.acos(min(max(cos_sun, -1.0), 1.0))
    # The side of the sky toward the sun's azimuth, 0 to 1.
    horizontal = math.sqrt(dx * dx + dy * dy)
    sun_horizontal = math.sqrt(to_sun[0] * to_sun[0] + to_sun[1] * to_sun[1])
    facing = 0.0
    if horizontal > 1e-6 and sun_horizontal > 1e-6:
        facing = max((dx * to_sun[0] + dy * to_sun[1]) / (horizontal * sun_horizontal), 0.0)
    horizon = tuple(c * (1.0 + p["SUN_SIDE_BRIGHTNESS"] * facing * facing) for c in p["HORIZON"])

    if dz >= 0.0:
        sky = lerp(horizon, p["ZENITH"], 1.0 - (1.0 - dz) ** p["HAZE_POWER"])
    else:
        sky = lerp(horizon, p["GROUND"], smoothstep(0.0, 0.18, -dz))

    glow_near, glow_far, sun_color = p["GLOW_NEAR"], p["GLOW_FAR"], p["SUN_COLOR"]
    glow = glow_near[0] * math.exp(-angle / glow_near[1]) + glow_far[0] * math.exp(-angle / glow_far[1])
    sky = tuple(sky[i] + sun_color[i] * glow for i in range(3))

    cloud = 0.0
    if dz > 0.0:
        # The cloud plane one unit above the eye, seen along the direction; thinner toward the horizon.
        scale = p["CLOUD_FREQUENCY"] / (dz + 0.08)
        offset = p["CLOUD_OFFSET"]
        noise = fbm(offset[0] + dx * scale, offset[1] + dy * scale, p["CLOUD_SEED"])
        density = smoothstep(p["CLOUD_COVER"][0], p["CLOUD_COVER"][1], noise)
        cloud = density * smoothstep(0.03, 0.30, dz) * 0.9
    if cloud > 0.0:
        # Lit toward the sun, grey away from it; the glow shows through a thin cloud.
        lit = 0.5 + 0.5 * cos_sun
        colour = lerp(p["CLOUD_SHADE"], p["CLOUD_LIT"], lit)
        colour = tuple(colour[i] + sun_color[i] * glow * 0.35 for i in range(3))
        sky = lerp(sky, colour, cloud)

    if angle < math.radians(SUN_RADIUS_DEGREES):
        disc = p["SUN_RADIANCE"] * (1.0 - 0.6 * cloud)
        sky = tuple(max(sky[i], sun_color[i] * disc) for i in range(3))
    return sky


def to_rgbe(rgb):
    """Radiance's shared-exponent bytes of a colour (Greg Ward's float2rgbe)."""
    value = max(rgb)
    if value < 1e-32:
        return (0, 0, 0, 0)
    mantissa, exponent = math.frexp(value)
    scale = mantissa * 256.0 / value
    return (int(rgb[0] * scale), int(rgb[1] * scale), int(rgb[2] * scale), exponent + 128)


def encode_run_length(channel):
    """One channel of a scanline in Radiance's run-length encoding: runs of 3 to 127 equal bytes as (128 + count,
    byte), the rest as literals of up to 128 bytes (count, bytes)."""
    out = bytearray()
    literal = bytearray()
    index = 0
    count = len(channel)
    while index < count:
        run = 1
        while index + run < count and run < 127 and channel[index + run] == channel[index]:
            run += 1
        if run >= 3:
            if literal:
                out.append(len(literal))
                out += literal
                literal = bytearray()
            out.append(128 + run)
            out.append(channel[index])
            index += run
            continue
        literal.append(channel[index])
        index += 1
        if len(literal) == 128:
            out.append(len(literal))
            out += literal
            literal = bytearray()
    if literal:
        out.append(len(literal))
        out += literal
    return out


def write_hdr(path, width, height, rows):
    """A Radiance RGBE file of `rows` (top row first, each a list of RGBE tuples), run-length encoded."""
    header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n" % (height, width)
    data = bytearray(header.encode("ascii"))
    for row in rows:
        data += bytes((2, 2, width >> 8, width & 0xFF))
        for channel in range(4):
            data += encode_run_length(bytes(texel[channel] for texel in row))
    with open(path, "wb") as file:
        file.write(data)
    return len(data)


def main(argv):
    out = HERE
    if "--out" in argv and argv.index("--out") + 1 < len(argv):
        out = os.path.abspath(argv[argv.index("--out") + 1])
    os.makedirs(out, exist_ok=True)
    yaws = [math.radians((x + 0.5) / WIDTH * 360.0 - 180.0) for x in range(WIDTH)]
    columns = [(math.cos(yaw), math.sin(yaw)) for yaw in yaws]
    for name, map_script, preset in PRESETS:
        sun = read_sun_direction(map_script)
        to_sun = (-sun[0], -sun[1], -sun[2])
        rows = []
        for y in range(HEIGHT):
            pitch = math.radians(90.0 - (y + 0.5) / HEIGHT * 180.0)
            cos_pitch = math.cos(pitch)
            sin_pitch = math.sin(pitch)
            rows.append([to_rgbe(radiance((c * cos_pitch, s * cos_pitch, sin_pitch), to_sun, preset))
                         for c, s in columns])
        path = os.path.join(out, name)
        size = write_hdr(path, WIDTH, HEIGHT, rows)
        elevation = math.degrees(math.asin(to_sun[2]))
        azimuth = math.degrees(math.atan2(to_sun[1], to_sun[0]))
        print("make_sky: wrote %s (%d x %d, %d bytes), the sun at yaw %.1f, pitch %.1f degrees"
              % (path, WIDTH, HEIGHT, size, azimuth, elevation))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
