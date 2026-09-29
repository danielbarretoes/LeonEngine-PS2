"""The characters' third-person clips on leon_art.HUMANOID_BONES (Docs/ART_PIPELINE.md, "Animations"), posed by code:
make_characters.py calls add_clips on the body's armature.

Every clip is a function of the time t in [0, 1] sampled every few frames and keyed LINEAR at 30 fps; a looping clip
ends on its first pose. The legs are set by hip swing, knee bend and foot angles and grounded (the pelvis moves so the
lowest foot or knee touches the floor); the upper body holds the weapon in a stance attached to the chest, the hands
placed on the weapon by two-bone IK: the right hand's weapon socket (Weapon_R) on the grip, the left hand on the
weapon's support (the handguard) or on the right hand (a pistol).

The stances and the aim offsets (UE: an aim offset per weapon): the locomotion, the crouch and the jump hold a rifle.
Each stance's aim offset (AO_Rifle, AO_Pistol, AO_Grenade) poses that stance at pitches -90..90 and measures its
additive from the rifle's centre pose (BasePose A_Aim_Rifle_Center), so on top of the locomotion it turns the rifle's
arms into the stance's and pitches the chest. The upper body montages of a pistol or a grenade are stored in the
rifle's space (rebased: montage = rifle centre * stance centre^-1 * intended pose, bone by bone), so that the aim
offset added on top gives the intended pose. The whole-body montages (plant, defuse, death) play without an aim
offset (AShooterCharacter clears it).
"""

import math

from mathutils import Quaternion, Vector

import leon_art

B = leon_art.to_blender
# Blender's axes: +X forward, +Y the character's left, +Z up.
X_AXIS = (1.0, 0.0, 0.0)
Y_AXIS = (0.0, 1.0, 0.0)
Z_AXIS = (0.0, 0.0, 1.0)

UPPER_BODY = ("spine_01", "spine_02", "spine_03", "neck_01", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
              "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r")

# A weapon's support point (the left hand's), in its space: engine metres from the grip, x forward, z up.
RIFLE_SUPPORT = (0.25, 0.0, 0.02)

# The stances: the grip (engine metres, the character at rest), the weapon's forward and up, the left hand.
STANCES = {
    "Rifle": {
        "grip": (0.25, 0.12, 1.30), "forward": (1.0, -0.06, 0.0), "up": (0.0, 0.0, 1.0),
        "clavicle": 12.0, "left": ("weapon", RIFLE_SUPPORT, (0.25, 0.95, 0.10), (0.95, -0.15, 0.25)),
        "pole_r": (-0.3, 0.8, -1.0), "pole_l": (-0.2, -0.6, -1.0),
    },
    "Pistol": {
        "grip": (0.40, 0.05, 1.37), "forward": (1.0, -0.02, 0.0), "up": (0.0, 0.0, 1.0),
        "clavicle": 16.0, "left": ("weapon", (-0.005, -0.035, -0.025), (0.55, 0.25, -0.80), (0.60, -0.10, 0.80)),
        "pole_r": (-0.2, 0.7, -1.0), "pole_l": (-0.2, -0.7, -1.0),
    },
    "Grenade": {
        "grip": (0.22, 0.17, 1.20), "forward": (1.0, 0.10, 0.25), "up": (-0.25, 0.0, 1.0),
        "clavicle": 6.0, "left": ("chest", (0.10, -0.24, 1.02), (0.25, -0.05, -1.0), (1.0, 0.0, 0.25)),
        "pole_r": (-0.4, 0.6, -1.0), "pole_l": (-0.5, -0.4, -1.0),
    },
}


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def track(keys, t):
    """A value from (time, value) keys, eased between them (values may be numbers or tuples)."""
    if t <= keys[0][0]:
        return keys[0][1]
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t <= t1:
            s = smooth((t - t0) / (t1 - t0)) if t1 > t0 else 1.0
            if isinstance(v0, tuple):
                return tuple(lerp(a, b, s) for a, b in zip(v0, v1))
            return lerp(v0, v1, s)
    return keys[-1][1]


