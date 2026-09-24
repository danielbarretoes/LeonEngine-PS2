# Leon Engine (PS2-first)

Monorepo **dual-target**:

| | **Host (Windows)** | **Target (PS2 EE)** |
| --- | --- | --- |
| Role | Editor, Tools, cook | Lean Runtime / capability ELF |
| RHI | OpenGL 3.3 | GS (`Plugins/RHI/PS2`) |
| Script | [`Scripts/build.bat`](Scripts/build.bat) | [`Scripts/build-ps2.sh`](Scripts/build-ps2.sh) / [`build-ps2-docker.sh`](Scripts/build-ps2-docker.sh) |
| Output | `Editor/build/Release/LeonEngine.exe` | `Projects/Ps2Cube/build-ps2/leon-Ps2Cube.elf` |

Docs: [`ARCHITECTURE`](Docs/ARCHITECTURE.md) · [`NAMING`](Docs/NAMING.md) · [`SETUP`](Docs/SETUP.md) · [`ASSET_FORMATS`](Docs/ASSET_FORMATS.md) · [`TOOLS`](Docs/TOOLS.md)

Naming follows Unreal-like PascalCase rules in [`Docs/NAMING.md`](Docs/NAMING.md) (`EKey`, `EPadButton`, no `U`/`A`/`F` prefixes).

---

## Host — open the editor

```bat
Scripts\build.bat
Editor\build\Release\LeonEngine.exe
```

Day-to-day: `Scripts\build-fast.bat` → `Editor\build-fast\LeonEngine.exe`

New host packs: File → New Project from `Templates/Blank` or `Templates/ThirdPerson`.

---

## PS2 — toolchain + 3D / 2D packs

Requires [ps2dev](https://github.com/ps2dev/ps2dev) (`PS2DEV` / `PS2SDK`) in WSL2, **or** Docker:

```powershell
.\Scripts\build-ps2-docker.ps1 hello   # Samples/Ps2Hello
.\Scripts\build-ps2-docker.ps1 cube    # Projects/Ps2Cube (3D boxes + z-buffer)
.\Scripts\build-ps2-docker.ps1 lab     # Projects/Ps2Lab (2D capability lab)
```

Run the `.elf` in [PCSX2](https://pcsx2.net/) (`.\Scripts\run-ps2-pcsx2.ps1 cube`, add `-Build` to build first). Details: [SETUP — PS2](Docs/SETUP.md#ps2-emotion-engine).

Canonical **3D** pack: [`Projects/Ps2Cube`](Projects/Ps2Cube/).

---

## Layout

| Path | Role |
| --- | --- |
| `Engine/` | Modules + `Assets/` |
| `Runtime/` | Host play-time |
| `Editor/` | Host editor |
| `Tools/` | `leon-cook`, `leon-cli` (host cook) |
| `Plugins/RHI/OpenGL` | Host GPU |
| `Plugins/RHI/PS2` | EE GS RHI |
| `Projects/Ps2Cube` | Canonical PS2 **3D** pack |
| `Projects/Ps2Lab` | PS2 2D capability lab |
| `Templates/` | New Project seeds (Blank / ThirdPerson) |
| `Build/toolchains/ps2-ee.cmake` | EE toolchain file |
| `Samples/Ps2Hello` | Toolchain-only ELF |

---

## Changelog

[CHANGELOG.md](CHANGELOG.md)
