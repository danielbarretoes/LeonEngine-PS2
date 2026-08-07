# Leon Engine — setup

## Requirements

| Need | Required for | Notes |
| --- | --- | --- |
| CMake 3.20+ | All builds | Prefer install under `C:\Program Files\CMake\bin` (scripts prepend it) |
| C++20 compiler (MSVC 2022+ / VS 18, or clang/gcc) | All builds | Scripts look for Community / Professional / **Enterprise** |
| Git | FetchContent deps | |
| GPU / driver with **OpenGL 3.3** | Editor / windowed projects | Headless init skips GL |
| [Ninja](https://ninja-build.org/) | Fast incremental, clangd, `test.bat` | `winget install Ninja-build.Ninja` |
| clang-format (VS LLVM) | `format.bat` / `lint.bat` | Bundled with Visual Studio “C++ Clang tools” |

The repo root is **not** a CMake project — configure `Editor`, `Projects/<name>`, or `Tools`.

## Scripts (`Scripts/`)

| Script | Purpose | Output / notes |
| --- | --- | --- |
| `_vsenv.bat` | Shared VS env helper (not run alone) | `vcvars` \| `vsdev` + `quiet` / `need-ninja` / `need-git` |
| `build.bat` | Full Editor (VS generator) + tests | `Editor\build\Release\LeonEngine.exe`, `leon_tests.exe` |
| `build-fast.bat` | Day-to-day Editor (Ninja, tests OFF) | `Editor\build-fast\LeonEngine.exe` |
| `configure-ninja.bat` | Ninja Release + compile DBs + Editor build | `Editor\build-ninja\`, `Tools\build-ninja\` (clangd) |
| `test.bat` | Build `leon_tests` + `ctest` | **Same tree as** `configure-ninja`: `Editor\build-ninja` |
| `cook.bat` | Build + run `leon-cook recipe <json>` | Prefers `Tools\build-ninja`, else VS `Tools\build` |
| `build-project.bat` / `.sh` | CMake-build pack; optional `--with-server` | Editor **Build → Build Game** → `<project>\Shipping\` (+ `*-server` when Project Settings enables it) |
| `package-editor.bat` | Portable Editor + SDK folder (dev/release) | `Dist\LeonEditor\` — run from Scripts, not from the editor UI |
| `make-editor-icon.py` | Rebuild `LeonEditor.ico` from brand logo (BMP for rc.exe) | `python Scripts\make-editor-icon.py` then rebuild editor |
| `format.bat` | clang-format in-place | Includes `Templates/`; skips `build*`, `_deps`, `_leon_*`, `.git` |
| `lint.bat` | format dry-run + Release Editor build | Same dirs as format |
| `build-linux.sh` | Linux Editor + tests | `Editor/build-linux` |

### Editor build trees (do not merge casually)

| Tree | Role | Tests | clangd |
| --- | --- | --- | --- |
| `Editor/build-fast` | Day-to-day (`build-fast.bat`) | OFF | optional local DB |
| `Editor/build-ninja` | Tests + clangd (`test.bat` / `configure-ninja.bat`) | ON when configured for tests | **canonical** (`.clangd`) |
| `Editor/build` | Full VS solution (`build.bat` / `lint.bat`) | ON | no |

VS discovery order: **18** then **2022**, editions Community → Professional → Enterprise (via `_vsenv.bat`). Paths like `Visual Studio\2026\` are **not** used (folder names are `18` / `2022`).

## First build (Editor)

Full Visual Studio solution + tests:

```bat
Scripts\build.bat
Editor\build\Release\LeonEngine.exe
```

## Day-to-day (recommended)

Ninja + Release + `LEON_BUILD_TESTS=OFF` + target `leon-editor` only (output `LeonEngine.exe`):

```bat
Scripts\build-fast.bat
Editor\build-fast\LeonEngine.exe
```

Portable release (editor + SDK for another PC):

```bat
Scripts\package-editor.bat
Dist\LeonEditor\LeonEngine.exe
```

What makes this fast:

- MSVC `/MP` (and `/FS` when using debug info)
- Precompiled header on `leon_editor` (`Editor/pch.h`)
- Asset POST_BUILD via `Build/SyncDirectory.cmake` (`copy_if_different`)
- No Catch2 graph unless you reconfigure with tests ON

Re-run after touching a single `.cpp` — typically seconds, not minutes.

Ninja + tests + `compile_commands.json` (clangd):

```bat
Scripts\configure-ninja.bat
Editor\build-ninja\LeonEngine.exe
```

### IDE / clangd (Cursor)

| Piece | Role |
| --- | --- |
| `.clangd` | `CompilationDatabase: Editor/build-ninja`; `Tools/` → `Tools/build-ninja` |
| `.vscode/settings.json` | clangd `QueryDriver` for `cl.exe`; MS C++ IntelliSense off |
| `Scripts/fix-compile-commands.ps1` | Rewrites `-IC:\Users\...` → forward slashes |

**Why the script exists:** CMake/Ninja emits MSVC commands with backslash include paths. clangd’s command lexer treats `\U` in `\Users` as an escape, so includes (`ufbx.h`, `Editor/include`, …) vanish and you get false “no member / file not found” errors even when `cl.exe` builds fine.

`configure-ninja.bat` configures **Editor** and **Tools** Ninja DBs, builds the Editor, then runs the fixer (copies Editor DB to repo-root `compile_commands.json` for editors that look there). After regenerating: **Developer: Restart Language Server**.

## Projects

```bat
cmake -S Projects/Smoke -B Projects/Smoke/build -G "Visual Studio 18 2026" -A x64
cmake --build Projects/Smoke/build --config Release
Projects\Smoke\build\Release\leon-smoke.exe
```

### Coop LAN (`Projects/CoopTp`)

Flow: **MainMenu** → **Lobby** (pick map) → **Courtyard** or **Rooftops** (2P LAN).

```bat
REM Client only
Scripts\build-project.bat Projects\CoopTp leon-CoopTp

REM Client + headless dedicated (leon-CoopTp-server)
Scripts\build-project.bat Projects\CoopTp leon-CoopTp --with-server
```

Linux (native build on a Linux host — no Win→Linux cross-compile from these scripts):

```bash
Scripts/build-project.sh Projects/CoopTp leon-CoopTp --with-server
```

Or open `Projects/CoopTp` in the Editor to author levels (assign GameMode Override in World Settings; Game Default Map in **Edit → Project Settings**). Enable **Build dedicated headless server** in Project Settings so **Build → Build Game** passes `--with-server` (`leon.game.json` → `buildDedicatedServer`). Full Menu/Lobby/match + net runs in the pack executable — the Editor only embeds `Default` / `third-person` PIE previews.


| Screen | Controls |
| --- | --- |
| Main Menu | Host / Join Listen Host / Join Dedicated / Quit; **Esc** / **Backspace** Quit |
| Lobby (Listen / Dedicated) | Pick map, **Start Match**; **Esc** / **Backspace** → Main Menu |
| Lobby (Client) | Wait for Start Match; **Disconnect** or **Esc** / **Backspace** |
| Match | WASD / mouse / Space; **Esc** / **Backspace** pause / resume; hold **Tab** player list (`GameState::PlayerArray`) |
| Dedicated binary | `Shipping\leon-CoopTp-server.exe` (or `run-dedicated.bat`) — headless by default; optional `--port 7777` / `--tick 60` |
| CLI dedicated (client exe) | `leon-CoopTp.exe --dedicated --port 7777` → Courtyard headless |
| Smoke dedicated | `Scripts\smoke-coop-dedicated.bat` (expects Shipping or build-fast exe) |
| Smoke packs (optional) | `Scripts\smoke-packs.ps1` — CoopTp / Zombies / Furytoon dedicated headless |
| Join Dedicated | Main Menu → Join Dedicated (LAN IP:7777), or `leon-CoopTp.exe --join 192.168.x.x` |

Default port **7777**. Join Dedicated uses the same UDP join path as Join Listen Host; the distinction is UX (you expect a headless `leon-CoopTp-server` already running). Standalone PIE on a gameplay map still supports **H** / **C** / **V** / **F9**. Snapshots replicate pawns + dynamic bodies; **Welcome / Travel** keep clients on the host map.

## Tools

Offline cook / CLI (`cmake -S Tools`). For clangd on Tools sources, prefer the Ninja DB from `configure-ninja.bat` (`Tools/build-ninja`).

```bat
Scripts\cook.bat Templates\ThirdPerson\assets\characters\bot\cook-bot.json
```

`cook.bat` will configure Tools if needed (Ninja when available, otherwise Visual Studio). Equivalents:

```bat
cmake -S Tools -B Tools/build
cmake --build Tools/build --config Release
Tools\build\Release\leon-cook.exe recipe Templates\ThirdPerson\assets\characters\bot\cook-bot.json
Tools\build\Release\leon-cli.exe cook Templates\ThirdPerson\assets\characters\bot\cook-bot.json
```

`leon-cli cook` runs the **sibling** `leon-cook` binary (same output directory; resolved via the CLI module path on Windows).

Full Tools reference (modes, recipe schema, lean link graph, ResourceTools API): **[TOOLS.md](TOOLS.md)**.

Level / lightmap authoring: [LEVELS.md](LEVELS.md). Material / mesh cook formats: [ASSET_FORMATS.md](ASSET_FORMATS.md). Editor panels: [EDITOR.md](EDITOR.md).

## Layout

| Path | Role |
| --- | --- |
| `Engine/` | Libraries + `Assets/` |
| `Editor/` | Level editor (+ PIE via Engine GameModes) |
| `Runtime/` | Thin game host |
| `Plugins/` | OpenGL RHI, Arcade physics, optional Jolt |
| `Projects/` | Game packs |
| `Build/` | Shared CMake helpers |
| `Scripts/` | Build / cook / format / test helpers |
| `Tools/` | `leon-cook`, `leon-cli`, `leon_resource_tools` ([TOOLS.md](TOOLS.md)) |
| `Dist/` | Portable outputs (`LeonEditor/`, `Games/<Name>/`) — not committed |
| `Docs/` | Architecture + authoring |
| `Tests/` | Catch2 |

### Portable game / editor (another PC)

```bat
REM GAME — from editor: Build → Build Game
REM → Projects\<Name>\Shipping\   ← copy this folder to play on another PC

REM EDITOR toolkit — developer script only (not in the editor menu)
Scripts\package-editor.bat
REM → Dist\LeonEditor\
```

**Not in git:** `*/build/`, `*/build-ninja/`, `Editor/build-fast/`, `Tools/build-ninja/`, `compile_commands.json` (see `.gitignore`).

## Tests

```bat
Scripts\test.bat
```

Requires Ninja. Or after an Editor configure with `LEON_BUILD_TESTS=ON`:

```bat
ctest --test-dir Editor/build -C Release -R "^leon\." --output-on-failure
ctest --test-dir Editor/build-ninja -R "^leon\." --output-on-failure
```

## Format / lint

```bat
Scripts\format.bat
Scripts\lint.bat
```

Both skip generated trees under `build`, `build-*`, `_deps`, `_leon_*`, and `.git`.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `cmake -S .` fails | Expected — configure `Editor`, `Projects/<name>`, or `Tools` |
| `VsDevCmd.bat not found` | Install VS 18 or 2022 with C++ workload (any edition) |
| Levels/materials missing next to exe | Rebuild so POST_BUILD syncs `assets/` + `Projects/<pack>/` |
| clangd missing includes / false “no member” | `Scripts\configure-ninja.bat` (fixes paths); Restart Language Server |
| clangd OK on Editor, red on `Tools/` | Same script configures `Tools/build-ninja`; check `.clangd` PathMatch |
| clangd STL1003 / “expected C++ compiler” | DB missing or not C++; re-run configure-ninja; ensure `--query-driver` for `cl.exe` |
| Editor rebuild takes many minutes | Use `Scripts\build-fast.bat`; avoid Debug for daily work; build target `leon-editor` only |
| `Ninja not on PATH` | `winget install Ninja-build.Ninja` and open a new terminal |
| `cook.bat` fails without Ninja | Script falls back to VS `Tools\build`; ensure CMake + VS are installed |
| `leon-cli cook` cannot find leon-cook | Build Tools so both exes sit in the same folder, or use `Scripts\cook.bat` |
| Format touches generated code | Should not — if it does, check path is under a skipped `build*` folder |
| Lightmaps missing in shipping | Bake + save level; runtime loads `.lm` via `LightmapIO` — see [LEVELS.md](LEVELS.md) |

More: [ARCHITECTURE](ARCHITECTURE.md) · [NAMING](NAMING.md) · [TOOLS](TOOLS.md) · [LEVELS](LEVELS.md) · [ASSET_FORMATS](ASSET_FORMATS.md) · [EDITOR](EDITOR.md) · [LIBRARIES](LIBRARIES.md)