class Body:
    """The rest pose's facts the posing needs, in Blender's axes."""

    def __init__(self, rig):
        self.rig = rig
        name, bone, location, axes = leon_art.HUMANOID_SOCKETS[0]
        self.socket = B(*location)
        self.socket_frame = leon_art._frame_matrix(B(*axes[0]), B(*axes[1]))
        self.hand_head = rig.head["hand_r"]
        self.left_dir = B(0.0, -1.0, 0.0)
        self.left_thumb = B(1.0, 0.0, 0.0)


def rotate(pose, bone, axis, degrees):
    pose.rotate(bone, axis, degrees)


def legs(pose, side, swing=0.0, knee=0.0, toe=0.0, spread=0.0):
    """A leg: the hip swung forward (degrees), the knee bent, the foot kept level plus `toe` (toes down), the hip
    turned out by `spread`."""
    outward = -1.0 if side == "r" else 1.0
    rotate(pose, "thigh_" + side, Y_AXIS, -swing)
    rotate(pose, "thigh_" + side, X_AXIS, outward * spread)
    rotate(pose, "calf_" + side, Y_AXIS, knee)
    rotate(pose, "foot_" + side, Y_AXIS, swing - knee + toe)


def ground(pose, knees=False):
    """Moves the pelvis so the lowest foot (or knee, when kneeling) touches the floor."""
    _world, heads = pose.solve()
    lowest = min(heads["foot_l"].z - 0.08, heads["foot_r"].z - 0.08, heads["ball_l"].z - 0.02,
                 heads["ball_r"].z - 0.02)
    if knees:
        lowest = min(lowest, heads["calf_l"].z - 0.06, heads["calf_r"].z - 0.06)
    pose.move("pelvis", (0.0, 0.0, -lowest))


def chest_point(pose, body, point_b):
    """A point that moves with the chest (spine_03), from where it is at rest."""
    world, heads = pose.solve()
    rest = body.rig.head["spine_03"]
    return heads["spine_03"] + world["spine_03"] @ (point_b - rest), world["spine_03"]


def weapon_frame(forward, up):
    return leon_art._frame_matrix(B(*forward), B(*up))


def place_right(pose, body, grip_b, frame_b, pole):
    """The right hand's socket on the grip, the weapon along frame_b (its x forward, z up)."""
    hand = (frame_b @ body.socket_frame.transposed()).to_quaternion()
    wrist = grip_b - hand @ (body.socket - body.hand_head)
    pose.reach("upperarm_r", "lowerarm_r", "hand_r", wrist, B(*pole))
    pose.orient("hand_r", hand)


def place_left(pose, body, center_b, dir_b, thumb_b, pole):
    """The left hand holding what is at center_b, the hand pointing along dir_b, the thumb to thumb_b."""
    rotation = leon_art.frame_rotation(body.left_dir, body.left_thumb, dir_b, thumb_b)
    palm = rotation @ B(0.0, 0.0, -1.0)
    direction = rotation @ body.left_dir
    wrist = center_b - direction * 0.07 - palm * 0.03
    pose.reach("upperarm_l", "lowerarm_l", "hand_l", wrist, B(*pole))
    pose.orient("hand_l", rotation)


def to_weapon(frame_b, grip_b, point):
    """A point of the weapon's space (engine metres from the grip) in the armature's space."""
    return grip_b + frame_b @ B(*point)


def hold(pose, body, stance_name, grip_offset=(0.0, 0.0, 0.0), turn=None, left=None, right_free=None,
         left_pole=None, clavicle=None):
    """The upper body holding a stance's weapon: the grip moved by grip_offset (weapon space) and the weapon turned by
    `turn` (a list of (axis in weapon space, degrees)), the left hand on the stance's hold unless `left` gives
    ("weapon" | "chest", point, dir, thumb) in the same shape. right_free, (chest point, forward, up), places the
    right hand without the stance's grip (a throw)."""
    stance = STANCES[stance_name]
    degrees = stance["clavicle"] if clavicle is None else clavicle
    rotate(pose, "clavicle_r", Z_AXIS, degrees)
    rotate(pose, "clavicle_l", Z_AXIS, -degrees)
    if right_free is not None:
        grip_rest, forward, up = right_free
    else:
        grip_rest, forward, up = stance["grip"], stance["forward"], stance["up"]
    rest_frame = weapon_frame(forward, up)
    frame = rest_frame
    for axis, angle in turn or ():
        frame = frame @ Quaternion(B(*axis), math.radians(angle)).to_matrix()
    grip_rest_b = B(*grip_rest) + rest_frame @ B(*grip_offset)
    grip_b, chest = chest_point(pose, body, grip_rest_b)
    frame_b = chest.to_matrix() @ frame
    place_right(pose, body, grip_b, frame_b, stance["pole_r"])
    kind, point, direction, thumb = left or stance["left"]
    if kind == "weapon":
        center = to_weapon(frame_b, grip_b, point)
        dir_b = frame_b @ B(*direction)
        thumb_b = frame_b @ B(*thumb)
    else:
        center, _chest = chest_point(pose, body, B(*point))
        dir_b = chest @ B(*direction)
        thumb_b = chest @ B(*thumb)
    place_left(pose, body, center, dir_b, thumb_b, left_pole or stance["pole_l"])
    return grip_b, frame_b


