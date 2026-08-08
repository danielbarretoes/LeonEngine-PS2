# Ps2Cube

PS2 **3D scene** starter — view target, directional light, materials + textures, FPS/ms HUD.

Editor-ready naming ([Docs/NAMING.md](../../Docs/NAMING.md)): POD fields `Location` / `BaseColor` / `Intensity`, content `T_*_D` / `M_*`, no Unreal `U`/`A`/`F` prefixes. Frame contract: clear → set view/lights → bind material → draw → HUD → swap.

## Features

- **ViewTarget** + **DirectionalLight** (orbiting sun; pad yaw / intensity)
- **Materials** (`M_Ground`, `M_Cube`): `BaseColor`, `BaseColorMap`, `EShadingModel::DefaultLit`
- **Textures** (embedded procedural, Content names): `T_Grid_D`, `T_Checker_D`
- **HUD**: `FPS` + work `ms` (5×7 DebugOverlay-lite)

## Build / run

```powershell
.\Scripts\build-ps2-docker.ps1 cube
```

PCSX2 → **File → Run ELF** → `Projects/Ps2Cube/build-ps2/leon-Ps2Cube.elf`

| Input | Action |
| --- | --- |
| (idle) | Object auto-spin; sun orbits |
| D-Pad | Object yaw / pitch |
| L1 / R1 | Orbit speed |
| L2 / R2 | Sun yaw |
| Square / Triangle | Sun intensity −/+ |
| Cross | Reset |
| Start | Quit |

## Layout

| Path | Role |
| --- | --- |
| `main.cpp` | Window create / destroy |
| `Ps2CubeDemo.*` | Scene loop (authoring POD → RHI submit) |
| `Content/Textures/` | `T_Checker_D` / `T_Grid_D` (embedded at runtime) |
| `Content/Materials/` | Logical `M_Cube` / `M_Ground` |
| RHI | `Ps2DrawBox`, `Ps2Texture`, `Ps2Material` (`Plugins/RHI/PS2`) |
