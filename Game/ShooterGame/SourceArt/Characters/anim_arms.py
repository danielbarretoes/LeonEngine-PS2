"""The first-person clips on leon_art.ARMS_BONES (Docs/ART_PIPELINE.md, "Animations"), posed by code: make_arms.py
calls add_clips on the arms' armature.

The arms hang from the eye (the armature's origin): the right hand's weapon socket (Weapon_R) holds the grip where a
weapon's pose puts it, the left hand holds the weapon's support or waits out of view below, both by two-bone IK
(anim_body's poses, the same socket convention). The shoulders move per weapon (the upper arms are off screen): the
left one forward and in to reach a rifle's handguard. The fingers close on the grip (fingers_01, thumb_01).

A weapon's clips (`<Weapon>_<Clip>`): Idle (a loop, the arms' base pose while it is drawn: a one-sample blend
space), Draw (up from below the view), Fire, Reload with MagOut and MagIn, and the grenade's PullPin and Throw, the
knife's Slash, the C4's Plant. Each clip starts and ends on its weapon's idle pose, so a montage blends in and out of
it without a jump.
"""

import math

from mathutils import Quaternion, Vector

import leon_art
from anim_body import lerp, smooth, track

B = leon_art.to_blender
Y_AXIS = (0.0, 1.0, 0.0)

RIFLE_SUPPORT = (0.27, 0.0, 0.065)
PISTOL_SUPPORT = (-0.005, -0.035, -0.025)
LEFT_AWAY = ("free", (0.05, -0.34, -0.62), (0.3, 0.2, -1.0), (0.2, 1.0, 0.0))

# The weapons' holds (engine metres from the eye): the grip, the weapon's forward and up, the left hand
# ("weapon", point, hand direction, thumb) in the weapon's space or ("free", point, direction, thumb) from the eye,
# the shoulders' moves and the fingers' curl.
HOLDS = {
    "Rifle": {"grip": (0.31, 0.15, -0.24), "forward": (1.0, -0.035, 0.035), "up": (0.0, 0.08, 1.0),
              "left": ("weapon", RIFLE_SUPPORT, (0.20, 0.95, 0.10), (0.95, -0.2, 0.25)),
              "shoulder_l": (0.23, 0.09, 0.03), "shoulder_r": (0.0, 0.0, 0.0), "curl": 75.0, "curl_l": 70.0},
    "Sniper": {"grip": (0.30, 0.15, -0.25), "forward": (1.0, -0.03, 0.03), "up": (0.0, 0.06, 1.0),
               "left": ("weapon", RIFLE_SUPPORT, (0.20, 0.95, 0.10), (0.95, -0.2, 0.25)),
               "shoulder_l": (0.23, 0.09, 0.03), "shoulder_r": (0.0, 0.0, 0.0), "curl": 75.0, "curl_l": 70.0},
    "Pistol": {"grip": (0.38, 0.14, -0.21), "forward": (1.0, -0.05, 0.03), "up": (0.0, 0.05, 1.0),
               "left": ("weapon", PISTOL_SUPPORT, (0.55, 0.25, -0.80), (0.60, -0.10, 0.80)),
               "shoulder_l": (0.12, 0.07, 0.02), "shoulder_r": (0.02, -0.01, 0.0), "curl": 80.0, "curl_l": 60.0},
    "Knife": {"grip": (0.34, 0.17, -0.23), "forward": (1.0, -0.20, 0.35), "up": (-0.3, 0.1, 1.0),
              "left": LEFT_AWAY, "shoulder_l": (0.0, 0.0, 0.0), "shoulder_r": (0.0, 0.0, 0.0), "curl": 85.0,
              "curl_l": 30.0},
    "Grenade": {"grip": (0.33, 0.16, -0.22), "forward": (1.0, -0.05, 0.40), "up": (-0.35, 0.0, 1.0),
                "left": LEFT_AWAY, "shoulder_l": (0.0, 0.0, 0.0), "shoulder_r": (0.0, 0.0, 0.0), "curl": 70.0,
                "curl_l": 30.0},
    "C4": {"grip": (0.36, 0.05, -0.32), "forward": (1.0, 0.0, 0.0), "up": (0.0, 0.0, 1.0),
           "left": ("weapon", (0.0, -0.13, 0.0), (0.2, 0.95, 0.0), (0.95, -0.2, 0.3)),
           "shoulder_l": (0.08, 0.06, 0.0), "shoulder_r": (0.02, -0.02, 0.0), "curl": 40.0, "curl_l": 40.0},
}


