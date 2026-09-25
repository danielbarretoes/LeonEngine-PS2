# Leon asset formats

**Audience:** content authors and tool writers
**Also:** [LEVELS.md](LEVELS.md) (`.llev` levels) · [TOOLS.md](TOOLS.md) (LeonCook) · [SETUP.md](SETUP.md)

Runtime formats are Leon binaries plus a few small INI-style text files. DCC sources (OBJ, FBX, glTF) are cooked with LeonCook; the runtime never loads them. The desktop runtime (`Engine` and `RenderCore` modules; the `Renderer` only uploads what they read) reads these files; the PS2 runtime does not load any of them yet (see [PS2](#ps2)). Since 0.15.0 CoreUObject saves and loads UObjects as `.lasset` / `.lmap` [packages](#packages--lasset--lmap), and since P14 the engine's assets are [asset classes](#asset-classes) that save to them; the legacy files still on disk become asset objects through the transitional [legacy asset loader](#legacy-asset-loader-transitional).

> Unreal `.uasset` / `.umap` are proprietary. Leon does not read or write them. Interchange with Blender / Unreal goes through FBX or glTF, cooked to Leon formats. The `.lasset` layout follows UE 4.27's package structure (summary, name / import / export tables, tagged properties) but is Leon's own binary format.

---

## Extension cheat sheet

| Ext | Kind | Role | Reader / writer |
| --- | --- | --- | --- |
| `.lasset` / `.lmap` | Binary `LEON` | UObject package: an asset / a map (`PKG_ContainsMap`) | `UPackage::Save`, `LoadPackage` / `LoadObject` (CoreUObject), see [Packages](#packages--lasset--lmap) |
| `.lmesh` | Binary `LMSH` | Cooked static mesh | `LeonMeshFormat` (RenderCore), `FLegacyAssetLoader::LoadStaticMesh` (Engine) |
| `.lmat` | INI text | Material | `LeonMaterialFormat` (RenderCore), `FLegacyAssetLoader::LoadMaterial` (Engine) |
| `.llev` | Binary `LLEV` | Level | `LeonLevelFormat` (Engine), see [LEVELS.md](LEVELS.md) |
| `.lproj` / `.lplugin` | JSON | Build descriptors | LeonBuildTool (CMake) |
| `.png` (and other stb_image formats) | Image | Textures | `FLegacyAssetLoader::LoadTexture` (Engine) |
| `.wav` | RIFF / WAVE, PCM16 | Sounds | `FLegacyAssetLoader::LoadSoundWave` (Engine); `FAudioDevice` plays files by path (AudioMixer) |
| `.obj` / `.fbx` / `.gltf` / `.glb` | Source | Cook / import input only | MeshUtilities |

No engine content is a package yet: the asset classes (`UStaticMesh`, `UTexture2D`, `UMaterial`, …) arrived in P14's first part, the content moves to `.lasset` packages in its second part and the `.lmap` maps come in P15; until then `.lmesh`, `.lmat`, `.llev`, PNG and WAV files stay the files on disk, loaded as asset objects. The cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, `*.blendspace1d.json`), `.lm` lightmaps, `.hdr` environment maps and the `leon.game.json` pack marker were removed in 0.12.0; skeletal assets return as `USkeletalMesh` / `UAnimSequence` `.lasset` packages and static lighting as `<Map>_BuiltData.lasset`.

Engine content lives in `Engine/Content` (`Materials/M_Default.lmat`, `Materials/M_WorldGrid.lmat`, `Materials/M_SolidMetal.lmat`, `Textures/T_Default_D.png`, `LevelTemplates/*.llev`) and GLSL shaders in `Engine/Shaders`. Paths inside assets are resolved with `FPaths::ResolveLegacyContentPath`, which checks the path as given, `Engine/Shaders` for `Shaders/...` keys, the project content and the engine content.

---

## Asset classes

Since P14 the engine's assets are UObjects in the Engine module, with UE 4.27's names and headers. Each saves to a
`.lasset` [package](#packages--lasset--lmap) as its tagged properties (its `UPROPERTY`s, a delta against the class
default object) followed by a native tail (`Serialize`), where the big payloads are `FByteBulkData` at the end of the
file. Other assets are referenced through `UPROPERTY` object pointers: in another package they are imports, which
load that package first. The tests save every class to memory and load it back
(`System.Engine.Assets.*RoundTrip`).

| Class (header, `Engine/Classes/`) | Tagged properties | Native tail |
| --- | --- | --- |
| `UTexture` (`Engine/Texture.h`), abstract | `SRGB` (recorded; the forward renderer uploads the texels as they are) | — |
| `UTexture2D` (`Engine/Texture2D.h`) | — | `FTexturePlatformData`: `int32` SizeX, SizeY, `uint8` `EPixelFormat` (UE values: `PF_R8G8B8A8` = 37, `PF_B8G8R8A8` = 2), `int32` mip count, then per mip `int32` SizeX, SizeY and its texels as bulk data, bottom row first. Leon stores mip 0; the renderer builds the others when it uploads |
| `UStaticMesh` (`Engine/StaticMesh.h`) | `StaticMaterials` (`FStaticMaterial`: `MaterialInterface`, `MaterialSlotName`), `BodySetup` (an inner object) | the local bounding box (`FBox`), then one bulk payload of `FStaticMeshLODResources` (one LOD): vertex count and each `FVertex` field by field (position, normal, UV, tangent), the `uint32` indices, section count and each section's index offset, index count and material slot |
| `UBodySetup` (`PhysicsEngine/BodySetup.h`) | `AggGeom` (`FKAggregateGeom`: `BoxElems`, each `FKBoxElem` Center, Rotation, X, Y, Z in cm), `CollisionTraceFlag` (`ECollisionTraceFlag`) | — |
| `UMaterialInterface` (`Materials/MaterialInterface.h`), abstract; `UMaterial` (`Materials/Material.h`) | `ShadingModel` (`MSM_Unlit`, `MSM_DefaultLit`), `BaseColor`, `Specular` (`FLinearColor`, linear RGB), `Metallic`, `Roughness`, `Opacity`, `Shininess`, `UVScale` (`FVector2D`), `bCastsShadows`, `bPlanarMirror`, `BaseColorMap`, `NormalMap` (`UTexture2D*`): the `.lmat` set | — |
| `USkeleton` (`Animation/Skeleton.h`) | `Sockets` (`USkeletalMeshSocket` inner objects: `SocketName`, `BoneName`, `RelativeLocation`, `RelativeRotation`, `RelativeScale`) | `FReferenceSkeleton`: the bone names (`FName`s, in the name table), the parent indices and the inverse bind pose (`FMatrix` each) |
| `USkeletalMesh` (`Engine/SkeletalMesh.h`) | `Skeleton`, `Materials` (`FSkeletalMaterial`) | the bounding box, then one bulk payload of the skinned vertices (`FSkeletalVertex`: position, normal, UV, tangent, 4 bone indices, 4 weights) and the indices |
| `UAnimationAsset` → `UAnimSequenceBase` → `UAnimSequence` (`Animation/AnimSequence.h`) | `Skeleton`, `SequenceLength`, `RateScale`, `bLoop`, `NumFrames`, `FrameRate` | one bulk payload of the tracks, one per bone (`FRawAnimSequenceTrack`: one model-space `FMatrix` key per frame) |
| `UBlendSpaceBase` → `UBlendSpace1D` (`Animation/BlendSpace1D.h`) | `Skeleton`, `BlendParameters[3]` (`FBlendParameter`: DisplayName, Min, Max, GridNum), `SampleData` (`FBlendSample`: `Animation`, `SampleValue`, `RateScale`) | — |
| `USoundBase` → `USoundWave` (`Sound/SoundWave.h`) | `Duration`, `NumChannels`, `SampleRate` | `RawPCMData`: the interleaved 16-bit PCM samples as bulk data |
| `UDataAsset` (`Engine/DataAsset.h`), abstract | the game subclass's `UPROPERTY`s | — |
| `UCommandlet` (`Commandlets/Commandlet.h`), abstract, transient | `HelpDescription`, `HelpUsage`, `IsServer`, `IsClient`, `IsEditor`, `LogToConsole`, `ShowErrorCount`; `Main(Params)`, `ParseCommandLine` (never saved: the base of P14 part 2's commandlets) | — |

**Bulk data.** A texture and a sound keep their payload in their `FByteBulkData` (as UE's mips and raw data do). The
meshes and the clips keep CPU arrays (the renderer and the physics scene read them) and go through
`SerializeBulkPayload` (`Engine/Private/AssetBulkData.h`): while saving, the arrays are written into a bulk data member
of the asset, which the package saver appends after the exports (so the member must outlive `Serialize`); while
loading, they are read back from it and the payload is freed. A payload that does not read back whole is reported
(`LogEngine`) and the arrays are emptied.

**GPU copies.** The renderer keeps one per texture and mesh, keyed by the asset, made the first time it is drawn. The
asset frees it when its data changes (`UTexture::UpdateResource`, `UStaticMesh::InitResources`, called by
`SetPlatformData`, `BuildFromMeshData` and `PostLoad`) and in `BeginDestroy` (`ReleaseResource` /
`ReleaseResources`), through `IRendererModule::ReleaseAssetResources` ([ARCHITECTURE.md §12](ARCHITECTURE.md#12-rendering-desktop)).

**Deviations from UE 4.27.** No texture source, compression, LOD groups or streaming; one static mesh LOD, no
mesh description or nanite; materials are fixed parameters, not an expression graph compiled to shaders; the
animation keys are model-space matrices (UE: compressed local position, rotation and scale keys) and the reference
skeleton keeps the inverse bind pose; a sound keeps PCM16 (UE: the imported `.wav` and the cooked compressed data);
no asset registry, primary data assets or import data (`UAssetImportData` comes with the editor module).

### Legacy asset loader (transitional)

Until P14 part 2 migrates the content to `.lasset` packages (and deletes it), `FLegacyAssetLoader`
(`Engine/Public/LegacyAssetLoader.h`) turns the legacy files into asset UObjects. It is the only reader of `.lmesh`,
`.lmat`, image and `.wav` files at run time:

| Entry point | Source | Result |
| --- | --- | --- |
| `LoadStaticMesh(Filename)` | `.lmesh` ([below](#static-mesh--lmesh)) | a transient `UStaticMesh`, with one transient `UMaterial` per material slot (the slot string's diffuse map as its `BaseColorMap`) |
| `LoadTexture(Filename)` | PNG, JPEG, TGA, ... (stb_image) | a transient `UTexture2D`, RGBA8 |
| `LoadMaterial(Filename)` | `.lmat` ([below](#material--lmat)) | a transient `UMaterial` with its maps loaded (`checker` / `bump` are the engine's `DefaultTexture` / `T_Default_Bump_N`) |
| `LoadSoundWave(Filename)` | RIFF / WAVE, 16-bit PCM (plain or extensible); anything else is an error | a transient `USoundWave` |
| `LoadEngineObject(Class, ObjectPath)` | the object in memory, else its `.lasset` package, else the table below | the engine asset, made once at its final path and rooted |
| `GetSphereMesh(Segments, Rings)` | procedural | `/Engine/BasicShapes/Sphere` for 24 × 16, else a transient sphere per tessellation |

A legacy file's asset lives in a transient package named after the file, `/Temp/LegacyAssets/<Root>/<folders>/<File>_<ext>`
(`Engine` for the engine content, `Game` for the project content, `External/<drive and folders>` otherwise; characters
a name cannot hold become `_`), named after the file's base name (`GetLegacyPackageName`). The package is the cache:
loading the same file returns the living object. Nothing else keeps it alive, so the garbage collector frees it with
its last user (a level's assets go with its world) and the next load reads the file again. Transient objects are never
saved: a package referencing one saves a null reference.

The engine assets, made in memory at the paths part 2 will save them to:

| Object path | Made from |
| --- | --- |
| `/Engine/EngineMaterials/<Name>` | `Materials/<Name>.lmat`, else `Textures/<Name>.png` (`FPaths::ResolveLegacyContentPath`: the project content first, then the engine content) |
| `/Engine/EngineMaterials/M_Default` | as above; without the file, the grey checker material (white, 8 shininess, `DefaultTexture`) |
| `/Engine/EngineResources/DefaultTexture` | the procedural grey checker, 64 × 64 (UE: DefaultTexture) |
| `/Engine/EngineMaterials/T_Default_Bump_N` | the procedural bump normal map, 256 × 256, not sRGB |
| `/Engine/BasicShapes/Cube`, `Plane`, `Sphere` | the procedural 100 cm cube, plane (Z up, UVs 0-1) and 24 × 16 UV sphere, without material slots |

The config names the defaults, as UE's `BaseEngine.ini` does, and `UEngine` reads them (`UPROPERTY(GlobalConfig)`
`FSoftObjectPath`s); `UEngine::InitializeObjectReferences` loads `DefaultTexture` and `DefaultBumpNormalTexture`, and
`UMaterial::GetDefaultMaterial` the default material (what a mesh slot without a material draws with):

```ini
[/Script/Engine.Engine]
DefaultMaterialName=/Engine/EngineMaterials/M_Default.M_Default
DefaultTextureName=/Engine/EngineResources/DefaultTexture.DefaultTexture
DefaultBumpNormalTextureName=/Engine/EngineMaterials/T_Default_Bump_N.T_Default_Bump_N
```

Once made, the engine assets are found like loaded ones (`LoadObject`, `FindObject`, `TSoftObjectPtr`). The UI sounds
are not assets yet: `FAudioDevice::PlayUiSound` (AudioMixer, below Engine) looks for `Audio/UI/UI_*.wav` files, none
of which exist, and plays procedural tones.

---

## Packages — `.lasset` / `.lmap`

A package holds UObjects, as UE's `.uasset` / `.umap` do (plan decision D13). API (CoreUObject, [README](../Engine/Source/Runtime/CoreUObject/README.md)): `UPackage::SavePackage` / `Save` / `SaveToMemory`, `LoadPackage`, `LoadObject<T>`, `LoadClass<T>`, `StaticLoadObject`, `FSoftObjectPath::TryLoad`, `TSoftObjectPtr::LoadSynchronous`; names and files: `FPackageName` (`Misc/PackageName.h`).

**Names.** A package is named by its long package name, `/Game/Maps/Arena`: a mount point root plus a path. `/Engine/` maps to `Engine/Content/`, `/Game/` to the project's `Content/`, and `FPackageName::RegisterMountPoint` adds others (plugins, tests). `/Script/<Module>` names a module's compiled-in package: it is valid but has no file. An object is named by its path, `/Game/Maps/Arena.Arena`, with `:` before a subobject of a top-level object (`/Game/Maps/Arena.Arena:PersistentLevel`). A map is saved as `.lmap` (the save sets `PKG_ContainsMap`), anything else as `.lasset`.

**One file.** Summary, tables, export data and bulk data are in one file. UE 4.27 cooked packages split them into `.uasset` (header), `.uexp` (export data) and `.ubulk` (bulk data); Leon keeps a single file, with the bulk data at its end (D13), so a package is read with one file open (a PS2 CD seek) and needs no companion files in a pak.

### Layout

Little-endian, as FArchive writes it: an `int32` is 4 bytes, `bool` is a `uint32` (0 or 1), an `FString` is its `int32` length including the terminator (0 for the empty string) followed by that many UTF-8 bytes, and an `FName` is 8 bytes: an `int32` index into the name table and an `int32` number (0 = no number, N + 1 for the suffix `_N`). An object reference is an `int32` [`FPackageIndex`](../Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectResource.h): 0 is null, N > 0 the export N − 1, −N the import N − 1.

```text
FPackageFileSummary                         (UObject/PackageFileSummary.h)
  int32   Tag                    0x4E4F454C: the file starts with the bytes "LEON"
  int32   FileVersionUE          ELeonPackageVersion (Core UObject/ObjectVersion.h); 1 in 0.15.0
  int32   FileVersionLicenseeUE  0
  int32   TotalHeaderSize        summary + tables: where the export data starts
  uint32  PackageFlags           PKG_Cooked 0x200, PKG_ContainsMap 0x20000, PKG_FilterEditorOnly 0x80000000, ...
  int32   NameCount, NameOffset
  int32   ExportCount, ExportOffset
  int32   ImportCount, ImportOffset
  int32   SoftPackageReferencesCount, SoftPackageReferencesOffset
  FGuid   Guid                   4 x uint32: FGuid::NewDeterministicGuid(long package name) (MD5)
  FEngineVersion SavedByEngineVersion
          uint16 Major, Minor, Patch; uint32 Changelist; FString Branch   ("0.15.0-0+LeonEngine")
  FString CookedPlatform         empty unless PKG_Cooked
  int64   BulkDataStartOffset
name table          NameCount x FString: every FName string of the package, number-less, sorted, no duplicates
import table        ImportCount x FObjectImport (28 bytes)
                      FName ClassPackage ("/Script/CoreUObject"), FName ClassName ("Class", "Package", ...),
                      int32 OuterIndex (an import; 0 for a package), FName ObjectName
export table        ExportCount x FObjectExport (60 bytes)
                      int32 ClassIndex (a /Script class import), int32 SuperIndex (0), int32 OuterIndex (0 = the
                      package, else an export), FName ObjectName, uint32 ObjectFlags (masked with RF_Load),
                      int64 SerialSize, int64 SerialOffset, bool bForcedExport, bNotForClient, bNotForServer (false),
                      uint32 PackageFlags (0), bool bIsAsset
soft package refs   SoftPackageReferencesCount x FName: the packages the exports' FSoftObjectPaths name, sorted
                    ---- TotalHeaderSize ----
export data         for each export, SerialSize bytes at SerialOffset:
                      tagged properties, ended by the FName None, then the native tail of UObject::Serialize
bulk data           at BulkDataStartOffset: the end-of-file FByteBulkData payloads, in the order they were saved
uint32              0x4E4F454C again: a shorter file is reported as truncated
```

**Imports and exports.** The exports are the objects the save was given: `Base`, the package's objects with the top-level flags (`RF_Public`, `RF_Standalone`), and, recursively, their outers inside the package, their inner objects (default subobjects included) and every object of the package they reference. Transient objects (`RF_Transient`, pending kill, inside a transient outer or of a `CLASS_Transient` class) are never saved, and a reference to one is saved as null; so is a reference to an object of the package that is not exported. Every other referenced object becomes an import, with its outers (up to its package) and its class; an export's class is always an import of a `/Script/<Module>` class. Native objects of `/Script` packages (classes, structs, enums) can be referenced even though they are flagged transient.

### Tagged properties

Each export's data starts with its reflected properties, one tag per saved value (`UObject/PropertyTag.h`, UE 4.27's order):

```text
FName  Name              NAME_None ends the list (and nothing else follows it)
FName  Type              the property class: "IntProperty", "StructProperty", "ArrayProperty", ...
int32  Size              bytes of the value that follows the tag
int32  ArrayIndex        the element of a C array property, else 0
       StructProperty:   FName StructName
       BoolProperty:     uint8 BoolVal (the value; Size is 0)
       Byte/EnumProperty: FName EnumName (None for a plain byte)
       Array/SetProperty: FName InnerType
       MapProperty:      FName InnerType (key), FName ValueType
uint8  HasPropertyGuid   0 (a 1 would be followed by a 16-byte GUID, which Leon skips)
value  Size bytes
```

| Property | Value |
| --- | --- |
| numbers | the C++ value (`int8` … `uint64`, `float`, `double`) |
| `bool` | in the tag; as an element of a container, one byte |
| `FString`, `FText` | an `FString` (`FText` saves its display string) |
| `FName` | an `FName` |
| enum class, `TEnumAsByte` | the enumerator's name as an `FName` ("EMyEnum::Value"), so reordered enumerators keep their meaning; a name the enum lost loads as its `_MAX` value with a warning |
| `UObject*`, `TSubclassOf`, `TWeakObjectPtr` | an `FPackageIndex` |
| `TSoftObjectPtr`, `TSoftClassPtr`, `FSoftObjectPath` | the path: `FName` asset path name + `FString` subobject path |
| struct | its own `Serialize` when `TStructOpsTypeTraits::WithSerializer` (`FSoftObjectPath`); the members in order, untagged, for an immutable struct (the Core math types: `FVector`, `FRotator`, `FQuat`, `FTransform`, `FGuid`, ...); otherwise nested tagged properties ending with None |
| `TArray` | `int32` count; for struct elements a tag of the element type (`StructName`, total size), which a load checks; the elements |
| `TSet` | `int32` 0 (UE's removed-defaults count), `int32` count, the elements |
| `TMap` | `int32` 0, `int32` count, each key then value |

**Delta.** A property is saved only when it differs from the object's archetype: the class default object, or for a default subobject the subobject of the same name in its outer's archetype (built by the same constructor, D12). A struct member is compared member by member against the archetype's struct; elements of containers are saved whole. Transient, deprecated and `CPF_SkipSerialization` properties are never saved; editor-only ones (`#if WITH_EDITORONLY_DATA`) not in a package with `PKG_FilterEditorOnly`.

**Schema evolution.** A load reads tags until None. A tag whose property no longer exists (renamed, removed, or editor-only data on a build without it) is skipped by its size; a property whose type changed is converted when it can be (any integer to any integer, `float` and `double` both ways, a byte to an enum by value, a `TEnumAsByte` to an enum class by enumerator name, `FName` / `FString` / `FText` among themselves, a hard object reference to a soft one, object and class references) and skipped with a `LogClass` warning otherwise. A struct tag of another struct, or a container of other element types, is skipped with a warning.

**Native tail.** After the None tag comes whatever the class's `Serialize` override writes after calling `Super::Serialize(Ar)`: raw members and `FByteBulkData`. A load checks that it reads exactly `SerialSize` bytes.

### Bulk data

`FByteBulkData` (`Serialization/BulkData.h`) writes a header, `uint32` flags (`BULKDATA_PayloadAtEndOfFile` 0x1, `BULKDATA_Unused` 0x20 for an empty payload, `BULKDATA_ForceInlinePayload` 0x40, `BULKDATA_Size64Bit` 0x2000, always set), `int64` element count, `int64` size on disk (equal: no compression) and `int64` offset. In a package the payload goes after all the exports and the offset is relative to `BulkDataStartOffset`; with `BULKDATA_ForceInlinePayload` (and in any archive that is not a package) the payload follows the header and the offset is −1. Payloads load eagerly with their owner.

### Determinism

Saving the same objects gives the same bytes on every run and platform (D13): the name table is sorted (case-insensitively, then case-sensitively) and has no duplicates, the imports and exports are sorted by path name, the package GUID is `FGuid::NewDeterministicGuid` of the long package name (an MD5 of the name, not a random GUID), and nothing records a time, a machine or an address. `System.CoreUObject.Package.Deterministic` saves a fixture twice, saves it again after loading it, and compares the MD5 of the file (with the engine version cleared) against a stored hash on Win64 and on the PS2.

### Versioning

`FileVersionUE` is an `ELeonPackageVersion`. A format change adds a value before `VER_LEON_AUTOMATIC_VERSION_PLUS_ONE`; code that reads data added by it checks `Ar.UEVer() >= VER_LEON_<Change>` (the linker sets the archive's version from the summary). The loader rejects packages older than `VER_LEON_OLDEST_LOADABLE_PACKAGE` or newer than `VER_LEON_LATEST` with an error. A licensee version other than 0 is rejected too.

### Editor-only data (D14)

Desktop builds outside Shipping have `WITH_EDITORONLY_DATA`: they save editor-only properties unless the package has `PKG_FilterEditorOnly` (the cook, P16, sets it). The PS2 and Shipping builds have no editor-only properties: every package they save is marked `PKG_FilterEditorOnly`, and when they load a package without it (an uncooked package) they log it once and skip the editor-only tags as unknown names.

### In memory

`UPackage::SaveToMemory` returns the same bytes `Save` writes; `FLinkerLoad::RegisterInMemoryPackage` makes `LoadPackage` and `FPackageName::DoesPackageExist` use registered bytes instead of a file. The tests use it on every platform (the PS2 platform file is read-only); the tables of a package can be read without loading it with `FLinkerLoad::CreateLinker(nullptr, ...)` (the cooker's dependency walk).

---

## Static mesh — `.lmesh`

Header: `Engine/Source/Runtime/RenderCore/Public/LeonMeshFormat.h`. API: `IsLeonMeshPath`, `LoadLeonMeshFile(Path, FMeshData&)`, `SaveLeonMeshFile(Path, const FMeshData&)`.

The file is written with raw struct writes (little-endian on every supported host):

```text
header (52 bytes, packed):
  char magic[4]        = "LMSH"
  u32  version         = 2          // writers emit 2; the loader also reads 1 and rejects anything else
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

**Space.** Both versions have the same layout; only the space of the data differs:

| Version | Space | On load |
| --- | --- | --- |
| 2 (written since 0.14.0) | the engine world (UE: X forward, Y right, Z up, left-handed, centimetres) | used as stored |
| 1 | legacy: Y up, right-handed, metres | `FLegacyCoordinateConversion::ConvertMeshData`: positions (X, Z, Y) × 100, normals (X, Z, Y), tangents (X, Z, Y, −W) |

UVs and the index order are kept in both, so triangles keep their winding on screen (the swap has determinant −1). The
header AABB is computed from the vertices by the writer and is not read back. A version-1 file and the same source
cooked to version 2 load to the same data: the `Cube.obj` test fixture
(`Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj`) cooks to a file with SHA-256
`EFF1459AE46710C6F1B44C0B1ECB2D739CB590F2492B9DF3EC11A03ECA7757C9`.

Material slot strings carry the source's diffuse texture path per slot (from the OBJ `.mtl`, for example). The loader keeps a slot string only when it contains `.png` or `.jpg` and binds it as the albedo map of that material slot. A `submeshCount` of 0 on load means one section covering all indices.

**Cook:** `FStaticMeshBuilder::CookFromObj` / `CookFromFbx` / `CookFromGltf` (`Engine/Source/Developer/MeshUtilities/Public/StaticMeshBuilder.h`), driven by `LeonCook staticmesh` or a recipe step. glTF / GLB import (vendored cgltf) merges every primitive of the first mesh and, with a materials directory, writes one `M_<Name>.lmat` per material plus copied textures. Each importer's last step is `FImportCoordinateConversion` (`MeshUtilities/Public/ImportCoordinateConversion.h`): OBJ and glTF sources are read as right-handed Y up in metres ((X, Z, Y) × 100, UE's glTF importer); FBX files are resolved by ufbx to right-handed Z up and converted with UE's `FFbxDataConverter` basis (X, −Y, Z) times the file's unit in centimetres (an FBX without declared axes is taken as right-handed Y up). Tangents are computed after the conversion.

**Runtime:** `FLegacyAssetLoader::LoadStaticMesh` accepts `.lmesh` only and logs an error for any other extension; it makes a transient `UStaticMesh` with a `UMaterial` per slot. In a `.llev`, a `StaticMesh` actor stores its mesh and material as content-relative paths.

---

## Material — `.lmat`

Header: `Engine/Source/Runtime/RenderCore/Public/LeonMaterialFormat.h` (the document, the reader and the writer); Engine's `FLegacyAssetLoader::LoadMaterial` makes a `UMaterial` of a file and loads its textures. INI-style text, similar to an Unreal Material Instance's parameters.

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

Section and key names are case-insensitive. `#` and `;` start comments. Unknown keys are reported as `LogLeonMaterial` warnings and ignored. Texture paths are resolved with `FPaths::ResolveLegacyContentPath` (content-relative, not relative to the `.lmat` file: the project content first, then the engine content).

**API:** `LoadLeonMaterialDocument` (parse only, paths kept as strings in `FLeonMaterialDocument`), `SaveLeonMaterialFile`, `MakeDefaultLeonMaterialText`. Runtime: `FLegacyAssetLoader::LoadMaterial` (a transient `UMaterial`, cached by path while it is used); a material the `.llev` reader cannot load is replaced by the default material with a warning.

---

## Level — `.llev`

Binary container (`LLEV`, little-endian, string table + meta + camera + actors + lights). Writers emit version 2; readers accept versions 1 and 2. Actors reference materials by `.lmat` path and meshes by `.lmesh` path; there are no inline materials. Full layout and loading pipeline: **[LEVELS.md](LEVELS.md)**.

---

## Skeletal — FBX import

The cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, `*.blendspace1d.json`) were removed in 0.12.0. Skinned meshes and animation sequences are imported straight from FBX (ufbx) with `LoadSkeletalMeshFromFbx` / `LoadAnimSequenceFromFbx` (`Engine/Source/Developer/MeshUtilities/Public/FbxSkeletalImport.h`). Skeletal assets return as `USkeletalMesh` / `UAnimSequence` `.lasset` packages.

---

## Build descriptors — `.lproj` / `.lplugin`

JSON read by LeonBuildTool in CMake (`Engine/Source/Programs/LeonBuildTool/System/ProjectDescriptor.cmake`, `PluginDescriptor.cmake`), equivalent to Unreal's `.uproject` / `.uplugin`. The runtime does not read them.

`.lproj` (the file name is the project name):

| Field | Used for |
| --- | --- |
| `Modules[].Name` | Project modules added to the project's game targets |
| `Plugins[].Name` / `Plugins[].Enabled` | Enable or disable a plugin for every target of the project (`Enabled` defaults to true) |
| `TargetPlatforms[]` | Recorded (`LEON_PROJECT_TARGET_PLATFORMS`) |
| `FileVersion`, `EngineAssociation`, `Category`, `Description`, `Modules[].Type` / `LoadingPhase` | Informational |

`.lplugin` (the file name is the plugin name; discovered under `Engine/Plugins` and `<Project>/Plugins`):

| Field | Used for |
| --- | --- |
| `EnabledByDefault` | Enabled for game targets unless a project or target disables it (defaults to false). Program targets ignore it and only get plugins enabled by their project or their `Target.cmake` |
| `Modules[].Name` | Modules added to targets that enable the plugin |
| `Modules[].PlatformAllowList[]` | Platforms (or platform groups) the module builds for |
| `FileVersion`, `Version`, `VersionName`, `FriendlyName`, `Description`, `Category`, `Modules[].Type` / `LoadingPhase` | Informational |

Examples: `Game/ThirdPerson/ThirdPerson.lproj`, `Engine/Plugins/Runtime/JoltPhysics/JoltPhysics.lplugin`.

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
| `.lasset` / `.lmap` packages | `Engine/Source/Runtime/CoreUObject` — `UPackage::Save` (`Private/UObject/SavePackage.cpp`), `FLinkerLoad`, `FLinkerSave`, `FPackageFileSummary`, `FObjectImport` / `FObjectExport`, `FPropertyTag`, `FByteBulkData`, `FPackageName` |
| `.lmesh` I/O | `Engine/Source/Runtime/RenderCore` — `LeonMeshFormat`, `FMeshData`, `FVertex` |
| `.lmat` I/O | `Engine/Source/Runtime/RenderCore` — `LeonMaterialFormat`, `FMaterial` (`Public/MaterialShared.h`) |
| Asset classes | `Engine/Source/Runtime/Engine` — `Classes/Engine` (`UTexture`, `UTexture2D`, `UStaticMesh`, `USkeletalMesh`, `USkeletalMeshSocket`, `UDataAsset`), `Classes/Materials`, `Classes/Animation`, `Classes/PhysicsEngine` (`UBodySetup`), `Classes/Sound`, `Classes/Commandlets`; `Public/StaticMeshResources.h`, `Private/AssetBulkData.h`; the plain skeletal data in `AnimationCore`; the GPU copies in the Renderer's private `FRenderResourceCache` |
| Legacy asset loader | `Engine/Source/Runtime/Engine` — `FLegacyAssetLoader` (`Public/LegacyAssetLoader.h`), until P14 part 2 |
| `.llev` I/O and apply | `Engine/Source/Runtime/Engine` — `LeonLevelFormat`, `LevelLoader` |
| Skeletal FBX import | `Engine/Source/Developer/MeshUtilities` — `FbxSkeletalImport` |
| Content paths | `Engine/Source/Runtime/Core` — `FPaths` |
| DCC → `.lmesh` | `Engine/Source/Developer/MeshUtilities` — `FStaticMeshBuilder`, `ObjImport`, `FbxStaticMesh`, `GltfImport` |
| Offline cook | `Engine/Source/Developer/Cooker` + `Engine/Source/Programs/LeonCook` ([TOOLS.md](TOOLS.md)) |
| PS2 drawing, materials, textures | `Engine/Platforms/PS2/Source/Runtime/PS2RHI` — `FPS2RHI`, `FPS2Material`, `FPS2Texture` |
