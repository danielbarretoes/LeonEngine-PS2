"""Synthesizes ShooterGame's weapon sounds as 16-bit PCM WAV files for LeonEd's sound import.

    python3 Game/ShooterGame/SourceArt/Sounds/make_sounds.py

Writes Pistol_Fire.wav, Rifle_Fire.wav, Sniper_Fire.wav, Reload.wav, Empty.wav, Equip.wav, Throw.wav and
Explosion.wav next to this script; Game/ShooterGame/SourceArt/ImportList.ini imports them as /Game/Sounds/S_<Name>
(DefaultGame.ini names them in each weapon's section).

Every sound is made here from noise and sine waves (22050 Hz, mono): a shot is a burst of filtered noise over a low
thump, decaying fast; the explosion a long low rumble; the mechanical sounds short clicks. The noise comes from a
fixed linear congruential generator, so the files are the same on every run and every machine (the reimport gate G5).
Only the Python standard library is used; no recorded or external audio (Game/ShooterGame/SourceArt/LICENSES.md).
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
}


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
