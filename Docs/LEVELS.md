# Levels (`.llev`)

A level is a binary Leon Level file (`.llev`) loaded into a `ULevel` by the desktop runtime (`Engine` module). Since P13 its content is actors, as in UE: the reader spawns them into the game world and the saver writes them back. There is no JSON level format: `LoadLevelFile` rejects any path whose extension is not `.llev`. The PS2 runtime does not load levels yet; the ThirdPerson demo builds its level in code (`FThirdPersonLevel`).

Code: `Engine/Source/Runtime/Engine/Classes/Engine/Level.h`, `Engine/Source/Runtime/Engine/Public/Level/` (`LeonLevelFormat.h`, `LevelLoader.h`, `LegacyLevelDataComponent.h`), the actor classes in `Engine/Source/Runtime/Engine/Classes/{Engine,GameFramework}/`, `Engine/Source/Runtime/Launch/Private/Desktop/GameApplication.cpp` (startup level).
Also: [ASSET_FORMATS.md](ASSET_FORMATS.md) (`.lmat` / `.lmesh` referenced by actors) · [ARCHITECTURE.md](ARCHITECTURE.md) · [SETUP.md](SETUP.md).

## Types

| Type | Header | Role |
| --- | --- | --- |
| `ULevel` | `Classes/Engine/Level.h` | Live level, a UObject: the game world's persistent level (`UGameEngine::GetLevel`). It holds the world's actors (`Actors`: the level content and the gameplay actors) and its `AWorldSettings` (`GetWorldSettings`) |
| `AWorldSettings` | `Classes/GameFramework/WorldSettings.h` | The level's settings actor, spawned first (`DefaultGameMode`, `KillZ`); its legacy data component keeps the level name, the game mode string, the environment fields and the camera framing |
| `ULegacyLevelDataComponent` | `Public/Level/LegacyLevelDataComponent.h` | The record fields with no UE counterpart yet, on the actor spawned from the record (class, mesh and material keys, sphere tessellation, spin, bob, trigger data, light orbit), so the saver can write them back; it goes away with the format (P15) |
| `FLevelDocument` | `Public/Level/LeonLevelFormat.h` | In-memory mirror of a `.llev`: plain data, no GPU resources (`FLevelActorRecord`, `FLevelLightRecord`, `FLevelCameraRecord`) |

## Load pipeline

```text
LoadLevelFile(UGameEngine&, Path)
  ├─ extension must be .llev
  ├─ LoadLeonLevelFile            .llev bytes -> FLevelDocument (DeserializeLeonLevel)
  └─ ApplyLevelDocument
        ├─ resolve every record first: transform, mesh (basic shape or .lmesh), material, fit height
        │     (a failure here leaves the current level untouched)
        ├─ destroy the previous level content actors (the gameplay actors stay)
        ├─ spawn AWorldSettings (ULevel::WorldSettings)
        ├─ spawn one actor per record, in file order (see Actor classes)
        ├─ spawn the lights (see Lights)
        ├─ camera: kept on the world settings and applied to UGameEngine::GetCamera
        └─ CollectGarbage               a level load is a safe point (D11): the replaced actors go
```

Spawning an actor registers its components: a primitive with collision adds its body to the world's physics scene (`CreatePhysicsState`) and, when the world renders, its scene proxy to the world's scene (`CreateRenderState_Concurrent`), so there is no separate physics or render step after the load.

There is no separate validation pass: magic, version, class values and limits are enforced by the reader, and resource failures by `ApplyLevelDocument`. A failed load leaves the previous level and camera untouched. If any `StaticMesh` actor's mesh fails to load, the whole level is rejected (no partial loads). Blank or lights-only levels are valid (a level always gets at least the default sun).