def aim(pose, pitch):
    """Pitches the upper body (degrees, up positive): the spine takes half, the neck and the head follow."""
    for bone, share in (("spine_01", 0.15), ("spine_02", 0.17), ("spine_03", 0.18)):
        rotate(pose, bone, Y_AXIS, -pitch * share)
    rotate(pose, "neck_01", Y_AXIS, -pitch * 0.15)
    rotate(pose, "head", Y_AXIS, -pitch * 0.2)


def aim_stance_turn(pitch):
    """The part of an aim the arms make: the weapon turns about its grip by the pitch the spine does not."""
    return [((0.0, 1.0, 0.0), pitch * 0.5)]


def standing(pose, t=0.0, breathe=1.0):
    """Standing at ease: the feet apart, the knees soft, breathing."""
    wave = math.sin(2.0 * math.pi * t)
    for side in ("l", "r"):
        legs(pose, side, swing=2.0, knee=6.0, spread=4.0)
    rotate(pose, "spine_02", Y_AXIS, 2.0)
    rotate(pose, "spine_03", Y_AXIS, 1.0 * breathe * wave)
    ground(pose)
    pose.move("pelvis", (0.0, 0.0, 0.004 * breathe * wave))


def stance_pose(body, stance, pitch=0.0, t=0.0):
    pose = body.rig.pose()
    standing(pose, t, 0.0)
    aim(pose, pitch)
    hold(pose, body, stance, turn=aim_stance_turn(pitch))
    return pose


# Locomotion.

def walk_cycle(pose, t, stride, knee_lift, stance_knee, lean, direction, bob=0.0):
    """One leg cycle at time t (0..1): left contact at 0, right at 0.5. direction is F, B, L or R."""
    phase = 2.0 * math.pi * (t if direction != "B" else -t)
    for side, offset in (("l", 0.0), ("r", math.pi)):
        p = phase + offset
        swinging = math.sin(p - math.pi)  # > 0 while the leg swings forward
        knee = stance_knee + knee_lift * max(0.0, swinging)
        toe = 12.0 * max(0.0, swinging)
        if direction in ("F", "B"):
            legs(pose, side, swing=stride * math.cos(p), knee=knee, toe=toe, spread=3.0)
        else:
            # Side steps: the leading leg reaches out, the other one follows under the body.
            lead = "l" if direction == "L" else "r"
            q = p if side == lead else p
            reach = stride * 0.6 * (math.cos(q) if side == lead else -math.cos(q))
            legs(pose, side, swing=4.0, knee=knee, toe=toe, spread=4.0 + reach)
    twist = 5.0 * math.cos(phase) if direction in ("F", "B") else 0.0
    rotate(pose, "pelvis", Z_AXIS, -twist)
    rotate(pose, "spine_01", Z_AXIS, twist)
    rotate(pose, "spine_01", Y_AXIS, lean)
    rotate(pose, "spine_03", Y_AXIS, -lean * 0.6)
    ground(pose)
    pose.move("pelvis", (0.0, 0.0, bob * math.cos(2.0 * phase)))


