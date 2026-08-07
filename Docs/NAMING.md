# Leon Engine — naming conventions

Canonical rules for folders, files, types, includes, and CMake targets.

**Order in this doc:** Engine → Runtime → Editor → Templates → Tools  
**Also:** [ARCHITECTURE.md](ARCHITECTURE.md) · [SETUP.md](SETUP.md) · [LEVELS.md](LEVELS.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md)

New code **must** follow these rules. Public free functions and Core path helpers are PascalCase only (no camelCase aliases).

---

## Global rules

| Kind | Convention | Examples |
| --- | --- | --- |
| Repo top folders | PascalCase | `Engine/`, `Editor/`, `Runtime/`, `Templates/`, `Tools/` |
| C++ source / headers | **PascalCase** basename = primary type | `Actor.h`, `LevelLoader.cpp` |
| snake_case in C++ filenames | **Forbidden** | — |
| Include (public) | `#include <leon/...>` angle brackets | `<leon/gameplay/Actor.h>` |
| Relative includes `"../Foo.h"` | **Forbidden** in Engine / Editor public code | — |
| Namespace root | `leon` | — |
| Types (class / struct) | PascalCase, **no** Unreal `U`/`A`/`F` prefixes | `Actor`, `World`, `HitResult` |
| Interfaces | `I` + PascalCase | `IRHIDevice`, `IPhysicsBackend` |
| Enums (new API) | `E` + PascalCase, prefer `enum class` + `uint8_t` | `ECameraMode`, `ENetMode`, `EKey`, `EPadButton`, `EPlatform` |
| Constants | `k` + PascalCase | `kMaxSkinBones`, `kProtocolMagic` |
| Public methods (Core / Gameplay / Scene / Level I/O) | **PascalCase** | `SpawnActor`, `GetActorLocation`, `ResolveAssetPath` |
| Free functions | **PascalCase** | `LoadLevelFile`, `MakeCube`, `ResolveAssetPath` |
| Public Unreal-like fields | PascalCase | `RelativeLocation`, `RelativeRotation` |
| Private members | trailing `_`; camelCase or snake ok if consistent in file | `world_`, `pendingKill_` |
| Low-level GPU accessors | PascalCase (`Create` / `Destroy` / `Valid` / `Bind`) | `Shader::Create` |
| Acronyms in identifiers | Treat as word: `Gpu`, `Gltf`, `Aabb`; keep `IO`, `HUD`, `RHI` as established | `GpuPassTimer`, `LightmapIO`, `IRHIDevice` |
| CMake **library** targets | `leon_<module>` snake_case | `leon_core`, `leon_editor` |
| CMake **alias** | `Leon::<Module>` | `Leon::Core`, `Leon::Runtime` |
| CMake **executables** | `leon-<name>` kebab-case targets; product `OUTPUT_NAME` may differ | target `leon-editor` → `LeonEngine.exe`; `leon-cook`, `leon-smoke` |
| CMake `project()` | `Leon` + Pascal | `LeonEditor`, `LeonTools`, `LeonSmoke` |
| Scripts | kebab-case | `build-fast.bat`, `configure-ninja.bat` |
| Content kind folders (disk) | **PascalCase** (Unreal Content Browser) | `Materials/`, `Textures/`, `LevelTemplates/` |
| English identifiers / comments in code | Required | Docs may be EN |

**Do not** use Unreal reflection prefixes (`U`, `A`, `F`, `T`) on Leon types. Unreal influence is gameplay shape + PascalCase content paths, not UObject naming.

**Forbidden on disk content paths:** kebab-case kind folders (`LevelTemplates/`), smashed lowercase (`leveltemplates/`), spaces in filenames. Scripts/CLI stay kebab-case — that is not content.

---

## 1. Engine

### 1.1 Physical modules vs include paths

Code lives under PascalCase module folders; **public headers** live under `Engine/include/leon/<subdir>/`. Subdirs are **lowercase** and stable (do not rename lightly).

