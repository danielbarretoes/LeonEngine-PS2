# Leon asset formats

**Audience:** content authors, Editor, Tools  
**Also:** [EDITOR.md](EDITOR.md) (UI) · [LEVELS.md](LEVELS.md) (`.llev` levels) · [SETUP.md](SETUP.md) (cook CLI)

Runtime shipping formats are **Leon binaries / `.lmat` text**. Sources (OBJ, FBX, glTF) are imported and cooked — they are not the play-time format. Optional `.obj` is an **import source only** (Editor / cook), not a runtime mesh path.

> Unreal `.uasset` / `.umap` are proprietary. Leon does **not** read or write them.  
> Interchange with Blender / Unreal: **FBX** or **glTF** → cook to Leon formats.

---

## Extension cheat sheet

| Ext | Kind | Role |
| --- | --- | --- |
| `.lmat` | Text | Material (canonical) |
| `.llev` | Binary `LLEV` | Level (canonical map format) |
| `.lmesh` | Binary `LMSH` | Cooked static mesh |
| `.lskel` | Binary `LSK1` | Skeleton |
| `.lskm` | Binary `LKM1` | Skeletal mesh |
| `.lanim` | Binary `LAN1` | Animation clip |
| `.lm` | Binary `LM01` | Baked lightmap |
| `.lchar` | Text | Character package (mesh + anim refs) |
| `*_Locomotion.blendspace1d.json` | JSON | BlendSpace1D descriptor |
| `.obj` / `.fbx` / `.gltf` / `.glb` | Source | Import → cook |

**Removed (do not use):** material `.json` assets; level `.json` files (use `.llev`); skeletal sidecars `*.skeleton.json`, `*.skelmesh.json`+`.bin`, `*.anim.json`+`.bin`.

---

## Material — `.lmat`

INI-style text (Unreal **Material Instance**–like parameters).

```ini
# Leon Material (.lmat) — version 1

[Info]
Name=M_Wood
ShadingModel=DefaultLit   ; DefaultLit | Unlit

[Parameters]
BaseColor=0.55,0.35,0.2
Metallic=0
Roughness=0.75
Specular=0.04,0.04,0.04
Opacity=1
UVScale=2,2
CastsShadows=true
PlanarMirror=false

[Textures]
BaseColorMap=Textures/T_Wood_D.png   ; empty = solid BaseColor
NormalMap=Textures/T_Wood_N.png      ; optional; "bump" = procedural
```

| Field | Role |
| --- | --- |
| `BaseColor` | Solid color when no map |
| `Metallic` / `Roughness` | PBR scalars |
| `BaseColorMap` / `NormalMap` | Texture paths, or `checker` / `bump` |

**Authoring**

- Content Browser → **New Material…**
- Double-click `.lmat` → **Material Editor** (dockable tab)
- Details → **Edit Material…** / **Pick Material…**
- Save writes `.lmat`; `ResourceCache::InvalidateMaterial` on save

**API:** `LoadLeonMaterialDocument` / `LoadLeonMaterialFile` / `SaveLeonMaterialFile` / `ResourceCache::loadMaterial`  
**Engine defaults:** `Engine/Assets/Materials/M_Default.lmat`, `M_WorldGrid.lmat`, `M_SolidMetal.lmat`

Level actors reference materials by `.lmat` path only — the `.llev` format has no inline material fields.

---

## Level — `.llev`

Binary map container (`LLEV`, little-endian, **version 2** writers; readers accept **v1 and v2**): string table, meta, camera, actors, lights. This is the only level format the engine reads. Full layout and compatibility notes in [LEVELS.md](LEVELS.md).

**API:** `LoadLeonLevelFile` / `SaveLeonLevelFile` / `SerializeLeonLevel` / `DeserializeLeonLevel` / `BuildLevelDocument` / `ApplyLevelDocument` (`leon/level/LeonLevelFormat.h`), plus `LoadLevelFile` for the full load path.

---

## Static mesh — `.lmesh`

Binary cooked mesh (`LMSH`, little-endian): AABB, `Vertex[]`, indices, submeshes, material-slot strings.

| Source | Cook |
| --- | --- |
| `.obj` (+ `.mtl`) | `CookStaticMeshFromObj` / Editor Import |
| `.fbx` (static) | `CookStaticMeshFromFbx` / Editor Import |
| `.gltf` / `.glb` | `CookStaticMeshFromGltf` (+ optional `.lmat` from PBR) |

**Runtime:** `ResourceCache::LoadStaticMesh` accepts **`.lmesh` only**. Source `.obj` / `.fbx` / `.gltf` are import/cook inputs — cook to `.lmesh` before use in levels. In a `.llev`, a `StaticMesh` actor stores its mesh and material as relative paths (`assets/imported/Prop/Prop.lmesh`, `Materials/M_Wood.lmat`).

---

## Skeletal — `.lskel` / `.lskm` / `.lanim`

### Skeleton — `.lskel` (`LSK1` = `0x314B534C`)

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LSK1` |
| 4–7 | Version (`uint32`, 1) |
| 8… | `nameLen` + UTF-8 name |
| … | `boneCount` (`uint32`) |
| per bone | `nameLen` + name, `parent` (`int32`), `inverseBind` (16 × `float`) |

### Skeletal mesh — `.lskm` (`LKM1` = `0x314D4B4C`)

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LKM1` |
| 4–7 | Version |
| 8… | name, skeletonRel (→ `.lskel`), materialRel (→ `.lmat`) |
| … | `localMin[3]`, `localMax[3]` |
| … | `vertexCount`, `indexCount` |
| … | Skinned vertices + indices |

