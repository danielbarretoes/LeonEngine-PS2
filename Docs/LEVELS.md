# Levels (`.llev`), project packs and lightmaps

A level is a binary Leon Level file (`.llev`) loaded into a `ULevel` by the desktop runtime (`Engine` module). There is no JSON level format: `LoadLevelFile` rejects any path whose extension is not `.llev`. The PS2 runtime does not load levels yet; the ThirdPerson demo builds its level in code (`FThirdPersonLevel`).

Code: `Engine/Source/Runtime/Engine/Classes/Engine/Level.h`, `Engine/Source/Runtime/Engine/Public/Level/` (`LeonLevelFormat.h`, `LevelLoader.h`, `LevelCatalog.h`, `LevelDirector.h`, `LightmapIO.h`, `LevelAnimation.h`), `Engine/Source/Runtime/Engine/Public/GameHostSession.h`, `Engine/Source/Runtime/Engine/Public/Validation/ContentValidator.h`.
Also: [ASSET_FORMATS.md](ASSET_FORMATS.md) (`.lmat` / `.lmesh` referenced by actors) · [ARCHITECTURE.md](ARCHITECTURE.md) · [SETUP.md](SETUP.md).

## Types

| Type | Header | Role |
| --- | --- | --- |
| `ULevel` | `Classes/Engine/Level.h` | Live level: `UStaticMeshComponent`s, `FPlayerStart`, `FTriggerVolume`, `FPainCausingVolume`, `FAISpawnPoint`, `FDirectionalLight`, `FPointLight`, environment map, name, game mode |
| `FLevelDocument` | `Public/Level/LeonLevelFormat.h` | In-memory mirror of a `.llev`: plain data, no GPU resources (`FLevelActorRecord`, `FLevelLightRecord`, `FLevelCameraRecord`) |
| `FLevelAnimation` | `Public/Level/LevelAnimation.h` | Spin / bob / point-light orbit hooks produced by a load |
| `FLevelCatalog`, `FLevelEntry` | `Public/Level/LevelCatalog.h` | Finds `.llev` files in a directory or project pack |
| `FLevelDirector` | `Public/Level/LevelDirector.h` | Owns a catalog, loads levels into a `UGameEngine`, runs level animation, draws the level browser |
| `FGameHostSession` | `Public/GameHostSession.h` | Play session: resolves the pack, loads its start level, wires travel and game modes |
| `FValidationReport` | `Public/Validation/ContentValidator.h` | Errors / warnings from `ValidateLevelDocument` |

## Load pipeline

```text
LoadLevelFile(UGameEngine&, Path, FLevelAnimation*)
  ├─ extension must be .llev
  ├─ LoadLeonLevelFile            .llev bytes -> FLevelDocument (DeserializeLeonLevel)
  ├─ ValidateLevelDocument        any error rejects the load (report printed to stderr)
  └─ ApplyLevelDocument           builds a staging ULevel; commits only on full success
        ├─ environment, actors, lights
        ├─ camera (UGameEngine::GetCamera)
        └─ LoadLevelLightmaps     .lm -> UTexture2D per mesh
```

A failed load leaves the previous level and camera untouched. If any `StaticMesh` actor's mesh fails to load, the whole level is rejected (no partial loads). A level with no actors, no lights and no environment is rejected; blank or lights-only levels are valid.

Saving: `BuildLevelDocument(const ULevel&, const UCameraComponent&)` snapshots a live level, then `SaveLeonLevelFile` (atomic write) or `SerializeLeonLevel` (bytes). Only the automation tests use these today: "Editor-style level save load apply headless" (AIModule) round-trips `Engine/Content/LevelTemplates/Blank.llev`, and the level catalog tests write temporary `.llev` files.

### Asset paths inside a level

Material, mesh and environment paths are strings in the level's string table. `ResolveLevelAssetPath(LevelPath, Key)` resolves materials and meshes:

1. an absolute path that exists is used as is;
2. `<level folder>/../<Key>`, which is the pack's `Content/` for `Content/Levels/X.llev`;
3. legacy keys containing `Materials/` are retried from that folder;
4. otherwise `FPaths::ResolveAssetPath(Key)` (active project content, then `Engine/Content`).

The environment path goes straight to `FPaths::ResolveAssetPath`.

## File layout (version 2)

All values are little-endian. Strings live in one deduplicated table; fields reference them by index, and index `0` is always the empty string (also "unset"). Out-of-range indices read as the empty string.

Writers emit `version = 2` (`LeonLevelVersion`); readers accept 1 and 2 and parse both the same way. Magic `LeonLevelMagic` = `0x56454C4C` ("LLEV" on disk).

```text
u32 magic   = 'LLEV'
u32 version = 2            // reader also accepts 1
u32 flags   = 0            // reserved

string table:
  u32 count                                    // <= 65536
  count × (u32 byteLen + UTF-8 bytes, no terminator)   // <= 1 MiB each

meta:
  u32 nameIdx, gameModeIdx, environmentIdx
  f32 environmentExposure

camera (always present):
  u8  mode (0 = Orbit, 1 = FreeLook)
  u8  pad[3]
  f32 target[3], eye[3], distance, yaw, pitch

actors:
  u32 count                                    // <= 100000
  each:
    u8  class     (ELevelActorClass)
    u8  mobility  (0 Static, 1 Movable)
    u16 pad
    u32 flags     (see below)
    f32 position[3], rotationDegrees[3], scale[3]
    u32 tagIdx           if hasTag
    u32 materialIdx      if hasMaterial
    u32 meshIdx          if hasMesh
    u32 lightmapIdIdx    if hasLightmapId
    u32 lightmapPathIdx  if hasLightmapPath
    u32 lightmapResolution                       // always
    i32 sphereSegments, sphereRings              // class == Sphere
    f32 spinYaw                                  if hasSpinYaw
    f32 bobBaseY, bobAmplitude, bobSpeed         if hasBob
    f32 fitHeight                                if hasFitHeight
    i32 interactCost     if hasInteractCost
    f32 interactRadius   if hasInteractCost
    f32 damagePerSecond  if hasPainData
    f32 damageInterval   if hasPainData
    u32 payloadIdx       if hasPayload

lights:
  u32 count                                    // <= 16384
  each:
    u8  class (0 DirectionalLight, 1 PointLight)
    u8  pad[3]
    u32 flags (bit 0 castShadows, bit 1 hasOrbit)
    f32 position[3], rotationDegrees[3], lightColor[3]
    f32 intensity, range, sourceAngle
    f32 radius, height, heightAmp, speed         if hasOrbit
```

Actor `flags` bits (`LevelActorFlag*` constants): `0` collisionEnabled, `1` simulatePhysics, `2` enableGravity, `3` hidden, `4` hasBob, `5` hasSpinYaw, `6` hasFitHeight, `7` hasMaterial, `8` hasMesh, `9` hasLightmapId, `10` hasLightmapPath, `11` hasTag, `12` hasInteractCost, `13` hasPainData, `14` hasPayload, `15` consumeOnUse.

The writer always sets `hasInteractCost` for `TriggerVolume` and `hasPainData` for `PainCausingVolume`, and for any other actor whose values differ from the defaults (cost 0, radius 2; 12 damage per second, 0.35 s interval).

### Actor classes (`ELevelActorClass`)