def crouch(pose, t=0.0, swing_l=0.0, swing_r=0.0, lift_l=0.0, lift_r=0.0, spread_l=0.0, spread_r=0.0):
    """A deep crouch, both feet flat: the hips back, the torso upright."""
    legs(pose, "l", swing=72.0 + swing_l, knee=128.0 + lift_l, toe=0.0, spread=8.0 + spread_l)
    legs(pose, "r", swing=72.0 + swing_r, knee=128.0 + lift_r, toe=0.0, spread=8.0 + spread_r)
    rotate(pose, "pelvis", Y_AXIS, 12.0)
    rotate(pose, "spine_01", Y_AXIS, -4.0)
    rotate(pose, "spine_02", Y_AXIS, -3.0)
    rotate(pose, "spine_03", Y_AXIS, -3.0 + 1.0 * math.sin(2.0 * math.pi * t))
    ground(pose)


def crouch_walk(pose, t, direction):
    phase = 2.0 * math.pi * (t if direction != "B" else -t)
    values = {}
    for side, offset in (("l", 0.0), ("r", math.pi)):
        p = phase + offset
        swinging = max(0.0, math.sin(p - math.pi))
        if direction in ("F", "B"):
            values[side] = (16.0 * math.cos(p), -14.0 * swinging, 0.0)
        else:
            lead = "l" if direction == "L" else "r"
            spread = 10.0 * math.cos(p) if side == lead else -10.0 * math.cos(p)
            values[side] = (0.0, -12.0 * swinging, spread)
    crouch(pose, t, values["l"][0], values["r"][0], values["l"][1], values["r"][1], values["l"][2], values["r"][2])


# The clips: (name, frames, loop, notifies, pose(body, t), stance of a rebased upper body montage or None).

def idle(body, t):
    pose = body.rig.pose()
    standing(pose, t)
    hold(pose, body, "Rifle", grip_offset=(0.0, 0.0, 0.004 * math.sin(2.0 * math.pi * t)))
    return pose


def locomotion(direction, stride, knee_lift, stance_knee, lean, bob):
    def clip(body, t):
        pose = body.rig.pose()
        walk_cycle(pose, t, stride, knee_lift, stance_knee, lean, direction, bob)
        sway = 0.006 * math.sin(4.0 * math.pi * t)
        hold(pose, body, "Rifle", grip_offset=(0.0, 0.0, sway))
        return pose
    return clip


def crouch_idle(body, t):
    pose = body.rig.pose()
    crouch(pose, t)
    hold(pose, body, "Rifle")
    return pose


def crouch_moving(direction):
    def clip(body, t):
        pose = body.rig.pose()
        crouch_walk(pose, t, direction)
        hold(pose, body, "Rifle", grip_offset=(0.0, 0.0, 0.005 * math.sin(4.0 * math.pi * t)))
        return pose
    return clip


def jump_start(body, t):
    pose = body.rig.pose()
    s = smooth(t)
    for side, swing, knee in (("l", 34.0, 55.0), ("r", 22.0, 40.0)):
        legs(pose, side, swing=lerp(2.0, swing, s), knee=lerp(6.0, knee, s), toe=lerp(0.0, 25.0, s), spread=4.0)
    rotate(pose, "spine_01", Y_AXIS, 6.0 * s)
    hold(pose, body, "Rifle", grip_offset=(0.0, 0.0, 0.03 * s))
    return pose


def jump_loop(body, t):
    pose = body.rig.pose()
    wave = math.sin(2.0 * math.pi * t)
    legs(pose, "l", swing=34.0 + 5.0 * wave, knee=55.0 + 6.0 * wave, toe=25.0, spread=4.0)
    legs(pose, "r", swing=22.0 - 5.0 * wave, knee=40.0 - 6.0 * wave, toe=25.0, spread=4.0)
    rotate(pose, "spine_01", Y_AXIS, 6.0)
    hold(pose, body, "Rifle", grip_offset=(0.0, 0.0, 0.03 + 0.01 * wave))
    return pose


def jump_land(body, t):
    pose = body.rig.pose()
    depth = track([(0.0, 0.7), (0.3, 1.0), (1.0, 0.0)], t)
    for side in ("l", "r"):
        legs(pose, side, swing=2.0 + 38.0 * depth, knee=6.0 + 70.0 * depth, spread=4.0 + 2.0 * depth)
    rotate(pose, "spine_01", Y_AXIS, 10.0 * depth)
    rotate(pose, "spine_03", Y_AXIS, -6.0 * depth)
    ground(pose)
    hold(pose, body, "Rifle", grip_offset=(0.0, 0.0, -0.02 * depth))
    return pose


