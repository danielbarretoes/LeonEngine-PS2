"""Synthesizes ShooterGame's sounds as 16-bit PCM WAV files for LeonEd's sound import.

    python3 Game/ShooterGame/SourceArt/Sounds/make_sounds.py

Writes next to this script, for Game/ShooterGame/SourceArt/ImportList.ini to import as /Game/Sounds/S_<Name>
(DefaultGame.ini names them):

- the weapons' and the bomb's: Pistol_Fire, Rifle_Fire, Sniper_Fire, Reload, Empty, Equip, Throw, Explosion,
  Bomb_Beep, Bomb_Plant, Bomb_Defuse;
- the footsteps on each surface, a left and a right foot (ps2-shipping N30f; CS's pl_step, pl_dirt, pl_tile,
  pl_metal): Step_<Surface>_L / _R for Concrete, Dirt, Tile, Metal and Wood, and the ladder's Step_Ladder_1 / _2
  (CS's pl_ladder);
- the bullets' impacts on each surface (CS's debris and ricochets): Impact_Concrete, _Dirt, _Tile, _Metal (a
  ricochet), _Wood, _Glass, and on a character (CS's bhit_): Hit_Flesh, Hit_Kevlar, Hit_Helmet;
- the radio (CS speaks its messages; these are squelched tone patterns, no speech): Radio_Command, Radio_Group and
  Radio_Report (a menu each), Radio_FireInTheHole and Radio_BombPlanted.

Every sound is made here from noise and sine waves (22050 Hz, mono): a shot is a burst of filtered noise over a low
thump, decaying fast; the explosion a long low rumble; the mechanical sounds short clicks; a step a heel and a toe, a
thud and the surface's noise band or ring; an impact the surface's crack, thump, ring or shatter; the radio a squelch,
tones and a squelch through a radio's band. The noise comes from a fixed linear congruential generator, so the files
are the same on every run and every machine (the reimport gate G5). Only the Python standard library is used; no
recorded or external audio (Game/ShooterGame/SourceArt/LICENSES.md).
"""

import math
import os
import struct
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
RATE = 22050


class Noise:
    """A deterministic white noise source in [-1, 1] (Numerical Recipes' LCG)."""

    def __init__(self, seed):
        self.state = seed & 0xFFFFFFFF

    def next(self):
        self.state = (1664525 * self.state + 1013904223) & 0xFFFFFFFF
        return (self.state / 0x7FFFFFFF) - 1.0


def shot(seconds, seed, noise_decay, thump_hz, thump_decay, brightness, gain):
    """A gunshot: low-passed noise (brightness 0..1 keeps that much of the new sample) plus a decaying low sine."""
    noise = Noise(seed)
    samples = []
    low = 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        low += brightness * (noise.next() - low)
        crack = low * math.exp(-t * noise_decay)
        thump = math.sin(2.0 * math.pi * thump_hz * t) * math.exp(-t * thump_decay)
        samples.append(gain * (0.75 * crack + 0.6 * thump))
    return samples


def click(seconds, seed, hz, decay, gain, offset=0.0):
    """A metallic click: a short ringing tone with a noise transient, starting at offset seconds."""
    noise = Noise(seed)
    samples = [0.0] * int(offset * RATE)
    for i in range(int(seconds * RATE)):
        t = i / RATE
        ring = math.sin(2.0 * math.pi * hz * t) * math.exp(-t * decay)
        tick = noise.next() * math.exp(-t * decay * 3.0)
        samples.append(gain * (0.6 * ring + 0.4 * tick))
    return samples


def mix(*tracks):
    length = max(len(track) for track in tracks)
    return [sum(track[i] for track in tracks if i < len(track)) for i in range(length)]


def whoosh(seconds, seed, gain):
    """A throw: band-limited noise swelling and fading."""
    noise = Noise(seed)
    samples = []
    low = 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        low += 0.08 * (noise.next() - low)
        envelope = math.sin(math.pi * t / seconds) ** 2
        samples.append(gain * low * envelope * 3.0)
    return samples