Vertex: pos 3f, normal 3f, uv 2f, tangent 4f, bone indices 4×i32, weights 4f.

### Animation — `.lanim` (`LAN1` = `0x314E414C`)

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LAN1` |
| 4–7 | Version |
| 8… | name, skeletonRel, `durationSeconds`, `framesPerSecond` |
| … | `frameCount`, `boneCount` |
| … | Pose matrices `[frame][bone]` × 16 floats (`mat4` column-major) |

**Loaders:** `LoadSkeleton` / `LoadSkeletalMesh` / `LoadAnimSequence`  
**Cook:** `CookCharacterFromFbx` / `CookAnimSequenceFromFbx`

### Character folder layout

```text
Bot.lskel
Bot.lskm
Materials/M_Bot.lmat
Anims/BreathingIdle.lanim
Anims/Running.lanim
…
Bot_Locomotion.blendspace1d.json
Bot.lchar
```

`.lchar` is INI-style (like `.lmat`). BlendSpace1D samples stay in a small JSON for now; anim paths point at `.lanim`.

Example recipe: `Templates/ThirdPerson/Content/assets/characters/bot/cook-bot.json` via `Scripts\cook.bat`.

---

## Lightmap — `.lm`

Written by Editor **Build Lights** (`leon::editor::BakeLevelLightmaps`):

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LM01` |
| 4–7 | Width (`uint32`) |
| 8–11 | Height (`uint32`) |
| 12… | RGBA8 pixels |

Beside the level: `Lightmaps/LM_<lightmapId>.lm`. Runtime: `LightmapIO` / `LoadLevelFile`. See [LEVELS.md](LEVELS.md#lightmaps).

---

## Import & cook

### Editor (File → Import…)

| Mode | Output |
| --- | --- |
| Static Mesh (OBJ / FBX / glTF) | `assets/imported/<Name>/<Name>.lmesh` (+ `.lmat` for glTF) |
| Character (FBX skinned) | `assets/characters/<Name>/` (`.lskel` / `.lskm` / `.lanim` / …) |
| Animation (FBX) | `Anims/<Name>.lanim` (needs existing `.lskel`) |

### CLI (`leon-cook`)

```bat
Scripts\cook.bat Templates\ThirdPerson\Content\assets\characters\bot\cook-bot.json

leon-cook staticmesh --obj mesh.obj --out mesh.lmesh
leon-cook staticmesh --fbx mesh.fbx --out mesh.lmesh
leon-cook staticmesh --gltf mesh.gltf --out mesh.lmesh --materials mats/
leon-cook character --name Bot --mesh idle.fbx --run run.fbx --out <dir>
leon-cook anim --fbx clip.fbx --skeleton Bot.lskel --name Clip --out Anims/Clip.lanim
leon-cook recipe <file.json>
```

Recipe step schema, lean CMake deps (`leon_engine_cook` / `leon_resource_tools`), and `leon-cli`: **[TOOLS.md](TOOLS.md)**.

---

## glTF interchange

- Library: vendored **cgltf** (`ThirdParty/cgltf`)
- Import cooks to `.lmesh`; PBR metallic-roughness → `.lmat` (+ copied textures)
- Runtime does **not** load glTF live

---

## PS2 cooked — `LPS2`

Host-cooked mesh blob for the EE RHI (`leon::rhi::Ps2DrawCookedMesh`). Not used on Host OpenGL.

| Field | Type | Notes |
| --- | --- | --- |
| `magic` | `char[4]` | `LPS2` |
| `version` | `u32` | `1` |
| `vertexCount` | `u32` | Positions as XYZ float32 follow header |
| `indexCount` | `u32` | `u16` indices follow vertices |

**Budgets (soft):** keep a single mesh under ~64 KiB cooked; GS local mem ~4 MiB total. Cook rejects (future `leon-cook platform: ps2`) should fail when vertexCount exceeds project limits.

**Layout on target:** stage beside the ELF under `host:Projects/Ps2Lab/Content/Meshes/` (PCSX2 hostfs) or pack into ISO. See [SETUP — PS2](SETUP.md#ps2-emotion-engine).

Recipe sketch (host Tools):

```json
{
  "platform": "ps2",
  "staticmesh": {
    "source": "Content/Meshes/SM_Triangle.obj",
    "output": "Content/Meshes/SM_Triangle.lps2"
  }
}
```

---

## Code map

| Concern | Location |
| --- | --- |
| `.lmat` I/O | `Engine/Renderer` — `LeonMaterialFormat` |
| `.lmesh` I/O | `LeonMeshFormat` (shipping) |
| DCC → `.lmesh` cook | `leon_import`: `FbxStaticMesh`, `GltfImport`, `ObjImport`, `StaticMeshCook` |
| Skeletal cook / load | `Engine/Content` — `CookedSkeletal` |
| Cache | `ResourceCache` (OpenGL plugin) |
| Editor import | `Editor/Importers` |
| Offline cook | `Tools/AssetPipeline/leon-cook` + `Tools/ResourceTools` ([TOOLS.md](TOOLS.md)) |
| PS2 `LPS2` draw | `Plugins/RHI/PS2` — `Ps2DrawPrimitives` |