| Disk folder | Include dir | CMake target | Notes |
| --- | --- | --- | --- |
| `Engine/Core/` | `leon/core/` | `leon_core` | |
| `Engine/Platform/` | `leon/core/` | `leon_platform` | No `leon/platform/`; Window / MemoryStats |
| `Engine/Scene/` | `leon/level/` | `leon_scene` | Level POD / load / LightmapIO — bake is Editor |
| `Engine/Gameplay/` | `leon/gameplay/` | `leon_gameplay` | Actors, World, GameMode |
| `Engine/Renderer/` | `leon/render/` | `leon_renderer` | CPU + iface; GPU in Plugins; no DCC import |
| `Engine/Import/` | `leon/import/` | `leon_import` | OBJ/FBX/glTF cook — Editor + Tools only |
| `Engine/Network/` | `leon/net/` | `leon_network` | |
| `Engine/Audio/` | `leon/audio/` | `leon_audio` | miniaudio backend |
| `Engine/Content/` | `leon/content/` | `leon_assets` | Target name ≠ folder (`assets`) |
| `Engine/Animation/` | `leon/animation/` | `leon_animation` | |
| `Engine/Serialization/` | `leon/serialization/` | `leon_serialization` | |
| `Engine/Utilities/` | `leon/ui/` (+ `leon/debug/`) | `leon_utilities` | HUD / widgets / debug headers |
| `Engine/RHI/`, `Engine/Physics/` | `leon/rhi/`, `leon/physics/` | iface targets | README under module folder; **headers** under `Engine/include/leon/{rhi,physics}/`; impl in Plugins |
| `Engine/Assets/` | — (content) | — | Not C++ |

Umbrella headers at `Engine/include/leon/`: `Engine.h`, `Gameplay.h`, `Physics.h`.

Source layout: `Engine/<Module>/src/<Type>.cpp` ↔ `Engine/include/leon/<subdir>/<Type>.h`.

### 1.2 Namespaces

| Namespace | Use |
| --- | --- |
| `leon` | Default for gameplay, level, most types |
| `leon::rhi` | RHI device / GPU contracts |
| `leon::net` | Packets / protocol |
| `leon::serialization` | JSON helpers |
| `leon::InputActions` | Action name constants (existing) |

Prefer new nested namespaces **lowercase** (`rhi`, `net`). Avoid new `leon::PascalNested` unless matching an existing pattern.

### 1.3 Types and API

| Area | Rule |
| --- | --- |
| Gameplay / Scene / Level authoring types | PascalCase methods; Unreal-like **PascalCase** fields on `CharacterMovement` and SceneComponents |
| Core (`Window`, `Camera`, `Paths`, `PlayInputTarget`) | PascalCase (`Create`, `ViewMatrix`, `Resolve`, `SetMouseLookActive`) |
| New free functions | PascalCase (`LoadLevelLightmaps`) |
| Render / debug public frame API (`Renderer`, `DebugOverlay`, `DebugDraw`) | PascalCase (`Initialize`, `BeginFrame`, `DrawScene`, `GetDebugOverlay`) |
| RHI / GPU resource accessors | PascalCase (`Create` / `Destroy` / `Valid` / `Bind` / `Set*`) on Shader, Texture, meshes, targets |
| Level visual POD | `Level::StaticMeshComponent` is **not** a gameplay `ActorComponent` / `UStaticMeshComponent` |
| Abbreviations | Prefer full words in public API (`Skeletal…` over `Skel…`); keep established `PhysScene` |

### 1.4 Assets under `Engine/Assets/`

Naming is **hybrid by domain** (Unreal-aligned content, C++/CLI norms elsewhere):

| Domain | Convention |
| --- | --- |
| Content kind folders / virtual catalog | **PascalCase** |
| Asset basenames | Unreal prefixes (`M_`, `T_*_D`, `LM_`) |
| Shader sources (`.vert` / `.frag`) | `snake_case` |
| C++ modules / types / files | PascalCase |
| Includes `leon/...` | lowercase |
| Scripts / exe CMake targets | kebab-case |
| CMake libs | `leon_*` |

**Kind folders (disk)** — same under `Content/assets/…` in Templates and Projects:

| Folder | Contents |
| --- | --- |
| `Materials/` | `.lmat` |
| `Shaders/` | `.vert` / `.frag` |
| `Textures/` | images (`T_*`) |
| `Hdr/` | environment maps |
| `LevelTemplates/` | New Level `.llev` seeds |
| `Anims/` | cooked clips (character packs) |
| `Audio/` | WAV/OGG UI cues + `Music/` beds (`Content/assets/Audio/…`) |
| `Lightmaps/` | baked `.lm` beside levels |
| `Levels/` | project map `.llev` |