Saving: `BuildLevelDocument(const ULevel&, const UCameraComponent&)` builds a document from the level's actors, then `SaveLeonLevelFile` (atomic write) or `SerializeLeonLevel` (bytes). The records go out grouped by class, as the format always wrote them (player starts, AI spawn points, trigger volumes, pain-causing volumes, then the meshes and blocking volumes; directional, then point lights), each group in spawn order, so loading a file and saving it gives the same bytes (`System.Engine.LevelFormat.SaveWritesTheSameBytes` checks the templates and a document with every record class against the hashes the pre-P13 saver gave). Only the automation tests save levels today: "Editor-style level save load apply headless" (AIModule) round-trips `Engine/Content/LevelTemplates/Blank.llev`, and the `.llev` format tests (`Engine/Private/Tests/LevelFormatTests.cpp`) round-trip a document through bytes.

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

### Coordinates in the file

Every value in a `.llev` is in the **legacy space**: Y up, right-handed, metres, rotations as XYZ Euler degrees
(applied Z, then Y, then X). `FLevelDocument` keeps them as stored. `ApplyLevelDocument` converts them to the engine
world (UE: X forward, Y right, Z up, left-handed, centimetres) with `FLegacyCoordinateConversion`
(`RenderCore/Public/LegacyCoordinateConversion.h`), and `BuildLevelDocument` converts back, so saved files stay in the
legacy space:

| Field | Conversion |
| --- | --- |
| `position`, camera `target` / `eye` | (X, Z, Y) × 100 |
| `rotationDegrees` of meshes and volumes | `ConvertEulerXYZ`: the legacy rotation with the axes swapped, as an `FQuat` |
| `rotationDegrees` of `PlayerStart` / `AISpawnPoint` | `ConvertActorEulerXYZ`: a pure legacy yaw ψ becomes the world yaw 90 − ψ |
| `scale` | (X, Z, Y); a component closer to zero than 1e-4 becomes ±1e-4 |
| lengths: camera `distance`, `fitHeight`, `bobBaseY`, `bobAmplitude`, `interactRadius`, light `range`, orbit `radius` / `height` / `heightAmp` | × 100 (`bobBaseY` becomes the live `BobBaseZ`) |
| `spinYaw` (degrees / s) | sign flipped |
| camera `yaw` / `pitch` | orbit: `FRotator(−pitch, yaw + 180, 0)`; free look: `FRotator(pitch, yaw, 0)` |
| light `rotationDegrees` (x = pitch, y = yaw) | `FRotator(−pitch, 90 − yaw, 0)`; the light shines along its forward axis |

Only the level reader, the saver and tests may use `FLegacyCoordinateConversion` (`CheckBannedApis.ps1`, gate G4).

Actor `flags` bits (`LevelActorFlag*` constants): `0` collisionEnabled, `1` simulatePhysics, `2` enableGravity, `3` hidden, `4` hasBob, `5` hasSpinYaw, `6` hasFitHeight, `7` hasMaterial, `8` hasMesh, `9` hasLightmapId, `10` hasLightmapPath, `11` hasTag, `12` hasInteractCost, `13` hasPainData, `14` hasPayload, `15` consumeOnUse.

The writer always sets `hasInteractCost` for `TriggerVolume` and `hasPainData` for `PainCausingVolume`, and for any other actor whose values differ from the defaults (cost 0, radius 2 m in the file, 200 cm in the world; 12 damage per second, 0.35 s interval).

### Actor classes (`ELevelActorClass`)

| Value | Class | Spawned as |
| --- | --- | --- |
| 0 | `PlayerStart` | `APlayerStart` (the spawn transform; `AGameModeBase::FindPlayerStart`) |
| 1 | `Cube` | `AStaticMeshActor` with a procedural mesh (`FBasicShape`) |
| 2 | `Sphere` | `AStaticMeshActor`, procedural, uses `sphereSegments` / `sphereRings` |
| 3 | `Plane` | `AStaticMeshActor`, procedural |
| 4 | `BlockingVolume` | `ABlockingVolume`: a 100 cm brush box (plan decision D16) sized by the scale, never drawn; its collision flags come from the record |
| 5 | `StaticMesh` | `AStaticMeshActor` with `FResourceCache::LoadStaticMesh` on the resolved `.lmesh` path |
| 6 | `TriggerVolume` | `ATriggerVolume` (interact radius / cost, game-defined `payload`, `consumeOnUse` on its legacy data component) |
| 7 | `PainCausingVolume` | `APainCausingVolume` (`DamagePerSec`, `PainInterval`) |
| 8 | `AISpawnPoint` | `ATargetPoint` (transform + tag) |