| Value | Class | Applied as |
| --- | --- | --- |
| 0 | `PlayerStart` | `FPlayerStart` (spawn transform) |
| 1 | `Cube` | Procedural `UStaticMeshComponent` (`FBasicShape`) |
| 2 | `Sphere` | Procedural, uses `sphereSegments` / `sphereRings` |
| 3 | `Plane` | Procedural |
| 4 | `BlockingVolume` | Procedural cube whose materials never cast shadows |
| 5 | `StaticMesh` | `FResourceCache::LoadStaticMesh` on the resolved `.lmesh` path |
| 6 | `TriggerVolume` | `FTriggerVolume` (interact radius / cost, pack-defined `payload`, `consumeOnUse`) |
| 7 | `PainCausingVolume` | `FPainCausingVolume` (damage per second / interval) |
| 8 | `AISpawnPoint` | `FAISpawnPoint` (transform + tag) |

Only `StaticMesh` carries a mesh path. `PlayerStart`, `AISpawnPoint`, `TriggerVolume` and `PainCausingVolume` are plain data (no drawable mesh). Mesh actors take their material from `materialPath` (`.lmat`); without one, a mesh with no materials of its own gets the default material. `collisionEnabled` is forced on when `simulatePhysics` is set. `fitHeight` scales the mesh to that height and grounds it (`ApplyFitHeight`). The `TriggerVolume` payload string is interpreted by the game mode.

Unknown actor or light classes fail the read.

### Lights

`DirectionalLight` and `PointLight` records become `FDirectionalLight` / `FPointLight`. If a level has no directional light, one default light is added. Counts are clamped to `MaxDirectionalLights` (2) and `MaxPointLights` (4) from `Level/Light.h`; orbits of dropped point lights are dropped too.

### Camera

Always stored. The orbit fields (`target`, `distance`, `yaw`, `pitch`) are the baseline; `eye` is stored for both modes so FreeLook restores exactly.

### Level animation

`ApplyLevelDocument` fills an `FLevelAnimation` with spins (`spinYaw` degrees per second), bobs (`bobBaseY`, `bobAmplitude`, `bobSpeed`) and point-light orbits. `FLevelDirector::Update` applies them every frame.

## Validation

`ValidateLevelDocument(const FLevelDocument&, SourcePath)` (`ContentValidator`) runs after decoding. Magic, version and class values are already enforced by the reader.

- **Errors:** `StaticMesh` without a mesh path; a mesh path on any other class; sphere with fewer than 3 segments or 2 rings; `TriggerVolume` with a non-positive interact radius; `PainCausingVolume` with negative damage or a non-positive interval; `fitHeight` set but not positive; a material path that does not resolve; negative light intensity; point light with a non-positive range; negative environment exposure. Issues found by `ValidateMaterialFile` in a referenced `.lmat` are merged into the report.
- **Warnings:** mesh file not found, lightmap file not found, HDR environment not found.

An empty actor list is valid.

## Level catalog and director

`FLevelCatalog` lists `.llev` files, sorted by path:

| Method | Scans |
| --- | --- |
| `Scan(Directory)` | `Directory/*.llev` (flat) |
| `ScanPack(PackDirectory)` | `FPaths::ProjectContentDir(Pack)/Levels/*.llev` |
| `ScanProjectPacks(ProjectsRoot)` | Every pack folder under the root (skips `_host` and folders starting with `.`) |

Each `FLevelEntry` has `Name` (the level document's name, else the file stem), `Path`, `Pack` (pack folder name) and `GameMode` (the level's game mode override). `FindIndexByLevelKey` matches the name, the file stem or the full path, case-insensitively; `FindIndexByGameModeOrPack` matches the game mode or pack name.

`FLevelDirector` wraps a catalog:

- `ScanPackAndLoad(Engine, PackDirectory, PreferredLevelKey)` loads the preferred level if it is in the catalog, otherwise the first entry; `ScanAndLoad(Engine, ProjectsDirectory)` scans every pack and loads the first level.
- `LoadIndex`, `LoadByKey` (travel by level key), `Next`, `Previous`. The current index only changes after a successful load.
- `DrawUi` / `HandleUiInput` draw a bottom-right `< name (i/n) >` browser; `[` / `]`, digit keys `1`–`9` and clicks on the arrows switch levels. `SetBrowserVisible(false)` hides and disables it.