def beep(seconds, hz, gain):
    """The bomb's beep: a pure tone with a short attack and release."""
    samples = []
    count = int(seconds * RATE)
    for i in range(count):
        t = i / RATE
        envelope = min(1.0, i / 60.0, (count - i) / 60.0)
        samples.append(gain * envelope * math.sin(2.0 * math.pi * hz * t))
    return samples


def explosion(seconds, seed, gain):
    """An explosion: a sharp noise crack over a long, very low rumble."""
    noise = Noise(seed)
    samples = []
    low = 0.0
    lower = 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        n = noise.next()
        low += 0.25 * (n - low)
        lower += 0.02 * (n - lower)
        crack = low * math.exp(-t * 14.0)
        rumble = lower * 5.0 * math.exp(-t * 2.2)
        boom = math.sin(2.0 * math.pi * 38.0 * t) * math.exp(-t * 4.0)
        samples.append(gain * (0.5 * crack + 0.6 * rumble + 0.5 * boom))
    return samples


class Biquad:
    """A second-order filter (RBJ's audio EQ cookbook): 'low', 'high' or 'band' (constant 0 dB peak) at hz, Q q."""

    def __init__(self, kind, hz, q=0.707):
        w = 2.0 * math.pi * hz / RATE
        alpha = math.sin(w) / (2.0 * q)
        cos_w = math.cos(w)
        if kind == "low":
            b = ((1.0 - cos_w) / 2.0, 1.0 - cos_w, (1.0 - cos_w) / 2.0)
        elif kind == "high":
            b = ((1.0 + cos_w) / 2.0, -(1.0 + cos_w), (1.0 + cos_w) / 2.0)
        else:
            b = (alpha, 0.0, -alpha)
        a0 = 1.0 + alpha
        self.b = tuple(x / a0 for x in b)
        self.a = (-2.0 * cos_w / a0, (1.0 - alpha) / a0)
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0

    def process(self, x):
        y = self.b[0] * x + self.b[1] * self.x1 + self.b[2] * self.x2 - self.a[0] * self.y1 - self.a[1] * self.y2
        self.x2, self.x1 = self.x1, x
        self.y2, self.y1 = self.y1, y
        return y


def filtered(samples, *filters):
    """Samples through the filters, in order."""
    out = []
    for s in samples:
        for f in filters:
            s = f.process(s)
        out.append(s)
    return out


def normalized(samples, peak):
    """Samples scaled so their largest magnitude is peak (the level each new sound is written at)."""
    top = max(abs(s) for s in samples) or 1.0
    return [s * peak / top for s in samples]


def uniform(noise):
    """A number in [0, 1) from the noise source."""
    return (noise.next() + 1.0) * 0.5


def hit(seconds, seed, thud=None, band=None, rings=(), grains=None, heel=None):
    """A strike on a surface, the base of the steps and the impacts:

    thud    (hz, decay, amp): a low sine;
    band    (kind, hz, q, decay, amp): white noise through a filter (the surface's scuff, crack or puff);
    rings   [(hz, decay, amp), ...]: the surface's partials (tile, metal, a helmet);
    grains  (count, spread, hz, amp): short noise grains through a band in the first `spread` seconds (gravel, sand);
    heel    (delay, scale): the same strike again `delay` seconds later, `scale` as loud (a step's toe after its heel).
    """
    noise = Noise(seed)
    count = int(seconds * RATE)
    out = [0.0] * count

    def strike(start, scale):
        filt = Biquad(band[0], band[1], band[2]) if band else None
        for i in range(start, count):
            t = (i - start) / RATE
            s = 0.0
            if thud:
                s += thud[2] * math.sin(2.0 * math.pi * thud[0] * t) * math.exp(-t * thud[1])
            if filt:
                s += band[4] * filt.process(noise.next()) * math.exp(-t * band[3])
            for hz, decay, amp in rings:
                s += amp * math.sin(2.0 * math.pi * hz * t) * math.exp(-t * decay)
            out[i] += scale * s

    strike(0, 1.0)
    if heel:
        strike(int(heel[0] * RATE), heel[1])
    if grains:
        number, spread, hz, amp = grains
        for _ in range(number):
            start = int(uniform(noise) * spread * RATE)
            level = amp * (0.4 + 0.6 * uniform(noise))
            filt = Biquad("band", hz * (0.7 + 0.6 * uniform(noise)), 1.5)
            for i in range(start, min(count, start + int(0.006 * RATE))):
                t = (i - start) / RATE
                out[i] += level * filt.process(noise.next()) * math.exp(-t * 500.0)
    # A few milliseconds of fade at the end: no click where the file stops.
    fade = int(0.004 * RATE)
    for i in range(max(0, count - fade), count):
        out[i] *= (count - i) / fade
    return out