class Arms:
    """The arms' rest facts in Blender's axes: the socket, the hands' directions."""

    def __init__(self, rig):
        self.rig = rig
        _name, _bone, location, axes = leon_art.ARMS_SOCKETS[0]
        self.socket = B(*location)
        self.socket_frame = leon_art._frame_matrix(B(*axes[0]), B(*axes[1]))
        self.hand_head = rig.head["hand_r"]
        self.left_dir = (rig.tail["hand_l"] - rig.head["hand_l"]).normalized()
        self.left_thumb = B(0.0, 0.0, 1.0)


def weapon_frame(forward, up):
    return leon_art._frame_matrix(B(*forward), B(*up))


def curl(pose, side, fingers, thumb):
    """Closes a hand: the fingers turn about the hand's thumb-side axis toward the palm, the thumb across it."""
    s = 1.0 if side == "r" else -1.0
    # The palm faces in (the right hand's to the left): the fingers bend about the up axis toward it.
    pose.rotate("fingers_01_" + side, (0.0, 0.0, 1.0), s * fingers)
    pose.rotate("thumb_01_" + side, (1.0, 0.0, 0.0), -s * thumb)


def place_right(pose, arms, grip_b, frame_b):
    hand = (frame_b @ arms.socket_frame.transposed()).to_quaternion()
    wrist = grip_b - hand @ (arms.socket - arms.hand_head)
    pose.reach("upperarm_r", "lowerarm_r", "hand_r", wrist, B(-0.4, 0.5, -1.0))
    pose.orient("hand_r", hand)


def place_left(pose, arms, center_b, dir_b, thumb_b):
    rotation = leon_art.frame_rotation(arms.left_dir, arms.left_thumb, dir_b, thumb_b)
    palm = rotation @ (arms.left_dir.cross(arms.left_thumb)).normalized()
    direction = rotation @ arms.left_dir
    wrist = center_b - direction * 0.05 - palm * 0.025
    pose.reach("upperarm_l", "lowerarm_l", "hand_l", wrist, B(-0.4, -0.6, -1.0))
    pose.orient("hand_l", rotation)


def hold(pose, arms, name, grip_offset=(0.0, 0.0, 0.0), turn=(), left=None, curl_r=None, curl_l=None,
         grip=None, forward=None, up=None):
    """The arms holding a weapon: its hold, the grip moved (weapon space) and turned (a list of (weapon axis,
    degrees)), the left hand where `left` says (a hold's shape) or on the hold's place."""
    spec = HOLDS[name]
    pose.move("upperarm_l", B(*spec["shoulder_l"]))
    pose.move("upperarm_r", B(*spec["shoulder_r"]))
    frame = weapon_frame(forward or spec["forward"], up or spec["up"])
    for axis, degrees in turn:
        frame = frame @ Quaternion(B(*axis), math.radians(degrees)).to_matrix()
    grip_b = B(*(grip or spec["grip"])) + frame @ B(*grip_offset)
    place_right(pose, arms, grip_b, frame)
    kind, point, direction, thumb = left or spec["left"]
    if kind == "weapon":
        center = grip_b + frame @ B(*point)
        place_left(pose, arms, center, frame @ B(*direction), frame @ B(*thumb))
    else:
        place_left(pose, arms, B(*point), B(*direction), B(*thumb))
    curl(pose, "r", spec["curl"] if curl_r is None else curl_r, 25.0)
    curl(pose, "l", spec["curl_l"] if curl_l is None else curl_l, 20.0)
    return grip_b, frame


def blend_left(a, b, s):
    """Two left-hand places blended (both "weapon" or both "free")."""
    return (a[0], tuple(lerp(x, y, s) for x, y in zip(a[1], b[1])), tuple(lerp(x, y, s) for x, y in zip(a[2], b[2])),
            tuple(lerp(x, y, s) for x, y in zip(a[3], b[3])))