## Project packs

A runtime project pack is a folder with a `leon.game.json` marker and its content under `Content/`:

```text
Projects/<Name>/
├── leon.game.json            { "defaultLevel": "Levels/Main.llev" }
└── Content/
    ├── Levels/
    │   ├── Main.llev
    │   └── Lightmaps/LM_<id>.lm      (lightmap paths are relative to the .llev folder)
    ├── Materials/M_*.lmat
    ├── Textures/T_*.png
    └── Meshes/*.lmesh
```

Only `Content/Levels/*.llev` and the `leon.game.json` location are fixed; other folders are whatever the levels and materials reference (content-relative keys such as `Materials/M_Floor.lmat`). `FPaths::ProjectContentDir` still accepts an older layout with `<pack>/Levels` and no `Content/Levels`, in which case the pack root is the content root.

`FGameHostSession::Start(Engine, PackName, RegisterModes, PreferredLevelKey, PackRootOverride)`:

1. initializes `FWorldRuntime`;
2. resolves the pack with `FProjectDescriptor::Resolve(PackName)` (`FPaths::ResolveAssetPath("Projects/<Name>")` + `leon.game.json`), or reads `leon.game.json` from `PackRootOverride`;
3. calls `FPaths::SetActiveContentRoot(PackRoot)` so asset lookups prefer the pack's `Content/`;
4. loads `PreferredLevelKey`, else the `defaultLevel` stem (`FProjectDescriptor::DefaultLevelKey`), else the first catalog entry (`FWorldRuntime::LoadPack` → `FLevelDirector::ScanPackAndLoad`);
5. binds `UGameInstance` level travel to `FLevelDirector::LoadByKey`, registers game modes on the `FGameplayRouter` (`ADefaultGameMode` by default) and ticks once.

The Win64 `LeonGame` target runs a pack with `Engine\Binaries\Win64\LeonGame.exe --pack <Name>`. The repository does not ship a runtime pack at the moment, and `Game/ThirdPerson` is a build project (`.lproj`), not a pack.

## Level templates

`Engine/Content/LevelTemplates/` holds seed levels:

| File | Contents |
| --- | --- |
| `Blank.llev` | Version 1, name `Untitled`, game mode `Default`, no actors, one directional light |
| `Starter.llev` | Version 1, name `Starter`, game mode `Default`: a `Plane` with `materials/M_WorldGrid.lmat`, a `PlayerStart`, one directional light, environment `hdr/autumn_field_puresky_1k.hdr` |

`Starter.llev` still uses pre-rename lowercase paths; the HDR in `Engine/Content` is now `Hdr/AutumnFieldPuresky1k.hdr`, so loading it reports a missing-HDR warning.

## Lightmaps

Per-mesh fields in the level: `lightmapId` (stable id, survives actor reorder; `EnsureLightmapId` generates one), `lightmapPath` (relative path to the `.lm` file, conventionally `Lightmaps/LM_<id>.lm`) and `lightmapResolution`.

### Load

`LoadLevelLightmaps(ULevel&, LevelPath)` (`LightmapIO`) runs at the end of `ApplyLevelDocument`. For each `UStaticMeshComponent` with a `lightmapPath`, `ResolveLightmapAbsolutePath` looks next to the `.llev` file (a legacy lowercase `lightmaps/` prefix is retried as `Lightmaps/`); without a level path it falls back to `FPaths::ResolveAssetPath`. `LoadLightmapFile` reads the file into a `UTexture2D`.

### Format `.lm`

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LM01` |
| 4–7 | Width (`u32`, host byte order) |
| 8–11 | Height (`u32`) |
| 12… | `width * height * 4` bytes RGBA8 |

Invalid magic, a zero dimension or a dimension above 4096 rejects the file.

### Bake

There is no lightmap baker in the repository: it lived in the Editor, which was removed. `.lm` files and the lightmap fields are still loaded and preserved on save.
