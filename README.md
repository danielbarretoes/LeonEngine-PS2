# Leon

Monorepo con builds independientes por app (la raíz no se configura):

| | **Editor** | **Project** | **Tools** |
| --- | --- | --- | --- |
| Carpeta | [`Editor/`](Editor/) | [`Projects/<nombre>/`](Projects/) | [`Tools/`](Tools/) |
| Build | `Editor/build` | `Projects/<nombre>/build` | `Tools/build` |
| Script | [`Scripts/build.bat`](Scripts/build.bat) | `cmake -S Projects/Smoke …` | `cmake -S Tools …` |

Docs: [`ARCHITECTURE`](Docs/ARCHITECTURE.md) · [`NAMING`](Docs/NAMING.md) · [`TOOLS`](Docs/TOOLS.md) · [`SETUP`](Docs/SETUP.md) · [`LEVELS`](Docs/LEVELS.md) · [`ASSET_FORMATS`](Docs/ASSET_FORMATS.md) · [`EDITOR`](Docs/EDITOR.md)

---

## Abrir el editor

```bat
Scripts\build.bat
Editor\build\Release\LeonEngine.exe
```

Incremental rápido (Ninja, sin tests):

```bat
Scripts\build-fast.bat
Editor\build-fast\LeonEngine.exe
```

Paquete portable (recomendado para “release”):

```bat
Scripts\package-editor.bat
Dist\LeonEditor\LeonEngine.exe
```

Con Ninja + tests + compile DB para clangd (Editor y Tools; ver [SETUP — IDE](Docs/SETUP.md#ide--clangd-cursor)):

```bat
Scripts\configure-ninja.bat
Editor\build-ninja\LeonEngine.exe
```

| Script | Uso |
| --- | --- |
| `Scripts\test.bat` | Unit tests (`Editor/build-ninja`) |
| `Scripts\cook.bat <recipe.json>` | Offline skeletal cook |
| `Scripts\format.bat` / `lint.bat` | clang-format (omite árboles `build*`) |

Detalle: [`Docs/SETUP.md`](Docs/SETUP.md#scripts-scripts).

---

## Smoke project (Runtime + Engine, sin Editor)

```bat
cmake -S Projects/Smoke -B Projects/Smoke/build
cmake --build Projects/Smoke/build --config Release
Projects\Smoke\build\Release\leon-smoke.exe
```

---

## Layout

| Path | Rol |
| --- | --- |
| `Engine/` | Módulos físicos + `Assets/` |
| `Runtime/` | Host fino (Application/Project/WorldRuntime/Input) |
| `Editor/` | App + paneles + PIE (`Gameplay/PieGameMode`) |
| `Tools/` | `leon-cook`, `leon-cli`, ResourceTools ([TOOLS](Docs/TOOLS.md)) |
| `Scripts/` | Build / cook / test / format |
| `Plugins/` | RHI OpenGL, Physics Arcade |
| `Projects/` | Packs de juego |
| `Templates/` | Plantillas de **proyecto** (Blank, ThirdPerson) |
| `ThirdParty/` | Deps vendored |
| `Docs/` | Arquitectura, setup, levels, formatos |
| `Tests/` | Catch2 |

---

## Changelog

[CHANGELOG.md](CHANGELOG.md)
