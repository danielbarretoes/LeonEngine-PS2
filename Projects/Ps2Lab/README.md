# Ps2Lab

Canonical PS2 Emotion Engine pack (`leon-Ps2Lab.elf`).

## Layout

| File | Role |
| --- | --- |
| `main.cpp` | Window create / destroy |
| `Ps2LabDemo.*` | Modes + frame loop |
| `EmbeddedSmTriangleLps2.h` | In-binary `SM_Triangle.lps2` |
| `Content/Meshes/SM_Triangle.lps2` | On-disk cook sample |

## Modes

| Mode | Button | Content |
| --- | --- | --- |
| Showcase | Cross (A) | Sky/ground, hero spin, orbiters, HUD |
| PadPilot | Circle (B) | D-Pad move, L1/R1 rotate, L2/R2 scale |
| StressGrid | Square (X) | 6×4 spinning tris |
| ClearOnly | Triangle (Y) | Pulsing clear + HUD |
| Quit | Start | Exit |

PCSX2 title `[?]` is normal (no game serial).

## Build

```powershell
.\Scripts\build-ps2-docker.ps1 lab
```

Output: `Projects/Ps2Lab/build-ps2/leon-Ps2Lab.elf`