# The footsteps (CS: pl_step on concrete, pl_dirt, pl_tile, pl_metal; wood has its own knock here), a heel and a toe
# each; the right foot a little lower and later than the left. Quieter than the shots (a peak of 0.45).
STEP_SURFACES = {
    "Concrete": dict(thud=(95.0, 50.0, 0.8), band=("band", 1800.0, 0.8, 70.0, 1.6), heel=(0.035, 0.45)),
    "Dirt": dict(thud=(70.0, 40.0, 0.5), band=("low", 900.0, 0.7, 30.0, 1.0), grains=(26, 0.09, 2600.0, 1.4),
                 heel=(0.04, 0.5)),
    "Tile": dict(thud=(110.0, 55.0, 0.6), band=("high", 2500.0, 0.8, 110.0, 1.1),
                 rings=((2150.0, 60.0, 0.22), (3460.0, 80.0, 0.12)), heel=(0.03, 0.5)),
    "Metal": dict(thud=(120.0, 40.0, 0.6), band=("band", 3000.0, 1.2, 90.0, 0.6),
                  rings=((523.0, 16.0, 0.22), (1287.0, 20.0, 0.16), (2211.0, 26.0, 0.1)), heel=(0.04, 0.4)),
    "Wood": dict(thud=(150.0, 30.0, 0.9), band=("band", 700.0, 1.4, 45.0, 1.4), rings=((235.0, 28.0, 0.25),),
                 heel=(0.04, 0.45)),
}


def step_sound(surface, right):
    """A step on a surface: the left foot's, or the right foot's (another seed, 6 % lower, the toe later)."""
    params = dict(STEP_SURFACES[surface])
    if right:
        if "thud" in params:
            hz, decay, amp = params["thud"]
            params["thud"] = (hz * 0.94, decay, amp)
        delay, scale = params["heel"]
        params["heel"] = (delay * 1.2, scale * 0.9)
    seed = 101 + 17 * sorted(STEP_SURFACES).index(surface) + (7 if right else 0)
    return normalized(hit(0.2, seed, **params), 0.45)


