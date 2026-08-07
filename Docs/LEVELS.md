# Leon Level format (`.llev`), camera, and lightmaps

Binary map format loaded by `LoadLevelFile` (Engine) and written by the Editor (`LevelSaver`). There is **no JSON level format** — `LoadLevelFile` rejects any file whose extension is not `.llev`.

Related code: `Engine/include/leon/level/LeonLevelFormat.h`, `Level.h`, `LevelLoader.h`, `LightmapIO.h`; Editor bake: `<leon/editor/LightmapBaker.h>`.  
Also: [SETUP.md](SETUP.md) (builds) · [ASSET_FORMATS.md](ASSET_FORMATS.md) (`.lmat` / `.lmesh` paths in actors) · [ARCHITECTURE.md](ARCHITECTURE.md).

## Pipeline

`.llev` bytes → `LoadLeonLevelFile` → `LevelDocument` (plain data) → `ValidateLevelDocument` → `ApplyLevelDocument` (staging `Level`, committed only on full success) → `LoadLevelLightmaps`.

The Editor uses the same document for undo/redo: `SerializeLevelSnapshot` / `LoadLevelSnapshot` keep `.llev` bytes in memory.

## File layout (version 2)

All values are **little-endian**. Strings live in one deduplicated table; fields reference them by index, and index `0` is always the empty string (also used for "unset").

**Compatibility:** writers emit `version = 2`. Readers accept **v1 and v2**. v1 files have no volume-class actors and no volume flags.

```text
u32 magic   = 'LLEV'
u32 version = 2            // reader also accepts 1
u32 flags   = 0            // reserved

string table:
  u32 count
  count × (u32 byteLen + UTF-8 bytes, no null terminator)

meta:
  u32 nameIdx, gameModeIdx, environmentIdx
  f32 environmentExposure

camera (always present):
  u8  mode (0 = Orbit, 1 = FreeLook)
  u8  pad[3]
  f32 target[3], eye[3], distance, yaw, pitch

actors:
  u32 count
  each:
    u8  class     (see ELevelActorClass below)
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
    i32 interactCost     if hasInteractCost      // TriggerVolume
    f32 interactRadius   if hasInteractCost
    f32 damagePerSecond  if hasPainData          // PainCausingVolume
    f32 damageInterval   if hasPainData
    u32 payloadIdx       if hasPayload           // pack-defined string

lights:
  u32 count
  each:
    u8  class (0 DirectionalLight, 1 PointLight)
    u8  pad[3]
    u32 flags (bit0 castShadows, bit1 hasOrbit)
    f32 position[3], rotationDegrees[3], lightColor[3]
    f32 intensity, range, sourceAngle
    f32 radius, height, heightAmp, speed         if hasOrbit
```

Actor `flags` bits: `0` collisionEnabled, `1` simulatePhysics, `2` enableGravity, `3` hidden, `4` hasBob, `5` hasSpinYaw, `6` hasFitHeight, `7` hasMaterial, `8` hasMesh, `9` hasLightmapId, `10` hasLightmapPath, `11` hasTag, `12` hasInteractCost, `13` hasPainData, `14` hasPayload, `15` consumeOnUse.

### Actors (`ELevelActorClass`)

| Value | Class | Runtime |
| --- | --- | --- |
| 0 | `PlayerStart` | Spawn transform (no mesh) |
| 1–3 | `Cube` / `Sphere` / `Plane` | Procedural mesh |
| 4 | `BlockingVolume` | Hidden collision cube |
| 5 | `StaticMesh` | Imported mesh path |
| 6 | `TriggerVolume` | POD interact (Use / buys); Unreal `ATriggerVolume` lite |
| 7 | `PainCausingVolume` | POD damage AABB; Unreal `APainCausingVolume` lite |
| 8 | `AISpawnPoint` | POD AI spawn transform |

`StaticMesh` is the only class that carries a mesh path; basic shapes are procedural. `PlayerStart` / `AISpawnPoint` / `TriggerVolume` / `PainCausingVolume` are level PODs (no drawable mesh). Materials are `.lmat` paths — inline material fields are not part of the level format.

**TriggerVolume payload** is pack-defined (e.g. Zombies: `Door`, `WallBuy:M14`, `Ammo`, `Perk:Jugg`, `PaP`). GameMode applies purchase / open logic on Use.

| Field | Role |
| --- | --- |
| `lightmapId` | Bake writes `Lightmaps/LM_<id>.lm`. Survives actor reorder. |
| `lightmapPath` | Relative path to the `.lm` file (set on Build Lights). |
| `fitHeight` | Optional: scale the mesh to this height and ground-align it. |

`BlockingVolume` defaults to collision on, hidden, and never casts shadows. `collisionEnabled` is forced on when `simulatePhysics` is set.

### Lights

If a level has no directional light, the loader adds a default one. Counts are clamped to the renderer limits (`kMaxDirectionalLights`, `kMaxPointLights`); orbits pointing at truncated lights are dropped.

## Camera

Always stored. Orbit fields (`target`, `distance`, `yaw`, `pitch`) are the baseline; `eye` is written for both modes so FreeLook restores exactly.

## Validation

`ValidateLevelDocument(const LevelDocument&, sourcePath)` runs after decoding. Magic, version and class enums are already enforced by the reader, so validation covers referenced assets and value ranges:

- error: `StaticMesh` without a mesh path, mesh path on a basic shape, missing `.lmat`, degenerate sphere tessellation, negative intensity / non-positive point range
- warning: missing mesh file, missing HDR environment

An empty actor list is valid (blank / lights-only levels).

## Lightmaps

### Bake (Editor Build Lights)

`leon::editor::BakeLevelLightmaps(level, message, levelPath)` (`Editor/SceneEditing/LightmapBaker.cpp`):

1. Bakes Static-mobility meshes that have CPU mesh data (UV0 atlas).
2. Ensures each mesh has a `lightmapId` (`EnsureLightmapId` in Engine `LightmapIO`).
3. Writes `Lightmaps/LM_<lightmapId>.lm` next to the `.llev` file (`LM01` + RGBA8).
4. Sets `lightmapPath` to `Lightmaps/LM_<id>.lm`.

Movable / hidden meshes clear their runtime lightmap texture (path cleared when mobility is not Static).

### Load (Runtime + Editor)

`LoadLevelLightmaps` lives in **Engine** (`LightmapIO`). `LoadLevelFile` calls it after a successful commit using the `.llev` path, so shipping packs and Editor open both hydrate GPU textures from `.lm` files.

### Format `.lm`

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LM01` |
| 4–7 | Width (`uint32`, host endian) |
| 8–11 | Height (`uint32`) |
| 12… | `width * height * 4` RGBA8 |

Invalid magic, zero size, or dimensions &gt; 4096 reject the load.

### Notes / limits

- Bake samples **UV0** (`texCoord`). Overlapping albedo UVs produce poor lightmaps; a dedicated lightmap UV channel is future work.
- Directional shadow in bake is cheap AABB ray occlusion (self AABB skipped).
- Point lights contribute diffuse attenuation only (no bake shadows).
- Ambient in bake is a flat constant (~0.12); no IBL.

## Pipeline diagram

```text
.llev ──LevelDocument──ContentValidator──► ApplyLevelDocument (staging)
                                      │
                                      ├─ actors / lights / env / camera
                                      └─ LoadLevelLightmaps(.lm → Texture)
                                              │
                                              ▼
                                         Engine::Level
                                              │
                         Editor Bake ◄────────┴──► Renderer (uLightmap)
```