**Virtual catalog** matches Unreal-style segments: `leon:Engine/BasicShapes/Cube`, `leon:Engine/Materials/M_Default`, `leon:Engine/Hdr/DefaultSky`.

| Asset kind | File naming |
| --- | --- |
| Shaders | `snake_case.vert` / `.frag` | `blinn_phong.vert` |
| Materials | `M_<Name>.lmat` | `M_Default` |
| Textures | `T_<Name>_<suffix>.png` | `T_Default_D` (diffuse), `T_Wood_N` (normal) |
| HDR / env maps | PascalCase, no spaces | `AutumnFieldPuresky1k.hdr` |
| Level templates | PascalCase `.llev` | `Blank.llev`, `Starter.llev` |
| Lightmaps (baked) | `LM_<id>.lm` | see [LEVELS.md](LEVELS.md) |
| BlendSpace1D desc | `*_Locomotion.blendspace1d.json` | `Bot_Locomotion.blendspace1d.json` |
| Cooked skeletal | short extensions | `.lskel`, `.lskm`, `.lanim` — [ASSET_FORMATS.md](ASSET_FORMATS.md) |
| DCC sources (FBX/OBJ/…) | PascalCase, **no spaces** | `BreathingIdle.fbx` |

Texture suffixes: `_D` diffuse / base color, `_N` normal, `_M` metallic (or mask), `_R` roughness — match [ASSET_FORMATS.md](ASSET_FORMATS.md) examples.

### 1.5 Engine ownership boundaries

- Engine owns framework bases (`GameMode`, `Character`, `PlayerController`, `DefaultGameMode`).
- Shared Unreal-lite helpers (do not reimplement in packs): `PrepareMatchWorld` / `RebuildNavigation` / `SnapCharacterToFloor`; `ApplyPointDamage` / `ApplyRadialDamage`; `GameplayStatics` traces; `VolumeHelpers`; `ArenaCamera`; `InteractionPromptWidget`; `PlayerController` button latches; `PlayerState` Lives.
- Third-person gameplay classes live in `Templates/ThirdPerson` (generated into the project).
- Editor PIE: `GameHostSession` + first-party `leon_*_gameplay` (`RegisterModes`); `PieGameMode` is fallback only.
- Shipping Projects `add_subdirectory(Runtime)` after Engine and link `leon_runtime` + pack gameplay lib, never `leon_editor`. Editor also adds Runtime for PIE; Tools do not.

### 1.5.1 Damage / volumes / arena camera aliases

| Leon | Unreal analogue |
| --- | --- |
| `ApplyPointDamage` / `ApplyRadialDamage` | `UGameplayStatics::ApplyPointDamage` / `ApplyRadialDamage` |
| `PainCausingVolume` (level POD) | `APainCausingVolume` |
| `TriggerVolume` (level POD) | `ATriggerVolume` + interact |
| `AISpawnPoint` | Custom AI start (not `APlayerStart`) |
| `ArenaCamera` / `UpdateArenaCamera` | Multi-target party camera framing |
| `Character::TakeDamage` / `Die` / `Revive` | `AActor::TakeDamage` lite on Character |

### 1.6 Known Engine debt (do not spread)

- Free functions are PascalCase (`LoadLevelFile`, `ValidateLevelDocument`, `MakeCube`, `LoadMaterialFile`, …) — do not reintroduce camelCase aliases.
- Enums without `E`: `ShadingModel`, `ValidationSeverity`, …

---

## 2. Runtime

Thin play-time host. Gameplay types stay in Engine.

### 2.1 Layout

```text
Runtime/
├── include/leon/runtime/   # public headers (<leon/runtime/…>); RuntimeInput is header-only here
├── Application/            # GameApplication.cpp, RunLeonGame.cpp
├── Project/                # ProjectPack.cpp
└── WorldRuntime/           # WorldRuntime.cpp
```

| Rule | Detail |
| --- | --- |
| Files | PascalCase, one primary type per file |
| Namespace | `leon::runtime` |
| Entry | `leon::runtime::RunLeonGame` |
| CMake | `leon_runtime` / `Leon::Runtime` only |
| Includes | `#include <leon/runtime/<Type>.h>` |

