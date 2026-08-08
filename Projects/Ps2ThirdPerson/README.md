# Ps2ThirdPerson

PS2 **third-person gameplay** starter — orbit camera, character move/jump, primitive sandbox level.

Built for a later **Editor** bridge: pack PODs use Unreal-like fields (`Location`, `Yaw256`, `Scale`, `SpringArm`, materials `M_*` / textures `T_*_D`). Level actors live under `Content/Levels/` (runtime is code-authored until `.llev` load lands on EE).

## Build / run

```powershell
.\Scripts\build-ps2-docker.ps1 tp
```

PCSX2 → **File → Run ELF** → `Projects/Ps2ThirdPerson/build-ps2/leon-Ps2ThirdPerson.elf`

| Input | Action |
| --- | --- |
| Left stick | Move (camera-relative) |
| Right stick | Camera orbit (yaw / pitch) |
| Cross | Jump |

## Layout

| Path | Role |
| --- | --- |
| `Ps2ThirdPersonDemo.*` | Character + SpringArm + level loop |
| `Content/Levels/` | Future `.llev` (sandbox in code today) |
| `Content/Materials/` | `M_Ground` / `M_Platform` / `M_Crate` / `M_Character` |
| `Content/Textures/` | `T_Grid_D` / `T_Checker_D` |
