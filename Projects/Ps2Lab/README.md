# Ps2Lab

Canonical PS2 Emotion Engine capability pack.

## What you should see

| Mode | Button | Content |
| --- | --- | --- |
| **Showcase** | Cross (A) | Sky/ground, spinning hero tri, 3 orbiting satellites, HUD |
| **PadPilot** | Circle (B) | Move with D-Pad, L1/R1 rotate, L2/R2 scale |
| **StressGrid** | Square (X) | 6×4 spinning triangles |
| **ClearOnly** | Triangle (Y) | Pulsing clear + HUD |
| Quit | Start | Exit |

Title bar `[?]` is normal for homebrew (no game serial).

## Build / run

```powershell
.\Scripts\build-ps2-docker.ps1 lab
```

PCSX2 → **File → Run ELF** → `Projects/Ps2Lab/build-ps2/leon-Ps2Lab.elf`