# The clips: functions of (arms, t) giving a pose.

def idle(name):
    def clip(arms, t):
        pose = arms.rig.pose()
        wave = math.sin(2.0 * math.pi * t)
        hold(pose, arms, name, grip_offset=(0.0, 0.0, 0.003 * wave), turn=[((1.0, 0.0, 0.0), 0.8 * wave)])
        return pose
    return clip


def draw(name):
    """Up from below the view, turned down and in, to the idle hold."""
    def clip(arms, t):
        pose = arms.rig.pose()
        s = smooth(t)
        spec = HOLDS[name]
        low = tuple(g + d for g, d in zip(spec["grip"], (-0.10, 0.06, -0.28)))
        grip = tuple(lerp(a, b, s) for a, b in zip(low, spec["grip"]))
        turn = [((0.0, 1.0, 0.0), -55.0 * (1.0 - s)), ((1.0, 0.0, 0.0), 30.0 * (1.0 - s))]
        left = spec["left"]
        if left[0] == "weapon":
            # The left hand joins once the weapon is up.
            reach = smooth((t - 0.5) / 0.4)
            away = LEFT_AWAY
            grip_b = B(*grip)
            frame = weapon_frame(spec["forward"], spec["up"])
            target = grip_b + frame @ B(*left[1])
            target_engine = (target.x, -target.y, target.z)
            joined = ("free", target_engine, left[2], left[3])
            hold(pose, arms, name, grip=grip, turn=turn,
                 left=blend_left(away, ("free",) + joined[1:], reach) if reach < 1.0 else None)
        else:
            hold(pose, arms, name, grip=grip, turn=turn)
        return pose
    return clip


def fire(name, back, up, settle=1.0):
    def clip(arms, t):
        pose = arms.rig.pose()
        k = math.exp(-6.0 * t / settle) * min(1.0, t * 10.0) if t < 1.0 else 0.0
        hold(pose, arms, name, grip_offset=(-back * k, 0.0, 0.004 * k), turn=[((0.0, 1.0, 0.0), up * k)])
        return pose
    return clip


def sniper_fire(arms, t):
    """The kick, then the bolt: the rifle rolls to the left and back while the right hand works it."""
    pose = arms.rig.pose()
    k = math.exp(-8.0 * t) * min(1.0, t * 30.0)
    bolt = track([(0.0, 0.0), (0.25, 0.0), (0.4, 1.0), (0.66, 1.0), (0.85, 0.0), (1.0, 0.0)], t)
    hold(pose, arms, "Sniper", grip_offset=(-0.06 * k - 0.02 * bolt, 0.0, 0.01 * k),
         turn=[((0.0, 1.0, 0.0), 9.0 * k + 4.0 * bolt), ((1.0, 0.0, 0.0), -14.0 * bolt)],
         curl_r=75.0 - 40.0 * bolt)
    return pose


def reload(name, left_keys, tilt_keys):
    """A reload: the weapon canted and raised (tilt_keys: (t, (roll, pitch, pull))), the left hand along left_keys:
    (t, point) in the weapon's space, or away below the view (None)."""
    def clip(arms, t):
        pose = arms.rig.pose()
        roll, pitch, pull = track(tilt_keys, t)
        spec = HOLDS[name]
        grip_offset = (-pull, 0.0, pull * 0.4)
        frame = weapon_frame(spec["forward"], spec["up"])
        for axis, degrees in (((1.0, 0.0, 0.0), roll), ((0.0, 1.0, 0.0), pitch)):
            frame = frame @ Quaternion(B(*axis), math.radians(degrees)).to_matrix()
        grip_b = B(*spec["grip"]) + frame @ B(*grip_offset)

        def place(point):
            if point is None:
                return LEFT_AWAY[1]
            p = grip_b + frame @ B(*point)
            return (p.x, -p.y, p.z)

        previous = left_keys[0]
        following = left_keys[-1]
        for a, b in zip(left_keys, left_keys[1:]):
            if a[0] <= t <= b[0]:
                previous, following = a, b
                break
        s = smooth((t - previous[0]) / (following[0] - previous[0])) if following[0] > previous[0] else 1.0
        start, end = place(previous[1]), place(following[1])
        point = tuple(lerp(x, y, s) for x, y in zip(start, end))
        hand_dir = frame @ B(*spec["left"][2])
        hand_thumb = frame @ B(*spec["left"][3])
        left = ("free", point, (hand_dir.x, -hand_dir.y, hand_dir.z), (hand_thumb.x, -hand_thumb.y, hand_thumb.z))
        hold(pose, arms, name, grip_offset=grip_offset, turn=[((1.0, 0.0, 0.0), roll), ((0.0, 1.0, 0.0), pitch)],
             left=left)
        return pose
    return clip