def ladder_step(seed):
    """A step on a wooden ladder's rung (CS: pl_ladder): a hollow knock and a short creak."""
    knock = hit(0.22, seed, thud=(190.0, 35.0, 0.7), band=("band", 900.0, 2.0, 50.0, 1.2),
                rings=((330.0, 22.0, 0.2), (512.0, 30.0, 0.1)))
    noise = Noise(seed + 1)
    creak = Biquad("band", 1400.0, 6.0)
    for i in range(int(0.02 * RATE), int(0.1 * RATE)):
        t = i / RATE - 0.02
        buzz = 1.0 if (i // 18) % 2 == 0 else -1.0
        knock[i] += 0.25 * creak.process(buzz + 0.3 * noise.next()) * math.sin(math.pi * t / 0.08)
    return normalized(knock, 0.4)


def ricochet(seconds, seed):
    """A bullet off metal (CS: ric*.wav): a crack, then a ring gliding down in pitch."""
    noise = Noise(seed)
    crack = Biquad("high", 2000.0)
    samples = []
    phase = 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        hz = 2700.0 * (1.0 - 0.35 * t / seconds)
        phase += 2.0 * math.pi * hz / RATE
        ring = 0.35 * math.sin(phase) * math.exp(-t * 9.0) + 0.12 * math.sin(2.37 * phase) * math.exp(-t * 14.0)
        samples.append(ring + 1.2 * crack.process(noise.next()) * math.exp(-t * 120.0))
    return samples


def shatter(seconds, seed):
    """A bullet through glass: a crack and a shower of high tinkles fading out."""
    noise = Noise(seed)
    count = int(seconds * RATE)
    out = hit(seconds, seed, band=("high", 3000.0, 0.7, 60.0, 1.2))
    for _ in range(40):
        start = int((uniform(noise) ** 2) * 0.8 * count)
        hz = 2500.0 + 4000.0 * uniform(noise)
        level = 0.25 * (1.0 - start / count)
        for i in range(start, min(count, start + int(0.05 * RATE))):
            t = (i - start) / RATE
            out[i] += level * math.sin(2.0 * math.pi * hz * t) * math.exp(-t * 90.0)
    return out


IMPACTS = {
    # A chip of concrete: a bright crack and a spray of grit.
    "Impact_Concrete": lambda: hit(0.22, 201, thud=(160.0, 60.0, 0.4), band=("band", 2600.0, 0.9, 80.0, 2.0),
                                   grains=(18, 0.12, 3500.0, 0.8)),
    # Into sand: a dull puff.
    "Impact_Dirt": lambda: hit(0.2, 211, thud=(80.0, 35.0, 0.6), band=("low", 1200.0, 0.7, 25.0, 1.4),
                               grains=(14, 0.1, 1800.0, 0.6)),
    # Tile: a crack with a ceramic ring.
    "Impact_Tile": lambda: hit(0.22, 221, band=("high", 2200.0, 0.8, 90.0, 1.6),
                               rings=((2870.0, 45.0, 0.3), (4150.0, 60.0, 0.2))),
    "Impact_Metal": lambda: ricochet(0.4, 231),
    # Wood: a hollow thock and splinters.
    "Impact_Wood": lambda: hit(0.22, 241, thud=(210.0, 30.0, 0.8), band=("band", 900.0, 1.5, 40.0, 1.5),
                               rings=((460.0, 35.0, 0.2),), grains=(8, 0.08, 2400.0, 0.5)),
    "Impact_Glass": lambda: shatter(0.4, 251),
    # A character (CS: bhit_flesh, bhit_kevlar, bhit_helmet).
    "Hit_Flesh": lambda: hit(0.16, 261, thud=(75.0, 40.0, 0.9), band=("low", 600.0, 0.7, 35.0, 1.6)),
    "Hit_Kevlar": lambda: hit(0.16, 271, thud=(95.0, 45.0, 0.8), band=("band", 1400.0, 0.8, 50.0, 1.4)),
    "Hit_Helmet": lambda: hit(0.3, 281, thud=(140.0, 50.0, 0.5), band=("high", 2500.0, 0.7, 120.0, 1.0),
                              rings=((1830.0, 14.0, 0.35), (2960.0, 20.0, 0.2), (4410.0, 30.0, 0.1))),
}


def radio(tones, seed):
    """A radio message (CS speaks them; these are tones): a squelch, the tones ((hz, seconds, gap) each, a little
    square) and a squelch, over a hiss, through a radio's band (300 to 3000 Hz)."""
    noise = Noise(seed)
    samples = []

    def squelch(seconds, level):
        for i in range(int(seconds * RATE)):
            t = i / RATE
            samples.append(level * noise.next() * math.exp(-t * 30.0))

    squelch(0.06, 0.5)
    for hz, seconds, gap in tones:
        count = int(seconds * RATE)
        ramp = int(0.006 * RATE)
        for i in range(count):
            t = i / RATE
            envelope = min(1.0, i / ramp, (count - i) / ramp)
            wave_ = math.sin(2.0 * math.pi * hz * t) + 0.3 * math.sin(6.0 * math.pi * hz * t)
            samples.append(0.5 * envelope * wave_ + 0.03 * noise.next())
        for i in range(int(gap * RATE)):
            samples.append(0.03 * noise.next())
    squelch(0.08, 0.4)
    return normalized(filtered(samples, Biquad("high", 300.0), Biquad("low", 3000.0)), 0.35)


RADIO = {
    # radio1, the commands: two tones rising.
    "Radio_Command": lambda: radio(((880.0, 0.09, 0.03), (1175.0, 0.12, 0.0)), 301),
    # radio2, the group's commands: three quick tones.
    "Radio_Group": lambda: radio(((988.0, 0.06, 0.03), (988.0, 0.06, 0.03), (1319.0, 0.1, 0.0)), 311),
    # radio3, the responses and reports: two tones falling.
    "Radio_Report": lambda: radio(((1175.0, 0.09, 0.03), (880.0, 0.12, 0.0)), 321),
    # "Fire in the hole!": an urgent warble.
    "Radio_FireInTheHole": lambda: radio(((1568.0, 0.05, 0.015), (1047.0, 0.05, 0.015)) * 3, 331),
    # "Bomb has been planted.": low, high, low.
    "Radio_BombPlanted": lambda: radio(((659.0, 0.12, 0.03), (1319.0, 0.12, 0.03), (659.0, 0.16, 0.0)), 341),
}


SOUNDS = {
    "Pistol_Fire": lambda: shot(0.35, 11, 28.0, 140.0, 22.0, 0.55, 0.8),
    "Rifle_Fire": lambda: shot(0.4, 47, 22.0, 95.0, 18.0, 0.45, 0.85),
    "Sniper_Fire": lambda: shot(0.9, 83, 9.0, 60.0, 7.0, 0.35, 0.95),
    "Reload": lambda: mix(click(0.08, 3, 1900.0, 60.0, 0.5), click(0.1, 5, 1200.0, 45.0, 0.6, 0.45),
                          click(0.1, 7, 2300.0, 60.0, 0.5, 0.9)),
    "Empty": lambda: click(0.06, 9, 2600.0, 90.0, 0.45),
    "Equip": lambda: mix(click(0.08, 13, 1500.0, 50.0, 0.45), click(0.08, 17, 2100.0, 55.0, 0.4, 0.12)),
    "Throw": lambda: whoosh(0.35, 19, 0.6),
    "Explosion": lambda: explosion(1.6, 23, 0.9),
    "Bomb_Beep": lambda: beep(0.08, 1760.0, 0.5),
    "Bomb_Plant": lambda: mix(click(0.06, 29, 1300.0, 70.0, 0.5), click(0.06, 31, 1300.0, 70.0, 0.5, 0.15),
                              beep(0.25, 880.0, 0.35)),
    "Bomb_Defuse": lambda: mix(click(0.08, 37, 2200.0, 55.0, 0.5), beep(0.3, 660.0, 0.3)),
    "Step_Ladder_1": lambda: ladder_step(401),
    "Step_Ladder_2": lambda: ladder_step(409),
}
for _surface in STEP_SURFACES:
    SOUNDS["Step_%s_L" % _surface] = (lambda s: lambda: step_sound(s, False))(_surface)
    SOUNDS["Step_%s_R" % _surface] = (lambda s: lambda: step_sound(s, True))(_surface)
SOUNDS.update({name: (lambda f: lambda: normalized(f(), 0.5))(make) for name, make in IMPACTS.items()})
SOUNDS.update(RADIO)


def write_wav(name, samples):
    peak = max(1.0, max(abs(s) for s in samples))
    frames = b"".join(struct.pack("<h", int(round(32767.0 * s / peak))) for s in samples)
    path = os.path.join(HERE, name + ".wav")
    with wave.open(path, "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(frames)
    print("wrote", os.path.relpath(path))


def main():
    for name in sorted(SOUNDS):
        write_wav(name, SOUNDS[name]())


if __name__ == "__main__":
    main()