def aim_pose(stance, pitch):
    def clip(body, t):
        return stance_pose(body, stance, pitch)
    return clip


def fire(stance, kick_back, kick_up):
    def clip(body, t):
        pose = body.rig.pose()
        standing(pose, 0.0, 0.0)
        k = math.exp(-5.0 * t) * min(1.0, t * 8.0) if t < 1.0 else 0.0
        rotate(pose, "spine_03", Y_AXIS, -2.0 * k)
        hold(pose, body, stance, grip_offset=(-kick_back * k, 0.0, 0.01 * k), turn=[((0.0, 1.0, 0.0), kick_up * k)])
        return pose
    return clip


def throw_grenade(body, t):
    pose = body.rig.pose()
    standing(pose, 0.0, 0.0)
    twist = track([(0.0, 0.0), (0.3, -18.0), (0.55, 14.0), (0.8, 8.0), (1.0, 0.0)], t)
    lean = track([(0.0, 0.0), (0.3, -8.0), (0.55, 10.0), (0.8, 6.0), (1.0, 0.0)], t)
    rotate(pose, "spine_02", Z_AXIS, twist * 0.5)
    rotate(pose, "spine_03", Z_AXIS, twist * 0.5)
    rotate(pose, "spine_02", Y_AXIS, lean)
    stance = STANCES["Grenade"]
    grip = track([(0.0, stance["grip"]), (0.3, (-0.10, 0.28, 1.66)), (0.55, (0.42, 0.14, 1.58)),
                  (0.8, (0.38, -0.02, 1.22)), (1.0, stance["grip"])], t)
    forward = track([(0.0, stance["forward"]), (0.3, (-0.4, 0.2, 1.0)), (0.55, (1.0, 0.0, 0.3)),
                     (0.8, (1.0, -0.3, -0.4)), (1.0, stance["forward"])], t)
    up = track([(0.0, stance["up"]), (0.3, (1.0, 0.0, 0.3)), (0.55, (-0.3, 0.0, 1.0)), (0.8, (0.3, 0.0, 1.0)),
                (1.0, stance["up"])], t)
    left_point = track([(0.0, stance["left"][1]), (0.3, (0.40, -0.18, 1.48)), (0.55, (0.10, -0.26, 1.10)),
                        (1.0, stance["left"][1])], t)
    hold(pose, body, "Grenade", right_free=(grip, forward, up),
         left=("chest", left_point, stance["left"][2], stance["left"][3]))
    return pose


def reload(stance, frames, keys_left, tilt_keys):
    """A reload: the weapon canted and raised (tilt_keys: (t, (roll, pitch, pull))), the left hand following
    keys_left: (t, ("weapon" | "chest", point))."""
    def clip(body, t):
        pose = body.rig.pose()
        standing(pose, 0.0, 0.0)
        roll, pitch, pull = track(tilt_keys, t)
        kind_point = None
        for (t0, (k0, p0)), (t1, (k1, p1)) in zip(keys_left, keys_left[1:]):
            if t0 <= t <= t1:
                s = smooth((t - t0) / (t1 - t0)) if t1 > t0 else 1.0
                kind_point = (k0, p0, k1, p1, s)
                break
        if kind_point is None:
            kind_point = (keys_left[-1][1][0], keys_left[-1][1][1], keys_left[-1][1][0], keys_left[-1][1][1], 1.0)
        left_stance = STANCES[stance]["left"]
        # The left hand's place: both ends in the armature's space, blended.
        grip_b, frame_b = hold(pose, body, stance, grip_offset=(-pull, 0.0, pull * 0.5),
                               turn=[((1.0, 0.0, 0.0), roll), ((0.0, 1.0, 0.0), pitch)])
        k0, p0, k1, p1, s = kind_point

        def where(kind, point):
            if kind == "weapon":
                return to_weapon(frame_b, grip_b, point)
            return chest_point(pose, body, B(*point))[0]

        center = where(k0, p0).lerp(where(k1, p1), s)
        if left_stance[0] == "weapon":
            dir_b = frame_b @ B(*left_stance[2])
            thumb_b = frame_b @ B(*left_stance[3])
        else:
            chest = chest_point(pose, body, Vector())[1]
            dir_b = chest @ B(*left_stance[2])
            thumb_b = chest @ B(*left_stance[3])
        place_left(pose, body, center, dir_b, thumb_b, STANCES[stance]["pole_l"])
        return pose
    return clip


