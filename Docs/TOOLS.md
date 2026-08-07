# Offline Tools (`leon-cook`, `leon-cli`, ResourceTools)

Build entry: `cmake -S Tools` (see [SETUP.md](SETUP.md#tools)). Wrapper: `Scripts\cook.bat <recipe.json>`.

## Layout

```text
Tools/
├── AssetPipeline/leon-cook/   # CLI executable (modes below)
├── Cli/                       # leon-cli — thin forwarder (no Engine link)
├── ResourceTools/             # shared library leon_resource_tools
│   ├── include/leon/tools/
│   └── src/
└── CMakeLists.txt
```

| Target | Role |
| --- | --- |
| `leon-cook` | Offline cook executable |
| `leon-cli` | `help` / `version` / `cook <recipe>` → sibling `leon-cook` |
| `leon_resource_tools` | Recipe runner + path helpers |
| `leon_engine_cook` | Engine INTERFACE: lean deps for cook (see below) |

## Link graph (lean cook)

```text
leon-cook
  └─ leon_resource_tools
       └─ leon_engine_cook
            ├─ leon_assets      (+ LevelClassNames, CookedSkeletal, ContentValidator)
            ├─ leon_animation
            ├─ leon_renderer    (CPU `.lmesh` / `.lmat`)
            ├─ leon_import      (OBJ / FBX / glTF → `.lmesh`)
            └─ leon_rhi_opengl  (ResourceCache / Texture for material maps)
```

**Not linked by cook:** `leon_gameplay`, `leon_scene` (LevelDirector/Loader), `leon_network`, `leon_physics_*`, `leon_engine_shell`, full `leon_engine`.

**Not linked by shipping `leon_engine`:** `leon_import` (Editor + cook only).
Class-name parsers (`tryParseBasicShapeName`, `tryParseBasicLightName`, PlayerStart / BlockingVolume) live in `Engine/Content/src/LevelClassNames.cpp` so ContentValidator does not force a Scene link.

POST_BUILD syncs `Engine/Assets` beside `leon-cook` via `Build/SyncDirectory.cmake` (copy-if-different).

## `leon-cook` modes

| Mode | Purpose |
| --- | --- |
| `staticmesh` | `--obj` \| `--fbx` \| `--gltf` → `.lmesh` (glTF: optional `--materials <dir>` for `.lmat` + textures) |
| `character` | Skinned FBX pack → `.lskel` / `.lskm` / anims / blendspace / `.lchar` |
| `anim` | Single clip → `.lanim` (needs skeleton) |
| `recipe` | JSON `steps[]` (types below) |

Formats detail: [ASSET_FORMATS.md](ASSET_FORMATS.md).

### Recipe JSON

Relative paths resolve next to the recipe file (`leon::tools::ResolveBeside`).

```json
{
  "steps": [
    {
      "type": "character",
      "name": "Bot",
      "mesh": "BreathingIdle.fbx",
      "run": "Running.fbx",
      "jump": "JumpingUp.fbx",
      "fall": "FallingIdle.fbx",
      "land": "FallingToLanding.fbx",
      "out": "."
    },
    {
      "type": "staticmesh",
      "gltf": "prop.gltf",
      "out": "Prop.lmesh",
      "materials": "materials"
    },
    {
      "type": "anim",
      "fbx": "Wave.fbx",
      "skeleton": "Bot.lskel",
      "name": "Wave",
      "out": "Anims/Wave.lanim",
      "loop": true
    }
  ]
}
```

| Step `type` | Required fields | Optional |
| --- | --- | --- |
| `character` | `name`, `mesh`, `run` | `out` (default `.`), `jump`, `fall`, `land` |
| `anim` | `fbx`, `skeleton`, `out` | `name`, `loop` (default true) |
| `staticmesh` | `out` + exactly one of `obj` / `fbx` / `gltf` | `materials` (glTF) |

Example pack recipe: `Templates/ThirdPerson/Content/assets/characters/bot/cook-bot.json`.

## ResourceTools API

Headers under `Tools/ResourceTools/include/leon/tools/`:

| API | Role |
| --- | --- |
| `ResolveBeside(baseDir, rel)` | Absolute unchanged; else `baseDir / rel` |
| `RunCookRecipeFile(path)` | Execute recipe; process-style exit code (`0` ok) |

`leon-cook recipe` calls `RunCookRecipeFile`. Lightmap **load** is Engine `LightmapIO`; **bake** is Editor `LightmapBaker` (Build Lights) — [LEVELS.md](LEVELS.md).

## `leon-cli`

| Command | Behavior |
| --- | --- |
| `help` | Usage |
| `version` | `Leon Tools 0.9.0` |
| `cook <recipe.json>` | `GetModuleFileName` (Windows) / `argv[0]` → run sibling `leon-cook.exe recipe …` |

No Engine libraries. Exit codes normalized (`WEXITSTATUS` on Unix).

## Related docs

[SETUP.md](SETUP.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [NAMING.md](NAMING.md) · [LEVELS.md](LEVELS.md)
