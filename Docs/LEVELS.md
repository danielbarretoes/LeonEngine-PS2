# Levels (`.llev`)

A level is a binary Leon Level file (`.llev`) loaded into a `ULevel` by the desktop runtime (`Engine` module). There is no JSON level format: `LoadLevelFile` rejects any path whose extension is not `.llev`. The PS2 runtime does not load levels yet; the ThirdPerson demo builds its level in code (`FThirdPersonLevel`).

Code: `Engine/Source/Runtime/Engine/Classes/Engine/Level.h`, `Engine/Source/Runtime/Engine/Public/Level/` (`LeonLevelFormat.h`, `LevelLoader.h`), `Engine/Source/Runtime/Launch/Private/Desktop/GameApplication.cpp` (startup level).
Also: [ASSET_FORMATS.md](ASSET_FORMATS.md) (`.lmat` / `.lmesh` referenced by actors) · [ARCHITECTURE.md](ARCHITECTURE.md) · [SETUP.md](SETUP.md).

## Types

| Type | Header | Role |
| --- | --- | --- |
| `ULevel` | `Classes/Engine/Level.h` | Live level: `UStaticMeshComponent`s, `FPlayerStart`, `FTriggerVolume`, `FPainCausingVolume`, `FAISpawnPoint`, `FDirectionalLight`, `FPointLight`, name, game mode |
| `FLevelDocument` | `Public/Level/LeonLevelFormat.h` | In-memory mirror of a `.llev`: plain data, no GPU resources (`FLevelActorRecord`, `FLevelLightRecord`, `FLevelCameraRecord`) |

## Load pipeline

```text
LoadLevelFile(UGameEngine&, Path)
  ├─ extension must be .llev
  ├─ LoadLeonLevelFile            .llev bytes -> FLevelDocument (DeserializeLeonLevel)
  └─ ApplyLevelDocument           builds a staging ULevel; commits only on full success
        ├─ actors, lights
        └─ camera (UGameEngine::GetCamera)
```

There is no separate validation pass: magic, version, class values and limits are enforced by the reader, and resource failures by `ApplyLevelDocument`. A failed load leaves the previous level and camera untouched. If any `StaticMesh` actor's mesh fails to load, the whole level is rejected (no partial loads). A level with no actors and no lights is rejected; blank or lights-only levels are valid.

Saving: `BuildLevelDocument(const ULevel&, const UCameraComponent&)` snapshots a live level, then `SaveLeonLevelFile` (atomic write) or `SerializeLeonLevel` (bytes). Only the automation tests use these today: "Editor-style level save load apply headless" (AIModule) round-trips `Engine/Content/LevelTemplates/Blank.llev`, and the `.llev` format tests (`Engine/Private/Tests/LevelFormatTests.cpp`) round-trip a document through bytes.

### Asset paths inside a level

Material, mesh and environment paths are strings in the level's string table. `ResolveLevelAssetPath(LevelPath, Key)` resolves materials and meshes:

1. an absolute path that exists is used as is;
2. `<level folder>/../<Key>`, which is the `Content/` folder for `Content/Levels/X.llev`;
3. legacy keys containing `Materials/` are retried from that folder;
4. otherwise `FPaths::ResolveLegacyContentPath(Key)` (the path as given, then the project content, then `Engine/Content`).

The environment path is read and written but ignored (HDR environment maps were removed in 0.12.0).

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
  f32 environmentExposure                      // environment fields: kept for compatibility, ignored

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
    u32 lightmapIdIdx    if hasLightmapId        // lightmap fields: kept for compatibility, ignored
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
| 6 | `TriggerVolume` | `FTriggerVolume` (interact radius / cost, game-defined `payload`, `consumeOnUse`) |
| 7 | `PainCausingVolume` | `FPainCausingVolume` (damage per second / interval) |
| 8 | `AISpawnPoint` | `FAISpawnPoint` (transform + tag) |

Only `StaticMesh` carries a mesh path. `PlayerStart`, `AISpawnPoint`, `TriggerVolume` and `PainCausingVolume` are plain data (no drawable mesh). Mesh actors take their material from `materialPath` (`.lmat`); without one, a mesh with no materials of its own gets the default material. `collisionEnabled` is forced on when `simulatePhysics` is set. `fitHeight` scales the mesh to that height and grounds it (`ApplyFitHeight`). The `TriggerVolume` payload string is interpreted by the game mode.

Unknown actor or light classes fail the read.

### Lights

`DirectionalLight` and `PointLight` records become `FDirectionalLight` / `FPointLight`. If a level has no directional light, one default light is added. Counts are clamped to `MaxDirectionalLights` (2) and `MaxPointLights` (4) from `Level/Light.h`.

### Camera

Always stored. The orbit fields (`target`, `distance`, `yaw`, `pitch`) are the baseline; `eye` is stored for both modes so FreeLook restores exactly.

### Level animation

Spins (`spinYaw` degrees per second), bobs (`bobBaseY`, `bobAmplitude`, `bobSpeed`) and point-light orbits are copied into the live level (`UStaticMeshComponent::SpinYaw` / `bHasBob`, `FPointLight::bHasOrbit`) and preserved on save, but nothing animates them at runtime: the level animation player was removed in 0.12.0.

## Running a level

The Win64 `LeonGame` target loads one level and runs `ADefaultGameMode` on it:

```text
Engine\Binaries\Win64\LeonGame.exe [-map=<.llev>] [-nullrhi] [-tick=<Hz>] [-showstats]
```

`-map=` takes a path relative to the working directory (or absolute), else relative to the content folders; without it the startup level is `GameDefaultMap` from `[/Script/EngineSettings.GameMapsSettings]` in the engine config (`BaseEngine.ini`: `LevelTemplates/Starter.llev`). `-nullrhi` runs headless at `-tick=` Hz (default 60). The level's game mode string is stored but not used to pick a game mode. There is no level catalog, level browser or project pack (all removed in 0.12.0), and `Game/ThirdPerson` is a build project (`.lproj`), not a runtime pack.

## Level templates

`Engine/Content/LevelTemplates/` holds seed levels:

| File | Contents |
| --- | --- |
| `Blank.llev` | Version 1, name `Untitled`, game mode `Default`, no actors, one directional light |
| `Starter.llev` | Version 1, name `Starter`, game mode `Default`: a `Plane` with `materials/M_WorldGrid.lmat`, a `PlayerStart`, one directional light, environment `hdr/autumn_field_puresky_1k.hdr` (ignored) |

`Starter.llev` still uses pre-rename lowercase paths.

## Lightmaps

Lightmaps (`LightmapIO`, `.lm` files) were removed in 0.12.0. The per-actor `lightmapId`, `lightmapPath` and `lightmapResolution` fields are still read and written for binary compatibility but ignored. Static lighting returns as `<Map>_BuiltData.lasset`.
