# Leon asset formats

**Audience:** content authors and tool writers
**Also:** [LEVELS.md](LEVELS.md) (`.llev` levels) · [TOOLS.md](TOOLS.md) (LeonCook and its commandlets) · [SETUP.md](SETUP.md)

Every asset is a UObject saved in a `.lasset` [package](#packages--lasset--lmap) (a map will be a `.lmap`, P15): the runtime loads packages and nothing else (no image, `.wav` or mesh source file). Source files (images, `.wav`, OBJ, FBX, glTF) are [imported](#importing-assets) by the editor module, LeonEd, through LeonCook's commandlets; each imported asset records its source in its `UAssetImportData`, so it can be reimported. The only other runtime files are the `.llev` levels (until P15), the GLSL shaders and the INI config. The PS2 runtime loads no asset file yet (see [PS2](#ps2)).

> Unreal `.uasset` / `.umap` are proprietary. Leon does not read or write them. Interchange with Blender / Unreal goes through FBX or glTF, imported to Leon packages. The `.lasset` layout follows UE 4.27's package structure (summary, name / import / export tables, tagged properties) but is Leon's own binary format.

---

## Extension cheat sheet

| Ext | Kind | Role | Reader / writer |
| --- | --- | --- | --- |
| `.lasset` / `.lmap` | Binary `LEON` | UObject package: an asset / a map (`PKG_ContainsMap`) | `UPackage::Save`, `LoadPackage` / `LoadObject` (CoreUObject), see [Packages](#packages--lasset--lmap) |
| `.llev` | Binary `LLEV` | Level (until the `.lmap` maps of P15) | `LeonLevelFormat` (Engine), see [LEVELS.md](LEVELS.md) |
| `.lproj` / `.lplugin` | JSON | Build descriptors | LeonBuildTool (CMake) |
| `.png`, `.jpg`, `.tga`, `.bmp` | Image | Texture source | `UTextureFactory` (LeonEd, stb_image): import only |
| `.wav` | RIFF / WAVE, PCM16 | Sound source | `USoundFactory` (LeonEd): import only |
| `.obj` / `.fbx` / `.gltf` / `.glb` | DCC source | Mesh and animation source | `UFbxFactory`, `UGLTFImportFactory` (LeonEd, through MeshUtilities): import only |
| `ImportList.ini` | INI text | The imports of a folder of source art | `UImportAssetsCommandlet` (`-importlist=`), see [TOOLS.md](TOOLS.md#importlistini) |
| `.lmat` / `.lmesh` | Legacy | The pre-P14 material and cooked mesh files | read only by `MigrateLegacyContent` (LeonEd, temporary): see [Legacy content](#legacy-content-migration) |

The cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, `*.blendspace1d.json`), `.lm` lightmaps, `.hdr` environment maps and the `leon.game.json` pack marker were removed in 0.12.0; the `.lmesh` / `.lmat` files and runtime PNG / WAV loading in P14. Skeletal assets are `USkeletalMesh` / `UAnimSequence` packages, and static lighting returns as `<Map>_BuiltData.lasset`.

Engine content lives in `Engine/Content` as `/Engine` packages ([below](#engine-content)), the source files of the imported ones in `Engine/SourceArt`, and the GLSL shaders in `Engine/Shaders`.

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
| `UMaterialInterface` (`Materials/MaterialInterface.h`), abstract; `UMaterial` (`Materials/Material.h`) | `ShadingModel` (`MSM_Unlit`, `MSM_DefaultLit`), `BaseColor`, `Specular` (`FLinearColor`, linear RGB), `Metallic`, `Roughness`, `Opacity`, `Shininess`, `UVScale` (`FVector2D`), `bCastsShadows`, `bPlanarMirror`, `BaseColorMap`, `NormalMap` (`UTexture2D*`): the parameters the legacy `.lmat` files had | — |
| `USkeleton` (`Animation/Skeleton.h`) | `Sockets` (`USkeletalMeshSocket` inner objects: `SocketName`, `BoneName`, `RelativeLocation`, `RelativeRotation`, `RelativeScale`) | `FReferenceSkeleton`: the bone names (`FName`s, in the name table), the parent indices and the inverse bind pose (`FMatrix` each) |
| `USkeletalMesh` (`Engine/SkeletalMesh.h`) | `Skeleton`, `Materials` (`FSkeletalMaterial`) | the bounding box, then one bulk payload of the skinned vertices (`FSkeletalVertex`: position, normal, UV, tangent, 4 bone indices, 4 weights) and the indices |
| `UAnimationAsset` → `UAnimSequenceBase` → `UAnimSequence` (`Animation/AnimSequence.h`) | `Skeleton`, `SequenceLength`, `RateScale`, `bLoop`, `NumFrames`, `FrameRate` | one bulk payload of the tracks, one per bone (`FRawAnimSequenceTrack`: one model-space `FMatrix` key per frame) |
| `UBlendSpaceBase` → `UBlendSpace1D` (`Animation/BlendSpace1D.h`) | `Skeleton`, `BlendParameters[3]` (`FBlendParameter`: DisplayName, Min, Max, GridNum), `SampleData` (`FBlendSample`: `Animation`, `SampleValue`, `RateScale`) | — |
| `USoundBase` → `USoundWave` (`Sound/SoundWave.h`) | `Duration`, `NumChannels`, `SampleRate` | `RawPCMData`: the interleaved 16-bit PCM samples as bulk data |
| `UDataAsset` (`Engine/DataAsset.h`), abstract | the game subclass's `UPROPERTY`s | — |
| `UCommandlet` (`Commandlets/Commandlet.h`), abstract, transient | `HelpDescription`, `HelpUsage`, `IsServer`, `IsClient`, `IsEditor`, `LogToConsole`, `ShowErrorCount`; `Main(Params)`, `ParseCommandLine` (never saved: the base of LeonEd's commandlets) | — |
| `UAssetImportData` (`EditorFramework/AssetImportData.h`), editor-only data | `SourceData` (`FAssetImportInfo`: `SourceFiles`, each `FAssetImportSourceFile` `RelativeFilename`, `FileHash`), `ImportSettings` (`TMap<FString, FString>`, sorted) | — |

**Import data.** `UTexture`, `UStaticMesh`, `USkeletalMesh`, `UAnimSequence` and `USoundWave` have an editor-only
`UPROPERTY(Instanced) UAssetImportData* AssetImportData` (UE's), which the factory that imported the asset makes as its
subobject `AssetImportData`: the source file relative to the source root of the asset's mount point (the engine
folder for `/Engine`, the project folder for `/Game`, the parent of the content folder for another mount point; an
absolute path only across drives), its MD5 as 32 hex digits, and the import settings the factory applied. There is
no timestamp, so importing the same file again saves the same bytes (gate G5). The data is inside
`#if WITH_EDITORONLY_DATA` (D14): the cook filters it out and the PS2 and Shipping builds do not have it.

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
no asset registry or primary data assets; the import data keeps a generic settings map where UE has typed
subclasses (`UFbxAssetImportData`, ...).

### Engine content

The engine's assets are `/Engine` packages in `Engine/Content`, migrated from the legacy files in P14:

| Package | Class | Made from |
| --- | --- | --- |
| `/Engine/EngineMaterials/T_Default_D` | `UTexture2D` (128 × 128, sRGB) | imported: `Engine/SourceArt/EngineMaterials/T_Default_D.png` (`Engine/SourceArt/ImportList.ini`) |
| `/Engine/EngineMaterials/M_Default`, `M_WorldGrid`, `M_SolidMetal` | `UMaterial` | converted once from `Materials/*.lmat` (same parameters; the maps are `T_Default_D`); the packages are the source of truth |
| `/Engine/EngineResources/DefaultTexture` | `UTexture2D` (64 × 64 grey checker, sRGB) | saved once from the procedural generator (UE: DefaultTexture) |
| `/Engine/EngineMaterials/T_Default_Bump_N` | `UTexture2D` (256 × 256 bump normal map, linear) | saved once from the procedural generator |
| `/Engine/BasicShapes/Cube`, `Plane`, `Sphere` | `UStaticMesh` (100 cm; the sphere 24 × 16; no material slots) | saved once from `MakeCube` / `MakePlane` / `MakeSphere` (RenderCore) |

A sphere of another tessellation (a `.llev` record may ask for one) is not a package: `GetSphereMesh` builds it at run
time from `MakeSphere`, once per tessellation (`/Temp/BasicShapes/Sphere_<Segments>x<Rings>`, transient).

The config names the defaults, as UE's `BaseEngine.ini` does, and `UEngine` reads them (`UPROPERTY(GlobalConfig)`
`FSoftObjectPath`s); `UEngine::InitializeObjectReferences` loads them with `LoadObject`, and `UMaterial::GetDefaultMaterial`
loads the default material (what a mesh slot without a material draws with) and roots it:

```ini
[/Script/Engine.Engine]
DefaultMaterialName=/Engine/EngineMaterials/M_Default.M_Default
DefaultTextureName=/Engine/EngineResources/DefaultTexture.DefaultTexture
DefaultBumpNormalTextureName=/Engine/EngineMaterials/T_Default_Bump_N.T_Default_Bump_N
UIClickSoundName=
UIConfirmSoundName=
UIBackSoundName=
UIErrorSoundName=
```

<a id="ui-sounds"></a>**UI sounds.** `FAudioDevice::PlayUiSound` (AudioMixer, below Engine) plays the `USoundWave` each
`UI*SoundName` names (`UEngine::Init` hands its samples to `FAudioDevice::SetUiSound`), or the cue's procedural tone
when the key is empty. The engine ships no UI sounds, so the keys are empty: the only candidates, the `UI_*.wav` files
of the project's first commit (`ad7e00e:Projects/Zombies/Content/assets/Audio/UI/`), carry no license, readme, credit
or generator in any commit, so their origin cannot be shown to be CC0 or the project's own. A project sets the keys in
its `DefaultEngine.ini` to sound waves it imported. Sounds play from memory: `UGameplayStatics::PlaySound2D` /
`PlaySoundAtLocation` take a `USoundWave`, whose PCM16 samples the device copies (`FSoundWavePCM`).

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

## Importing assets

LeonEd (`Engine/Source/Editor/LeonEd`, UE's UnrealEd) imports source files into packages; LeonCook runs it from the
command line (`LeonCook [<Project>.lproj] -run=ImportAssets ...`, [TOOLS.md](TOOLS.md)). A factory (`UFactory`) turns
a file into an asset; the import commandlet picks it by the file's extension and names the asset after the file with
UE's prefix for its class:

| Factory (UE name) | Sources | Asset (prefix) |
| --- | --- | --- |
| `UTextureFactory` | PNG, JPEG, TGA, BMP (stb_image) | `UTexture2D` (`T_`): RGBA8, bottom row first; sRGB unless `ColorSpaceMode=Linear` or a `_N` / `_Normal` name |
| `UFbxFactory` | FBX (ufbx), OBJ (tinyobjloader) | `UStaticMesh` (`SM_`), or with `MeshTypeToImport` `USkeletalMesh` (`SK_`, on `Skeleton` or a new `SKEL_` skeleton) and `UAnimSequence` (`A_`, on `Skeleton`) |
| `UGLTFImportFactory` | glTF / GLB (cgltf) | `UStaticMesh` (`SM_`) |
| `USoundFactory` | `.wav`, 16-bit PCM | `USoundWave` (`S_`) |
| `UMaterialFactoryNew` | — (new) | `UMaterial` (`M_`) |
| `ULegacyMaterialFactory`, `ULegacyStaticMeshFactory` | `.lmat`, `.lmesh` (temporary) | `UMaterial`, `UStaticMesh` ([Legacy content](#legacy-content-migration)) |

- **Meshes** are read into mesh data with their material slots and converted to the engine world by MeshUtilities
  (`FStaticMeshBuilder::BuildFromFile`; every importer ends with `FImportCoordinateConversion`: OBJ and glTF are right-handed
  Y up in metres, `(X, Z, Y) × 100` as UE's glTF importer; FBX is resolved by ufbx to right-handed Z up and converted
  with UE's `FFbxDataConverter` basis `(X, −Y, Z)` times the file's unit in centimetres). Each named source material
  gets an `M_<Name>` `UMaterial` next to the mesh (`bImportMaterials`, UE's default) with its values and its maps
  imported as `T_` textures; an existing material of that name is reused as it is.
- **Import over an asset.** Importing onto an existing asset reimports it in place (the factory fills the same object:
  every reference to it stays valid); a mesh slot whose name did not change keeps its material (UE).
- **Reimport.** The import factories are `FReimportHandler`s (`FReimportManager`): an asset whose import data names a
  file one of them imports is read again from it, with the recorded settings. `ImportAssets -reimport -all` reimports
  every asset under the mount points and saves them; gate G5 (`CheckReimport.bat`) checks that this leaves the content
  unchanged.
- **Saving** writes the asset's package (and those of the materials, textures and skeletons the import made) to its
  file under the mount point, deterministically (D13).

**Identity.** The `Cube.obj` test fixture (`Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj`) imported with
`LeonCook Engine/Saved/CookIdentity/CookIdentity.lproj -run=ImportAssets -source=Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj -dest=/Game/Identity`
(a scratch project in the ignored `Engine/Saved`) saves `SM_Cube.lasset` with SHA-256
`2EAE6C7DE3B209D7E77A0257D2E94D996A4013BF8C25F681A1AF1A7976AA149C` (2 692 bytes; the same when imported again over
it or reimported; the engine version, `0.16.0`, is in the package summary: a release changes it).

---

<a id="legacy-content-migration"></a>

## Legacy content (migration)

The pre-P14 files, `.lmat` materials (INI text: `[Info]` `Name` / `ShadingModel`, parameters, `[Textures]` maps) and
`.lmesh` cooked meshes (binary `LMSH`: a 52-byte header, 48-byte `FVertex` records, `uint32` indices, sections and
slot strings), are read only by `LeonCook -run=MigrateLegacyContent -source=<ContentDir>` (temporary), which
converts every legacy file of a folder into the package its content key names:

- the key is the file's path under the folder; it loses its extension, gains the class prefix of its extension unless
  its name has it (`.lmat` `M_`, `.lmesh` `SM_`, images `T_`, `.wav` `S_`), and under `/Engine` the legacy
  `Materials/` and `Textures/` folders are `EngineMaterials/` (`FLegacyAssetKeys::GetMigratedPackageName`);
- the folder's package path is that of the mount point containing it, or a mount point registered for it and named
  after it (`FLegacyAssetKeys::MountContentDirectory`: `.../RenderTest` is `/RenderTest`);
- images and sounds are imported (their import data names the file, which stays as their source), then `.lmesh`
  (version 2 only: a version 1 file, Y up in metres, must be imported again from its source) and `.lmat` files are
  converted without import data: a `.lmat` map is a content key resolved to the texture package (`checker` / `bump`:
  the engine's `DefaultTexture` / `T_Default_Bump_N`), and each `.lmesh` slot gets `M_<Mesh>_Slot<N>` with the
  runtime's default parameters and the slot's diffuse map.

`-engine` also saves the engine's procedural assets ([above](#engine-content)). The engine content was migrated with
`LeonCook -run=ImportAssets -importlist=Engine/SourceArt/ImportList.ini` then
`LeonCook -run=MigrateLegacyContent -engine -source=Engine/Content`, and the `.lmat` files were deleted.

---

## Level — `.llev`

Binary container (`LLEV`, little-endian, string table + meta + camera + actors + lights). Writers emit version 2; readers accept versions 1 and 2. Actors reference their material and mesh by a content key (the old `.lmat` / `.lmesh` path), which resolves to the package the migration made of it (`ResolveLevelAssetObjectPath`, [LEVELS.md](LEVELS.md)); there are no inline materials. Full layout and loading pipeline: **[LEVELS.md](LEVELS.md)**.

---

## Skeletal — FBX import

The cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, `*.blendspace1d.json`) were removed in 0.12.0. Skinned meshes and animation sequences are imported from FBX (ufbx: `LoadSkeletalMeshFromFbx` / `LoadAnimSequenceFromFbx`, `Engine/Source/Developer/MeshUtilities/Public/FbxSkeletalImport.h`) by `UFbxFactory` (`-type=SkeletalMesh` / `-type=Animation`) into `USkeletalMesh`, `USkeleton` and `UAnimSequence` packages.

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

The PS2 runtime (`Engine/Platforms/PS2/Source/Runtime/PS2RHI`) draws with the Graphics Synthesizer directly and loads no `.lasset` package or `.llev` file yet (the cook for the PS2 comes later). The ThirdPerson demo builds its textures, materials and level in code.

### Cooked mesh blob — `LPS2`

`FPS2RHI::DrawCookedMesh(Data, Size)` validates this header:

| Field | Type | Notes |
| --- | --- | --- |
| `magic` | `char[4]` | `LPS2` |
| `version` | `u32` | `1` |
| `vertexCount` | `u32` | Must be > 0 |
| `indexCount` | `u32` | |

Vertex upload is not implemented yet: a valid blob draws a placeholder triangle. No tool produces `LPS2` blobs; the cook has no PS2 target platform yet.

### Materials and textures

`FPS2Material` (`PS2RHI/Public/PS2RHITypes.h`) mirrors a subset of `UMaterial`:

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
| Mesh data, material values | `Engine/Source/Runtime/RenderCore` — `FMeshData`, `FVertex`, `FMaterial` (`Public/MaterialShared.h`) |
| Asset classes | `Engine/Source/Runtime/Engine` — `Classes/Engine` (`UTexture`, `UTexture2D`, `UStaticMesh`, `USkeletalMesh`, `USkeletalMeshSocket`, `UDataAsset`), `Classes/Materials`, `Classes/Animation`, `Classes/PhysicsEngine` (`UBodySetup`), `Classes/Sound`, `Classes/Commandlets`, `Classes/EditorFramework` (`UAssetImportData`); `Public/StaticMeshResources.h`, `Private/AssetBulkData.h`; the plain skeletal data in `AnimationCore`; the GPU copies in the Renderer's private `FRenderResourceCache` |
| `.llev` I/O and apply, content keys | `Engine/Source/Runtime/Engine` — `LeonLevelFormat`, `LevelLoader`, `FLegacyAssetKeys` (`Public/Level/LegacyAssetKeys.h`), until P15 |
| Skeletal FBX import | `Engine/Source/Developer/MeshUtilities` — `FbxSkeletalImport` |
| Content paths | `Engine/Source/Runtime/Core` — `FPaths` |
| DCC → mesh data | `Engine/Source/Developer/MeshUtilities` — `FStaticMeshBuilder`, `ObjImport`, `FbxStaticMesh`, `GltfImport` |
| Factories, reimport, commandlets | `Engine/Source/Editor/LeonEd` + `Engine/Source/Programs/LeonCook` ([TOOLS.md](TOOLS.md)) |
| PS2 drawing, materials, textures | `Engine/Platforms/PS2/Source/Runtime/PS2RHI` — `FPS2RHI`, `FPS2Material`, `FPS2Texture` |
