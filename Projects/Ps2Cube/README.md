# Ps2Cube

PS2 **3D** starter pack — perspective camera, z-buffer, unlit boxes.

## Build / run

```powershell
.\Scripts\build-ps2-docker.ps1 cube
```

PCSX2 → **File → Run ELF** → `Projects/Ps2Cube/build-ps2/leon-Ps2Cube.elf`

| Input | Action |
| --- | --- |
| (idle) | Auto yaw / pitch |
| D-Pad | Manual rotate (stops auto) |
| L1 / R1 | Orbit speed |
| Cross (A) | Reset |
| Start | Quit |

## Layout

| File | Role |
| --- | --- |
| `main.cpp` | Window create / destroy |
| `Ps2CubeDemo.*` | Camera scene loop |
| RHI | `Ps2DrawUnlitBox` (`Plugins/RHI/PS2/src/Ps2Draw3D.cpp`) |