### 2.2 Rules for new Runtime code

1. Put types in `leon::runtime` under `include/leon/runtime/`.
2. Do not reintroduce `leon::games` or CMake `leon_game_host`.
3. Do not depend on Editor headers.
4. Prefer PascalCase public methods (`GameApplication::Run`).

---

## 3. Editor

Edit-time only. Must remain unlinkable from shipping Projects.

### 3.1 Layout

```text
Editor/
├── include/leon/editor/     # public headers
│   └── panels/              # *Panel.h, modal UI headers
├── Application/             # EditorApp, Layout, Project, Theme, main
├── Gameplay/                # PieGameMode.cpp, PiePackRegistry.cpp
├── Panels/                  # panel .cpp
├── Preview/                 # AssetPreviewPanel.cpp
├── Importers/               # import / file dialogs
├── SceneEditing/            # save, factory, history, commands, LightmapBaker (Build Lights)
├── Gizmos/
├── Resources/               # edit-time brand / fonts / Win32 icon (not Engine/Assets)
│   ├── Brand/               # LeonLogo.png (master), LeonLogoUi.png (UI)
│   ├── Icons/               # LeonEditor.png / .ico / .rc.in
│   └── Fonts/               # Inter-*.ttf
└── pch.h
```

| Rule | Detail |
| --- | --- |
| Public include | `<leon/editor/…>` or `<leon/editor/panels/…>` |
| C++ namespace | `leon::editor` (hard; no `using` into `leon`) |
| Panel headers | Under `panels/` even if `.cpp` lives in `Preview/` or `Importers/` |
| Non-panel editor types | Flat `leon/editor/<Type>.h` (`EditorContext`, `TransformGizmo`, …) |
| Files | PascalCase |
| Lib / exe | `leon_editor` (STATIC) + `leon-editor` → `LeonEngine.exe` |
| Enums | `E*` (`EEditorPlayMode`, `EGizmoOperation`) |
| PIE API | `StartPie` / `StopPie` in EditorApp |

### 3.2 PIE / project templates

| Item | Value |
| --- | --- |
| Template | `Templates/ThirdPerson` → fixed `ThirdPerson*` types (copy as-is on New Project) |
| Editor PIE | `GameHostSession` + pack/`templateId` `RegisterModes`; `PieGameMode` fallback |
| Pack gameplay lib | `leon_<pack>_gameplay` + `leon::packs::<pack>::RegisterModes` |
| Fly camera | `leon::DefaultGameMode` |
| Level document | `gameMode` = `"third-person"` or `"Default"` |