RIFLE_RELOAD_LEFT = [
    (0.00, ("weapon", RIFLE_SUPPORT)), (0.14, ("weapon", (0.11, 0.0, -0.10))), (0.29, ("weapon", (0.11, 0.0, -0.24))),
    (0.44, ("chest", (0.16, -0.12, 1.02))), (0.56, ("chest", (0.16, -0.12, 1.02))),
    (0.62, ("weapon", (0.11, 0.0, -0.24))), (0.67, ("weapon", (0.11, 0.0, -0.10))),
    (0.77, ("weapon", (0.10, 0.0, -0.09))), (0.90, ("weapon", RIFLE_SUPPORT)), (1.00, ("weapon", RIFLE_SUPPORT)),
]
RIFLE_RELOAD_TILT = [(0.0, (0.0, 0.0, 0.0)), (0.12, (28.0, 8.0, 0.04)), (0.82, (28.0, 8.0, 0.04)),
                     (0.95, (0.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))]
PISTOL_RELOAD_LEFT = [
    (0.00, ("weapon", (-0.005, -0.035, -0.025))), (0.14, ("weapon", (0.0, -0.01, -0.12))),
    (0.22, ("weapon", (0.0, -0.01, -0.24))), (0.38, ("chest", (0.08, -0.12, 1.00))),
    (0.47, ("chest", (0.08, -0.12, 1.00))), (0.57, ("weapon", (0.0, -0.01, -0.24))),
    (0.64, ("weapon", (0.0, -0.01, -0.12))), (0.76, ("weapon", (-0.02, -0.02, 0.06))),
    (0.82, ("weapon", (-0.09, -0.02, 0.06))), (0.93, ("weapon", (-0.005, -0.035, -0.025))),
    (1.00, ("weapon", (-0.005, -0.035, -0.025))),
]
PISTOL_RELOAD_TILT = [(0.0, (0.0, 0.0, 0.0)), (0.12, (-22.0, 16.0, 0.12)), (0.86, (-22.0, 16.0, 0.12)),
                      (0.96, (0.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))]


def kneel(pose, t_down, work=0.0):
    """Kneeling on the right knee over something on the floor, leaning to it (t_down 0 standing, 1 down)."""
    s = smooth(t_down)
    legs(pose, "l", swing=lerp(2.0, 82.0, s), knee=lerp(6.0, 95.0, s), spread=lerp(4.0, 10.0, s))
    legs(pose, "r", swing=lerp(2.0, -8.0, s), knee=lerp(6.0, 105.0, s), toe=lerp(0.0, 60.0, s), spread=4.0)
    rotate(pose, "spine_01", Y_AXIS, 14.0 * s)
    rotate(pose, "spine_02", Y_AXIS, 14.0 * s)
    rotate(pose, "spine_03", Y_AXIS, 8.0 * s)
    rotate(pose, "neck_01", Y_AXIS, 10.0 * s)
    ground(pose, knees=True)


def hands_at(pose, body, right_b, left_b, press=0.0):
    down = B(0.3, 0.0, -1.0)
    rotation_r = leon_art.frame_rotation(B(0.0, 1.0, 0.0), B(1.0, 0.0, 0.0), down, B(1.0, 0.0, 0.3))
    palm_r = rotation_r @ B(0.0, 0.0, -1.0)
    wrist = right_b - (rotation_r @ B(0.0, 1.0, 0.0)) * 0.07 - palm_r * 0.03 + Vector((0.0, 0.0, press))
    pose.reach("upperarm_r", "lowerarm_r", "hand_r", wrist, B(-0.2, 1.0, -0.6))
    pose.orient("hand_r", rotation_r)
    place_left(pose, body, left_b, down, B(1.0, 0.0, 0.3), (-0.2, -1.0, -0.6))


ARMS = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r")


