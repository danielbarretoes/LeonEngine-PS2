# Leon asset formats

**Audience:** content authors and tool writers
**Also:** [LEVELS.md](LEVELS.md) (`.llev` levels, lightmaps, project packs) · [TOOLS.md](TOOLS.md) (LeonCook) · [SETUP.md](SETUP.md)

Runtime formats are Leon binaries plus a few small INI-style text files. DCC sources (OBJ, FBX, glTF) are cooked with LeonCook; the runtime never loads them. The desktop runtime (`Renderer`, `Engine` modules) reads these files; the PS2 runtime does not load any of them yet (see [PS2](#ps2)).

> Unreal `.uasset` / `.umap` are proprietary. Leon does not read or write them. Interchange with Blender / Unreal goes through FBX or glTF, cooked to Leon formats.

---

## Extension cheat sheet

| Ext | Kind | Role | Reader / writer |
| --- | --- | --- | --- |
| `.lmesh` | Binary `LMSH` | Cooked static mesh | `LeonMeshFormat` (RenderCore) |
| `.lmat` | INI text | Material | `LeonMaterialFormat` (Renderer) |
| `.llev` | Binary `LLEV` | Level | `LeonLevelFormat` (Engine), see [LEVELS.md](LEVELS.md) |
| `.lskel` | Binary `LSK1` | Skeleton | `CookedSkeletal` (Engine) |
| `.lskm` | Binary `LKM1` | Skeletal mesh | `CookedSkeletal` (Engine) |
| `.lanim` | Binary `LAN1` | Animation sequence | `CookedSkeletal` (Engine) |
| `.lchar` | INI text | Character package (mesh + animation refs) | `CookedSkeletal` (Engine) |
| `*.blendspace1d.json` | JSON | 1D blend space | `CookedSkeletal` (Engine) |
| `.lm` | Binary `LM01` | Lightmap | `LightmapIO` (Engine), load only |
| `leon.game.json` | JSON | Runtime project pack marker | `FProjectDescriptor` (Projects) |
| `.leonproject` / `.leonplugin` | JSON | Build descriptors | LeonBuildTool (CMake) |
| `.png` (and other stb_image formats) | Image | Textures | `FResourceCache::LoadTexture` (Renderer) |
| `.hdr` | Radiance HDR | Environment map | `FResourceCache::LoadEnvMap` (Renderer) |
| `.obj` / `.fbx` / `.gltf` / `.glb` | Source | Cook input only | MeshUtilities, AnimationCore |

There is no `.lasset` format in the engine.

Engine content lives in `Engine/Content` (`Materials/M_Default.lmat`, `Materials/M_WorldGrid.lmat`, `Materials/M_SolidMetal.lmat`, `Textures/T_Default_D.png`, `Hdr/AutumnFieldPuresky1k.hdr`, `LevelTemplates/*.llev`) and GLSL shaders in `Engine/Shaders`. Paths inside assets are resolved with `FPaths::ResolveAssetPath`, which checks the active project's `Content/` first, then `Engine/Content` (or `Engine/Shaders` for `Shaders/...` keys).

---

## Static mesh — `.lmesh`

Header: `Engine/Source/Runtime/RenderCore/Public/LeonMeshFormat.h`. API: `IsLeonMeshPath`, `LoadLeonMeshFile(Path, FMeshData&)`, `SaveLeonMeshFile(Path, const FMeshData&)`.

The file is written with raw struct writes (little-endian on every supported host):

```text
header (52 bytes, packed):
  char magic[4]        = "LMSH"
  u32  version         = 1          // loader rejects any other version
  u32  flags           = 0
  u32  vertexCount                   // must be > 0
  u32  indexCount                    // must be > 0
  u32  submeshCount                  // writer always writes >= 1
  u32  materialSlotCount             // writer always writes >= 1
  f32  aabbMin[3], aabbMax[3]
FVertex  vertices[vertexCount]       // 48 bytes each
u32      indices[indexCount]
submesh  sections[submeshCount]      // u32 indexOffset, u32 indexCount, u32 materialIndex
char     slots[materialSlotCount][]  // null-terminated strings
```

`FVertex` (`RenderCore/Public/Vertex.h`): `Position` 3 × f32, `Normal` 3 × f32, `TexCoord` 2 × f32, `Tangent` 4 × f32 (`w` = bitangent handedness).

Material slot strings carry the source's diffuse texture path per slot (from the OBJ `.mtl`, for example). The loader keeps a slot string only when it contains `.png` or `.jpg` and binds it as the albedo map of that material slot. A `submeshCount` of 0 on load means one section covering all indices.

**Cook:** `FStaticMeshBuilder::CookFromObj` / `CookFromFbx` / `CookFromGltf` (`Engine/Source/Developer/MeshUtilities/Public/StaticMeshBuilder.h`), driven by `LeonCook staticmesh` or a recipe step. glTF / GLB import (vendored cgltf) merges every primitive of the first mesh and, with a materials directory, writes one `M_<Name>.lmat` per material plus copied textures.

**Runtime:** `FResourceCache::LoadStaticMesh` accepts `.lmesh` only and logs an error for any other extension. In a `.llev`, a `StaticMesh` actor stores its mesh and material as content-relative paths.

---

## Material — `.lmat`

Header: `Engine/Source/Runtime/Renderer/Public/LeonMaterialFormat.h`. INI-style text, similar to an Unreal Material Instance's parameters.

```ini
# Leon Material (.lmat)

[Info]
Name=M_Default
ShadingModel=DefaultLit

[Parameters]
BaseColor=1.0,1.0,1.0
Specular=0.04,0.04,0.04
Metallic=0.0
Roughness=0.4472135954999579
Opacity=1.0
Shininess=8.0
UVScale=1.0,1.0
CastsShadows=true
PlanarMirror=false

[Textures]
BaseColorMap=Textures/T_Default_D.png
NormalMap=
```

(`Engine/Content/Materials/M_Default.lmat`.)

| Section | Key (aliases) | Value |
| --- | --- | --- |
| `[Info]` | `Name` | Display name (default `Material`) |
| `[Info]` | `ShadingModel` (`Shading`) | `DefaultLit` or `Unlit` (anything other than `Unlit` is lit) |
| any other | `BaseColor` (`Albedo`) | `r,g,b` |
| | `Specular` | `r,g,b` |
| | `Metallic` | Clamped to [0, 1] |
| | `Roughness` | Clamped to [0.04, 1]; when absent it is derived from `Shininess` |
| | `Opacity` (`Alpha`) | Clamped to [0, 1] |
| | `Shininess` | Blinn-Phong exponent |
| | `UVScale` (`Tiling`) | `u,v` (a single value applies to both) |
| | `CastsShadows`, `PlanarMirror` | `true/false`, `1/0`, `yes/no`, `on/off` |
| | `Unlit` | `true` forces the Unlit shading model |
| `[Textures]` | `BaseColorMap` (`AlbedoMap`, `DiffuseMap`) | Texture path, `checker` (procedural 64 px checker), or empty |
| `[Textures]` | `NormalMap` | Texture path, `bump` (procedural normal map), or empty |

Section and key names are case-insensitive. `#` and `;` start comments. Unknown keys are reported on stderr and ignored. Texture paths are resolved with `FPaths::ResolveAssetPath` (content-relative, not relative to the `.lmat` file).

**API:** `LoadLeonMaterialDocument` (parse only, paths kept as strings in `FLeonMaterialDocument`), `LoadLeonMaterialFile` (parse and load textures into an `FMaterial`), `SaveLeonMaterialFile`, `MakeDefaultLeonMaterialText`. Runtime cache: `FResourceCache::LoadMaterial` / `InvalidateMaterial`. Validation: `ValidateMaterialFile` (`Engine/Source/Runtime/Engine/Public/Validation/ContentValidator.h`).

---

## Level — `.llev`

Binary container (`LLEV`, little-endian, string table + meta + camera + actors + lights). Writers emit version 2; readers accept versions 1 and 2. Actors reference materials by `.lmat` path and meshes by `.lmesh` path; there are no inline materials. Full layout, loading pipeline and project packs: **[LEVELS.md](LEVELS.md)**.

---

## Skeletal — `.lskel` / `.lskm` / `.lanim`

Header: `Engine/Source/Runtime/Engine/Public/Animation/CookedSkeletal.h`. All three share: `u32 magic`, `u32 version` (`CookedFormatVersion` = 1, loaders reject other versions), and strings stored as `u32 byteLength` + UTF-8 bytes (no terminator). Values use host byte order (little-endian on every supported host).

### Skeleton — `.lskel` (`LeonSkeletonMagic` = `0x314B534C`, "LSK1")

```text
u32 magic, u32 version
string name
u32 boneCount
per bone: string name, i32 parentIndex, f32 inverseBindPose[16]
```

### Skeletal mesh — `.lskm` (`LeonSkelMeshMagic` = `0x314D4B4C`, "LKM1")

```text
u32 magic, u32 version
string assetName, string skeletonRel (-> .lskel), string materialRel (-> .lmat)
f32 localMin[3], localMax[3]
u32 vertexCount (> 0), u32 indexCount
vertices[vertexCount]: position 3f, normal 3f, texCoord 2f, tangent 4f, boneIndices 4 x i32, boneWeights 4f   (80 bytes)
u32 indices[indexCount]
```

`skeletonRel` is resolved against the `.lskm` directory unless the caller passes a skeleton.

### Animation — `.lanim` (`LeonAnimMagic` = `0x314E414C`, "LAN1")

```text
u32 magic, u32 version
string name, string skeletonRel
f32 durationSeconds, f32 framesPerSecond
u32 frameCount (> 0), u32 boneCount (> 0)
f32 localPose[frameCount][boneCount][16]   // glm::mat4, column-major
```

Loader limits: strings up to 1 MiB, 2,000,000 vertices, 6,000,000 indices, 512 bones per `.lanim`, 100,000 frames.

**Loaders:** `LoadSkeleton`, `LoadSkeletalMesh`, `LoadAnimSequence`. **Writers:** `SaveSkeletonLeon`, `SaveSkeletalMeshLeon`, `SaveAnimSequenceLeon`. **Cook:** `CookCharacterFromFbx`, `CookAnimSequenceFromFbx` (FBX import through ufbx in AnimationCore).

### Blend space — `*.blendspace1d.json`

```json
{
  "version": 1,
  "name": "Bot_Locomotion",
  "axisMin": 0.0,
  "axisMax": 1.0,
  "samples": [
    { "anim": "Anims/BreathingIdle.lanim", "position": 0.0 },
    { "anim": "Anims/Running.lanim", "position": 1.0 }
  ]
}
```

`samples[].anim` is relative to the blend space file. At least one sample is required. API: `LoadBlendSpace1DJson` / `SaveBlendSpace1DJson` (`FBlendSpace1DAssetDesc`).

### Character package — `.lchar`

INI-style, like `.lmat`. Paths are relative to the `.lchar` file.

```ini
# Leon Character (.lchar) — version 1

[Info]
Name=Bot
FitHeight=1.85

[Mesh]
SkeletalMesh=Bot.lskm

[Animation]
BlendSpace=Bot_Locomotion.blendspace1d.json
JumpStart=Anims/JumpingUp.lanim
FallLoop=Anims/FallingIdle.lanim
Land=Anims/FallingToLanding.lanim
```

`JumpStart` / `FallLoop` / `Land` are optional. API: `LoadCharacterVisualLchar` / `SaveCharacterVisualLchar` (`FCharacterVisualDesc`). `LoadCharacterVisual` also accepts a legacy JSON file (`skeletalMesh`, `blendSpace`, `fitHeight`, `jumpStart`, `fallLoop`, `land`) when the extension is `.json`. `USkeletalMeshComponent` loads characters through `LoadCharacterVisual`.

### Character folder written by `LeonCook character`

```text
<Name>.lskel
<Name>.lskm
Materials/M_<Name>.lmat
Anims/BreathingIdle.lanim
Anims/Running.lanim
Anims/JumpingUp.lanim / FallingIdle.lanim / FallingToLanding.lanim   (optional clips)
<Name>_Locomotion.blendspace1d.json
<Name>.lchar
```

---

## Lightmap — `.lm`

Header: `Engine/Source/Runtime/Engine/Public/Level/LightmapIO.h`.

| Offset | Content |
| --- | --- |
| 0–3 | Magic `LM01` |
| 4–7 | Width (`u32`, host byte order) |
| 8–11 | Height (`u32`) |
| 12… | `width * height` RGBA8 pixels |

Zero or larger-than-4096 dimensions are rejected. Loading only: `LoadLightmapFile`, `LoadLevelLightmaps`. No tool in the repository writes `.lm` files at the moment (the lightmap baker was part of the removed Editor). Details in [LEVELS.md](LEVELS.md#lightmaps).

---

## Project pack — `leon.game.json`

A runtime project pack is a folder `Projects/<Name>/` with a `leon.game.json` marker and levels under `Content/Levels/`. `FProjectDescriptor::Resolve(PackName)` (`Engine/Source/Runtime/Projects/Public/ProjectDescriptor.h`) finds the folder through `FPaths::ResolveAssetPath("Projects/<Name>")` and reads one key:

```json
{ "defaultLevel": "Levels/MainMenu.llev" }
```

`FProjectDescriptor::DefaultLevelKey()` returns the file stem (`MainMenu`), which is matched against the level catalog. A missing or unreadable marker keeps the pack root and falls back to the first level. The repository currently ships no runtime pack; see [LEVELS.md](LEVELS.md#project-packs).

---

## Build descriptors — `.leonproject` / `.leonplugin`

JSON read by LeonBuildTool in CMake (`Engine/Source/Programs/LeonBuildTool/System/ProjectDescriptor.cmake`, `PluginDescriptor.cmake`), equivalent to Unreal's `.uproject` / `.uplugin`. The runtime does not read them.

`.leonproject` (the file name is the project name):

| Field | Used for |
| --- | --- |
| `Modules[].Name` | Project modules added to the project's game targets |
| `Plugins[].Name` / `Plugins[].Enabled` | Enable or disable a plugin for every target of the project (`Enabled` defaults to true) |
| `TargetPlatforms[]` | Recorded (`LEON_PROJECT_TARGET_PLATFORMS`) |
| `FileVersion`, `EngineAssociation`, `Category`, `Description`, `Modules[].Type` / `LoadingPhase` | Informational |

`.leonplugin` (the file name is the plugin name; discovered under `Engine/Plugins` and `<Project>/Plugins`):

| Field | Used for |
| --- | --- |
| `EnabledByDefault` | Enabled for game targets unless a project or target disables it (defaults to false). Program targets ignore it and only get plugins enabled by their project or their `Target.cmake` |
| `Modules[].Name` | Modules added to targets that enable the plugin |
| `Modules[].PlatformAllowList[]` | Platforms (or platform groups) the module builds for |
| `FileVersion`, `Version`, `VersionName`, `FriendlyName`, `Description`, `Category`, `Modules[].Type` / `LoadingPhase` | Informational |

Examples: `Game/ThirdPerson/ThirdPerson.leonproject`, `Engine/Plugins/Runtime/JoltPhysics/JoltPhysics.leonplugin`.

---

<a id="ps2-cooked-lps2"></a>

## PS2

The PS2 runtime (`Engine/Platforms/PS2/Source/Runtime/PS2RHI`) draws with the Graphics Synthesizer directly and does not load `.lmesh`, `.lmat` or `.llev` files. The ThirdPerson demo builds its textures, materials and level in code.

### Cooked mesh blob — `LPS2`

`FPS2RHI::DrawCookedMesh(Data, Size)` validates this header:

| Field | Type | Notes |
| --- | --- | --- |
| `magic` | `char[4]` | `LPS2` |
| `version` | `u32` | `1` |
| `vertexCount` | `u32` | Must be > 0 |
| `indexCount` | `u32` | |

Vertex upload is not implemented yet: a valid blob draws a placeholder triangle. No tool produces `LPS2` blobs; LeonCook has no PS2 mode.

### Materials and textures

`FPS2Material` (`PS2RHI/Public/PS2RHITypes.h`) mirrors a subset of `.lmat`:

| Field | Notes |
| --- | --- |
| `BaseColorR` / `BaseColorG` / `BaseColorB` | RGB tint |
| `BaseColorMap` | Optional `const FPS2Texture*` |
| `ShadingModel` | `EMaterialShadingModel::DefaultLit` or `Unlit` |

`FPS2Texture` holds an RGBA8 texture in GS memory: `Create(Width, Height, Rgba)`, `CreateFromAlignedRgba`, and the procedural `CreateChecker(Size)` / `CreateGrid(Size)`. There is no image file loading on PS2. Lights and view: `FPS2DirectionalLight`, `FPS2ViewTarget`, `FPS2RHI::SetAmbientLightColor`.

---

## Code map

| Concern | Location |
| --- | --- |
| `.lmesh` I/O | `Engine/Source/Runtime/RenderCore` — `LeonMeshFormat`, `FMeshData`, `FVertex` |
| `.lmat` I/O | `Engine/Source/Runtime/Renderer` — `LeonMaterialFormat`, `FMaterial` (`RenderCore/Public/Material.h`) |
| GPU resource cache | `Engine/Source/Runtime/Renderer` — `FResourceCache` (`UStaticMesh`, `UTexture2D`, `FEnvironmentMap`, materials) |
| `.llev` I/O and apply | `Engine/Source/Runtime/Engine` — `LeonLevelFormat`, `LevelLoader` |
| Skeletal cook / load | `Engine/Source/Runtime/Engine` — `CookedSkeletal` |
| `.lm` load | `Engine/Source/Runtime/Engine` — `LightmapIO` |
| Level / material validation | `Engine/Source/Runtime/Engine` — `ContentValidator` |
| Project packs | `Engine/Source/Runtime/Projects` — `FProjectDescriptor`; `Engine/Source/Runtime/Core` — `FPaths` |
| DCC → `.lmesh` | `Engine/Source/Developer/MeshUtilities` — `FStaticMeshBuilder`, `ObjImport`, `FbxStaticMesh`, `GltfImport` |
| Offline cook | `Engine/Source/Developer/Cooker` + `Engine/Source/Programs/LeonCook` ([TOOLS.md](TOOLS.md)) |
| PS2 drawing, materials, textures | `Engine/Platforms/PS2/Source/Runtime/PS2RHI` — `FPS2RHI`, `FPS2Material`, `FPS2Texture` |