**`editorId`:** session-stable selection id on Actors and level PODs (meshes / lights / PlayerStarts). Shipping may ignore it; do not use for gameplay identity. Full contract: [ARCHITECTURE.md — PIE / editorId](ARCHITECTURE.md#pie--editorid-contract-unreal-like).

### 3.3 Editor notes

- File dialogs: `EditorFileDialog.h` / `Importers/EditorFileDialog.cpp` (`EditorPick*`).
- Lightmaps: bake is Editor-only (`<leon/editor/LightmapBaker.h>`, `BakeLevelLightmaps` — Build Lights). Shipping load uses Engine `<leon/level/LightmapIO.h>`.

---

## 4. Templates

Project seeds copied by the editor hub into `Projects/<name>/`. Distinct from **level** templates (`Engine/Assets/LevelTemplates/`).

### 4.1 Layout

```text
Templates/
├── Blank/           # minimal pack
└── ThirdPerson/     # sample gameplay under src/
```

| Artifact | Convention |
| --- | --- |
| Folder id | PascalCase (`Blank`, `ThirdPerson`) |
| Marker | `leon.game.json` |
| New Project | Copy template folder as-is; stamp `name` / `displayName` / `templateId` only |
| Game Default Map | `defaultLevel` in `leon.game.json` (e.g. `Levels/MainMenu.llev`) — Project Settings → Maps & Modes |
| Default GameMode | `defaultGameMode` in `leon.game.json` — pack fallback when a level has no GameMode Override |
| Template seed id | `templateId` in `leon.game.json` (e.g. `ThirdPerson`) — PIE gameplay registry |
| Default level (templates) | `Levels/Main.llev` when no `defaultLevel` is set |
| Materials | `M_<Name>.lmat` |
| CMake pack / exe | Folder-derived `LEON_PACK_NAME` → `Leon${LEON_PACK_NAME}` / `leon-${LEON_PACK_NAME}` |
| Template gameplay types | Fixed seed names (`ThirdPersonGameMode`, …) — never renamed to the project name |
| ThirdPerson character pack | `assets/characters/bot/` (`Bot.lchar`, `Materials/`, `Textures/`, `Anims/`) |
| Project-local headers | Quoted includes; **not** under `leon/` |
| Project namespace (seed) | `game` |
| Entry | `#include <leon/runtime/RunLeonGame.h>` + `leon::runtime::RunLeonGame` |

### 4.2 GameMode id strings

Prefer one style for **new** router ids:

| Context | Id style | Example |
| --- | --- | --- |
| Engine default | Pascal | `"Default"` |
| Editor PIE | Pascal | `"Pie"` |
| Project / template | kebab-case allowed for product ids | `"third-person"` |

When adding a new shipping GameMode id, prefer **kebab-case** for product packs and keep editor-only modes Pascal (`Pie`).

---

## 5. Tools

Offline CLIs. Shared helpers use `leon/tools/` under `Tools/ResourceTools/include/`. Full behavior: [TOOLS.md](TOOLS.md).

### 5.1 Layout

```text
Tools/
├── AssetPipeline/
│   └── leon-cook/     # folder name = executable name
├── Cli/               # leon-cli (thin forwarder)
└── ResourceTools/     # leon_resource_tools (recipe / paths)
    ├── include/leon/tools/
    └── src/
```

| Rule | Detail |
| --- | --- |
| Executables | `leon-cook`, `leon-cli` (kebab) |
| Shared lib | `leon_resource_tools`; headers `leon/tools/*.h` |
| Tool group folders | PascalCase (`AssetPipeline`, `ResourceTools`, `Cli`) |
| Cook | Links `leon_engine_cook` (+ ResourceTools); includes `leon_import`; **not** full `leon_engine` / `leon_editor` |
| `leon-cli` | No Engine link (forward to sibling `leon-cook`) |
| Engine cook facade | CMake `leon_engine_cook` / `Leon::EngineCook` |
| Import module | CMake `leon_import` / `Leon::Import` — DCC → `.lmesh`; not in shipping `leon_engine` |

---

## 6. Related trees (brief)

| Tree | Naming note |
| --- | --- |
| `Plugins/` | `leon_rhi_opengl`, `leon_physics_arcade`, optional `leon_physics_jolt` + `Leon::RHI_OpenGL` / `Leon::Physics_Arcade` / `Leon::Physics_Jolt`; public headers stay under `Engine/include/leon/` |
| `Projects/` | Same rules as Templates after clone; exe `leon-<name>`; never link Editor |
| `Build/` | Descriptive Pascal / camel CMake files (`LeonCompileOptions.cmake`, `SyncDirectory.cmake`) |
| `Scripts/` | kebab-case `.bat` / `.sh`; Python utilities may be `snake_case.py` |
| `Docs/` | `SCREAMING_SNAKE` or Pascal topic files (`ARCHITECTURE.md`, `NAMING.md`) |

---

## 7. Quick checklist (PR / new file)

1. File name PascalCase and matches primary type?
2. Engine/Editor include uses `<leon/...>`?
3. New public API PascalCase (method / free function)?
4. New enum uses `E` prefix?
5. Lib `leon_*` vs exe `leon-*` correct?
6. Edit-time-only code under `Editor/` (not Engine)?
7. Shipping path free of ImGui / Editor types?
8. Assets follow PascalCase kind folders + `M_` / `T_` / level-template rules?

---

## 8. Completed cleanups (do not regress)

- CamelCase free-function aliases removed (`LoadLevelFile`, `MakeCube`, Paths, …).
- `leon::games` / `leon_game_host` / `Leon::GameHost` removed; use `leon::runtime` / `leon_runtime`.
- Engine enums: `EShadingModel`, `EShaderReloadResult`, `GpuPassTimer::EPass`, `EValidationSeverity` (no `using` alias).
- Editor types live in `leon::editor`.

**Still out of scope:** mass-rename include dirs (`level` ↔ `scene`).

Do not mass-rename includes (`level` ↔ `scene`) without a dedicated migration — those paths are public API.