def from_stance_arms(base, target, body, s):
    """target with its arms blended from the rifle stance's on base (weight 0) to its own (weight 1): a whole-body
    clip starts from the pose the locomotion held."""
    held = base.copy()
    hold(held, body, "Rifle")
    for bone in ARMS:
        a = held.rot.get(bone, Quaternion())
        b = target.rot.get(bone, Quaternion())
        target.rot[bone] = a.slerp(b, s)
    return target


def plant(body, t):
    base = body.rig.pose()
    down = track([(0.0, 0.0), (0.15, 1.0), (0.87, 1.0), (1.0, 0.0)], t)
    kneel(base, down)
    pose = base.copy()
    tap = max(0.0, math.sin(2.0 * math.pi * t * 7.0)) * 0.03 if 0.22 < t < 0.8 else 0.0
    hands_at(pose, body, B(0.55, 0.05, 0.18), B(0.55, -0.10, 0.12), tap)
    return from_stance_arms(base, pose, body, smooth(down))


def defuse(body, t):
    """Kneeling down over 15 frames, then working at the bomb in a 30-frame loop (frames 15 to 45: the montage's
    Loop section)."""
    frame = t * DEFUSE_FRAMES
    down = smooth(min(frame / 15.0, 1.0))
    base = body.rig.pose()
    kneel(base, down)
    pose = base.copy()
    wave = math.sin(2.0 * math.pi * max(frame - 15.0, 0.0) / 30.0)
    right = B(0.55 + 0.03 * wave, 0.02, 0.16 + 0.03 * max(0.0, wave))
    left = B(0.55 - 0.02 * wave, -0.12, 0.12 + 0.03 * max(0.0, -wave))
    hands_at(pose, body, right, left)
    return from_stance_arms(base, pose, body, down)


DEFUSE_FRAMES = 45


def death(forward):
    """Shot and falling: the knees give, the body turns about the feet onto its back (or front) and lies still, the
    arms flung out from the weapon's stance onto the floor."""
    sign = 1.0 if forward else -1.0

    def clip(body, t):
        base = body.rig.pose()
        fall = track([(0.0, 0.0), (0.18, 0.05), (0.62, 1.0), (1.0, 1.0)], t)
        fall = fall * fall if t < 0.62 else 1.0
        bounce = track([(0.0, 0.0), (0.62, 0.0), (0.68, 1.0), (0.78, 0.0), (1.0, 0.0)], t)
        buckle = track([(0.0, 0.0), (0.25, 1.0), (0.62, 0.6), (1.0, 0.3)], t)
        rotate(base, "root", Y_AXIS, sign * 90.0 * fall)
        base.move("root", (0.0, 0.0, (0.17 if forward else 0.13) * fall + 0.02 * bounce))
        for side, extra in (("l", 1.0), ("r", 0.7)):
            legs(base, side, swing=sign * 12.0 * buckle * extra, knee=35.0 * buckle * extra, toe=10.0 * fall,
                 spread=6.0 + 6.0 * fall)
        rotate(base, "spine_02", Y_AXIS, sign * 10.0 * buckle)
        rotate(base, "head", Y_AXIS, -sign * 20.0 * fall)
        pose = base.copy()
        for side, s in (("l", 1.0), ("r", -1.0)):
            # Lying: the arms out on the floor, a little down from the T pose, the elbows soft.
            rotate(pose, "upperarm_" + side, X_AXIS, -s * 25.0)
            rotate(pose, "upperarm_" + side, Z_AXIS, -s * sign * 10.0)
            rotate(pose, "lowerarm_" + side, Z_AXIS, s * sign * 20.0)
        return from_stance_arms(base, pose, body, track([(0.0, 0.0), (0.45, 1.0), (1.0, 1.0)], t))
    return clip