Only `StaticMesh` carries a mesh path. The record `tag` becomes the actor's first `Tags` entry (`UGameplayStatics::GetAllActorsWithTag`) and `hidden` its `bHidden`. Mesh actors take their material from `materialPath` (`.lmat`); without one, a mesh with no materials of its own gets the default material. `mobility`, `collisionEnabled`, `simulatePhysics` and `enableGravity` go to the mesh component or the volume's brush (`SetMobility`, `SetCollisionEnabled`, `SetSimulatePhysics`, `SetEnableGravity`); `collisionEnabled` is forced on when `simulatePhysics` is set. `fitHeight` scales the mesh (or the blocking volume's 100 cm cube) to that height and grounds it (`ApplyFitHeight`). The `TriggerVolume` payload string is interpreted by the game mode. Volumes test containment with the axis-aligned box around the actor (`AVolume::EncompassesPoint`), as before.

Unknown actor or light classes fail the read.

### Lights

`DirectionalLight` and `PointLight` records become `ADirectionalLight` / `APointLight` actors (`UDirectionalLightComponent`: `LightColor`, `Intensity`, `CastShadows`, `LightSourceAngle`; `UPointLightComponent`: `AttenuationRadius` from `range`). If a level has no directional light, the default sun is spawned. Only the first `MaxDirectionalLights` (2) and `MaxPointLights` (4) of each kind are spawned (`Level/Light.h`), with a warning for the rest.

### Camera

Always stored. The orbit fields (`target`, `distance`, `yaw`, `pitch`) are the baseline; `eye` is stored for both modes so FreeLook restores exactly.

### Level animation

Spins (`spinYaw` degrees per second), bobs (`bobBaseY`, `bobAmplitude`, `bobSpeed`) and point-light orbits are kept on the actors' legacy data components (`SpinYaw`, `bHasBob`, `bHasOrbit`) and preserved on save, but nothing animates them at runtime: the level animation player was removed in 0.12.0.

## Running a level

The Win64 `LeonGame` target loads one level and runs `ADefaultGameMode` on it:

```text
Engine\Binaries\Win64\LeonGame.exe [-map=<.llev>] [-nullrhi] [-tick=<Hz>] [-showstats] [-AxesGizmo]
                                   [-Screenshot=<file.bmp> [-ExitAfterFrames=N]]
```

`-map=` takes a path relative to the working directory (or absolute), else relative to the content folders; without it the startup level is `GameDefaultMap` from `[/Script/EngineSettings.GameMapsSettings]` in the engine config (`BaseEngine.ini`: `LevelTemplates/Starter.llev`). `-nullrhi` runs headless at `-tick=` Hz (default 60). `-AxesGizmo` starts with the axes gizmo on (F6 toggles it); `-Screenshot=` saves frame `-ExitAfterFrames=` (default 60) as a BMP and exits ([TESTING.md](TESTING.md)). The level's game mode string is stored (on the world settings) but not used to pick a game mode. There is no level catalog, level browser or project pack (all removed in 0.12.0), and `Game/ThirdPerson` is a build project (`.lproj`), not a runtime pack.

## Level templates

`Engine/Content/LevelTemplates/` holds seed levels:

| File | Contents |
| --- | --- |
| `Blank.llev` | Version 1, name `Untitled`, game mode `Default`, no actors, one directional light |
| `Starter.llev` | Version 1, name `Starter`, game mode `Default`: a `Plane` with `materials/M_WorldGrid.lmat`, a `PlayerStart`, one directional light, environment `hdr/autumn_field_puresky_1k.hdr` (ignored) |

`Starter.llev` still uses pre-rename lowercase paths.

## Lightmaps

Lightmaps (`LightmapIO`, `.lm` files) were removed in 0.12.0. The per-actor `lightmapId`, `lightmapPath` and `lightmapResolution` fields are still read and written for binary compatibility but ignored. Static lighting returns as `<Map>_BuiltData.lasset`.