RIFLE_RELOAD = ([(0.00, RIFLE_SUPPORT), (0.12, RIFLE_SUPPORT), (0.22, (0.11, 0.0, -0.08)),
                 (0.30, (0.11, 0.0, -0.22)), (0.42, None), (0.52, None), (0.60, (0.11, 0.0, -0.22)),
                 (0.67, (0.11, 0.0, -0.08)), (0.72, (0.11, 0.0, -0.06)), (0.80, (0.02, 0.05, 0.08)),
                 (0.86, (0.02, 0.05, 0.08)), (0.94, RIFLE_SUPPORT), (1.0, RIFLE_SUPPORT)],
                [(0.0, (0.0, 0.0, 0.0)), (0.12, (-30.0, 10.0, 0.05)), (0.72, (-30.0, 10.0, 0.05)),
                 (0.82, (-12.0, 4.0, 0.03)), (0.92, (0.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
SNIPER_RELOAD = ([(0.00, RIFLE_SUPPORT), (0.12, RIFLE_SUPPORT), (0.22, (0.10, 0.0, -0.06)),
                  (0.29, (0.10, 0.0, -0.20)), (0.40, None), (0.55, None), (0.62, (0.10, 0.0, -0.20)),
                  (0.67, (0.10, 0.0, -0.06)), (0.74, (0.10, 0.0, -0.04)), (0.80, (-0.03, 0.06, 0.08)),
                  (0.88, (-0.07, 0.06, 0.08)), (0.94, RIFLE_SUPPORT), (1.0, RIFLE_SUPPORT)],
                 [(0.0, (0.0, 0.0, 0.0)), (0.10, (-25.0, 8.0, 0.05)), (0.76, (-25.0, 8.0, 0.05)),
                  (0.86, (-10.0, 3.0, 0.03)), (0.95, (0.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
PISTOL_RELOAD = ([(0.00, PISTOL_SUPPORT), (0.10, (0.0, -0.03, -0.10)), (0.22, (0.0, -0.02, -0.16)),
                  (0.35, None), (0.47, None), (0.60, (0.0, -0.02, -0.16)), (0.66, (0.0, -0.02, -0.10)),
                  (0.76, (-0.03, -0.03, 0.06)), (0.82, (-0.09, -0.03, 0.06)), (0.93, PISTOL_SUPPORT),
                  (1.0, PISTOL_SUPPORT)],
                 [(0.0, (0.0, 0.0, 0.0)), (0.10, (22.0, 18.0, 0.08)), (0.86, (22.0, 18.0, 0.08)),
                  (0.96, (0.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])


def grenade_pull_pin(arms, t):
    pose = arms.rig.pose()
    come = track([(0.0, 0.0), (0.35, 1.0), (0.5, 1.0), (0.75, 0.6), (1.0, 0.0)], t)
    pin = (0.0, -0.02, 0.07)
    spec = HOLDS["Grenade"]
    frame = weapon_frame(spec["forward"], spec["up"])
    at = B(*spec["grip"]) + frame @ B(*pin)
    pulled = at + B(-0.02, -0.12, 0.02) * smooth((t - 0.45) / 0.3)
    target = (pulled.x, -pulled.y, pulled.z)
    left = blend_left(LEFT_AWAY, ("free", target, (0.4, 0.9, -0.2), (0.3, 0.0, 1.0)), come)
    hold(pose, arms, "Grenade", left=left, curl_l=30.0 + 50.0 * come)
    return pose


def grenade_throw(arms, t):
    pose = arms.rig.pose()
    spec = HOLDS["Grenade"]
    grip = track([(0.0, spec["grip"]), (0.3, (0.05, 0.20, -0.02)), (0.5, (0.42, 0.08, 0.02)),
                  (0.75, (0.30, 0.00, -0.45)), (1.0, spec["grip"])], t)
    pitch = track([(0.0, 0.0), (0.3, 40.0), (0.5, -10.0), (0.75, -50.0), (1.0, 0.0)], t)
    hold(pose, arms, "Grenade", grip=grip, turn=[((0.0, 1.0, 0.0), pitch)],
         curl_r=track([(0.0, 70.0), (0.45, 70.0), (0.52, 10.0), (0.85, 20.0), (1.0, 70.0)], t))
    return pose


def knife_slash(arms, t):
    pose = arms.rig.pose()
    spec = HOLDS["Knife"]
    grip = track([(0.0, spec["grip"]), (0.25, (0.24, 0.26, -0.05)), (0.55, (0.36, -0.10, -0.26)),
                  (0.8, (0.26, 0.05, -0.30)), (1.0, spec["grip"])], t)
    forward = track([(0.0, spec["forward"]), (0.25, (0.6, 0.5, 0.6)), (0.55, (0.5, -0.9, -0.1)),
                     (0.8, (0.8, -0.3, 0.1)), (1.0, spec["forward"])], t)
    hold(pose, arms, "Knife", grip=grip, forward=forward)
    return pose


def c4_plant(arms, t):
    pose = arms.rig.pose()
    tap = max(0.0, math.sin(2.0 * math.pi * t * 7.0)) * 0.02 if 0.15 < t < 0.8 else 0.0
    down = track([(0.0, 0.0), (0.12, 1.0), (0.9, 1.0), (1.0, 0.0)], t)
    hold(pose, arms, "C4", grip_offset=(0.0, 0.0, -0.06 * down - tap), turn=[((0.0, 1.0, 0.0), -20.0 * down)])
    return pose


CLIPS = []
for _name, _draw in (("Pistol", 30), ("Rifle", 30), ("Sniper", 38), ("Grenade", 15), ("C4", 15), ("Knife", 30)):
    CLIPS.append((_name + "_Idle", 60, True, [], idle(_name)))
    CLIPS.append((_name + "_Draw", _draw, False, [(_draw - 4, "Deploy")], draw(_name)))
CLIPS += [
    ("Pistol_Fire", 8, False, [], fire("Pistol", 0.025, 9.0)),
    ("Rifle_Fire", 8, False, [], fire("Rifle", 0.02, 3.5)),
    ("Sniper_Fire", 45, False, [(18, "BoltBack"), (30, "BoltForward")], sniper_fire),
    ("Pistol_Reload", 81, False, [(18, "MagOut"), (52, "MagIn")], reload("Pistol", *PISTOL_RELOAD)),
    ("Rifle_Reload", 75, False, [(22, "MagOut"), (50, "MagIn"), (62, "BoltBack")], reload("Rifle", *RIFLE_RELOAD)),
    ("Sniper_Reload", 111, False, [(32, "MagOut"), (74, "MagIn"), (92, "BoltBack")],
     reload("Sniper", *SNIPER_RELOAD)),
    ("Grenade_PullPin", 20, False, [(12, "PinPull")], grenade_pull_pin),
    ("Grenade_Throw", 15, False, [(7, "Release")], grenade_throw),
    ("Knife_Slash", 15, False, [], knife_slash),
    ("C4_Plant", 90, False, [(75, "Plant")], c4_plant),
]


def add_clips(armature):
    rig = leon_art.Rig(armature, leon_art.ARMS_BONES)
    arms = Arms(rig)
    for name, frames, loop, notifies, pose_of in sorted(CLIPS, key=lambda clip: clip[0]):
        step = 1 if frames <= 15 else 2
        frame_list = sorted(set(list(range(0, frames + 1, step)) + [frames]))
        poses = [(frame, pose_of(arms, frame / frames).keys()) for frame in frame_list]
        leon_art.add_action(armature, name, poses, notifies, loop)