FOOTSTEPS = lambda frames: [(0, "Footstep_L"), (frames // 2, "Footstep_R")]  # noqa: E731
FOOTSTEPS_BACK = lambda frames: [(0, "Footstep_R"), (frames // 2, "Footstep_L")]  # noqa: E731

# Walk and run: the hips' swing, the knee's lift, the standing knee, the lean and the bob. A cycle covers about the
# distance the blend space's speed moves in its length (walk 330 cm/s, run 560: the feet slide a little).
WALK = (30.0, 50.0, 5.0, 3.0, 0.01)
RUN = (42.0, 85.0, 14.0, 9.0, 0.02)

CLIPS = [
    ("Idle", 60, True, [], idle, None),
    ("Crouch_Idle", 60, True, [], crouch_idle, None),
    ("Jump_Start", 6, False, [], jump_start, None),
    ("Jump_Loop", 20, True, [], jump_loop, None),
    ("Jump_Land", 8, False, [(0, "Land")], jump_land, None),
    ("Fire_Rifle", 8, False, [], fire("Rifle", 0.03, 5.0), None),
    ("Fire_Pistol", 8, False, [], fire("Pistol", 0.02, 11.0), "Pistol"),
    ("Throw_Grenade", 20, False, [(11, "Release")], throw_grenade, "Grenade"),
    ("Reload_Rifle", 75, False, [(22, "MagOut"), (50, "MagIn")],
     reload("Rifle", 75, RIFLE_RELOAD_LEFT, RIFLE_RELOAD_TILT), None),
    ("Reload_Sniper", 111, False, [(32, "MagOut"), (74, "MagIn")],
     reload("Rifle", 111, RIFLE_RELOAD_LEFT, RIFLE_RELOAD_TILT), None),
    ("Reload_Pistol", 81, False, [(18, "MagOut"), (52, "MagIn")],
     reload("Pistol", 81, PISTOL_RELOAD_LEFT, PISTOL_RELOAD_TILT), "Pistol"),
    ("Plant_C4", 90, False, [(75, "Plant")], plant, None),
    ("Defuse", DEFUSE_FRAMES, False, [], defuse, None),
    ("Death_Back", 40, False, [], death(False), None),
    ("Death_Front", 40, False, [], death(True), None),
]
for _direction, _name in (("F", "F"), ("B", "B"), ("L", "L"), ("R", "R")):
    _steps = FOOTSTEPS_BACK if _direction == "B" else FOOTSTEPS
    CLIPS.append(("Walk_" + _name, 20, True, _steps(20), locomotion(_direction, *WALK), None))
    CLIPS.append(("Run_" + _name, 16, True, _steps(16), locomotion(_direction, *RUN), None))
    CLIPS.append(("Crouch_Walk_" + _name, 30, True, [], crouch_moving(_direction), None))
AIM_PITCHES = (("Down90", -90.0), ("Down45", -45.0), ("Center", 0.0), ("Up45", 45.0), ("Up90", 90.0))
for _stance in ("Rifle", "Pistol", "Grenade"):
    for _suffix, _pitch in AIM_PITCHES:
        CLIPS.append(("Aim_%s_%s" % (_stance, _suffix), 1, True, [], aim_pose(_stance, _pitch), None))


def rebase(keys, base, stance, bones=UPPER_BODY):
    """An upper-body pose stored in the rifle's space: base * stance^-1 * pose for each upper-body bone's rotation
    (and base - stance + pose for its location), so that the stance's aim offset on top gives the pose back."""
    out = dict(keys)
    for bone in bones:
        q = Quaternion(base[bone][0]) @ Quaternion(stance[bone][0]).inverted() @ Quaternion(keys[bone][0])
        loc = Vector(base[bone][1]) - Vector(stance[bone][1]) + Vector(keys[bone][1])
        out[bone] = (tuple(q), tuple(loc))
    return out


def key_frames(frames, step):
    return sorted(set(list(range(0, frames + 1, step)) + [frames]))


def add_clips(armature):
    rig = leon_art.Rig(armature, leon_art.HUMANOID_BONES)
    body = Body(rig)
    base = stance_pose(body, "Rifle").keys()
    centres = {name: stance_pose(body, name).keys() for name in STANCES}
    for name, frames, loop, notifies, pose_of, space in sorted(CLIPS, key=lambda clip: clip[0]):
        step = 1 if frames <= 10 else 2
        poses = []
        for frame in key_frames(frames, step):
            keys = pose_of(body, frame / frames).keys()
            if space is not None:
                keys = rebase(keys, base, centres[space])
            poses.append((frame, keys))
        leon_art.add_action(armature, name, poses, notifies, loop)
