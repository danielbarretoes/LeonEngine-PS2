# Leon asset formats

**Audience:** content authors and tool writers
**Also:** [LEVELS.md](LEVELS.md) (`.lmap` maps, the glTF map import) · [TOOLS.md](TOOLS.md) (LeonCook and its commandlets) · [SETUP.md](SETUP.md)

Every asset is a UObject saved in a `.lasset` [package](#packages--lasset--lmap), and every map a world saved in a `.lmap` package ([Maps](#maps--lmap)): the runtime loads packages and nothing else (no image, `.wav`, mesh or scene source file). Source files (images, `.wav`, glTF) are [imported](#importing-assets) by the editor module, LeonEd, through LeonCook's commandlets; each imported asset (and each imported map) records its source in its `UAssetImportData`, so it can be reimported. The only other runtime files are the GLSL shaders (`Engine/Shaders`) and the INI config. Since P16 the cook saves the packages a game needs without their editor-only data ([Cooked packages](#cooked-packages)), and a staged build reads them, with its config and shaders, from one [`.lpak`](#paks--lpak) file. The PS2 runtime loads the same packages, cooked for it (paletted textures), loose or from a pak (see [PS2](#ps2)).

> Unreal `.uasset` / `.umap` are proprietary. Leon does not read or write them. Interchange with Blender / Unreal goes through glTF, the only mesh, skeletal mesh and animation format since [ps2-shipping](PLANS/ps2-shipping.md) N21 (D11: FBX and OBJ were removed), imported to Leon packages. The `.lasset` layout follows UE 4.27's package structure (summary, name / import / export tables, tagged properties) but is Leon's own binary format.

---

## Extension cheat sheet

| Ext | Kind | Role | Reader / writer |
| --- | --- | --- | --- |
| `.lasset` / `.lmap` | Binary `LEON` | UObject package: an asset / a map (`PKG_ContainsMap`) | `UPackage::Save`, `LoadPackage` / `LoadObject` (CoreUObject), see [Packages](#packages--lasset--lmap) |
| `.lpak` | Binary, footer `LPAK` | A staged build's content, config and shaders in one file, mounted as a platform file | `FPakWriter` / LeonPak (writing), `FPakFile` / `FPakPlatformFile` (PakFile module), see [Paks](#paks--lpak) |
| `.lproj` / `.lplugin` | JSON | Build descriptors | LeonBuildTool (CMake) |
| `.png`, `.jpg`, `.tga`, `.bmp` | Image | Texture source | `UTextureFactory` (LeonEd, stb_image): import only |
| `.wav` | RIFF / WAVE, PCM16 | Sound source | `USoundFactory` (LeonEd): import only |
| `.hdr` | Radiance RGBE | A long-lat HDR panorama: a cube map's source ([cube maps](#cube-maps)) | `UTextureCubeFactory` (LeonEd, stb_image): import only |
| `.gltf` / `.glb` | DCC source | Static mesh, skeletal mesh and animation source; a glTF scene is also a map's source | `UGLTFImportFactory`, `UGLTFMapFactory` (LeonEd, through MeshUtilities): import only |
| `ImportList.ini` | INI text | The imports of a folder of source art | `UImportAssetsCommandlet` (`-importlist=`), see [TOOLS.md](TOOLS.md#importlistini) |

The cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, `*.blendspace1d.json`), `.lm` lightmaps, the cooked `.hdr` environment maps (a Radiance file is a cube map's source again since ps2-polish P8) and the `leon.game.json` pack marker were removed in 0.12.0; the `.lmesh` / `.lmat` files and runtime PNG / WAV loading in P14; the `.llev` levels, their reader and the legacy content tools in P15 ([Legacy content](#legacy-content-migration)). Skeletal assets are `USkeletalMesh` / `UAnimSequence` packages, maps are `.lmap` packages, and static lighting is baked per vertex into the map's static mesh components ([instance colours](#lps2-instance-colors), N22).

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
| `UTexture2D` (`Engine/Texture2D.h`) | `AddressX`, `AddressY` (`ETextureAddress`: `Wrap` by default, `Clamp`; ps2-polish P8) | `FTexturePlatformData`: `int32` SizeX, SizeY, `uint8` `EPixelFormat` (UE values: `PF_R8G8B8A8` = 37, `PF_B8G8R8A8` = 2; Leon's paletted formats of the PS2 cook: `PF_P8` = 200, 256 RGBA8 palette entries then an index a texel, and `PF_P4` = 201, 16 entries then two texels a byte, the first in the low nibble), `int32` mip count, then per mip `int32` SizeX, SizeY and its data as bulk data, bottom row first: mip 0 `GetPixelFormatDataSize` bytes, a later mip `GetPixelFormatMipDataSize` (a paletted mip is its indices only, through mip 0's palette). An imported texture has mip 0 only; the PS2 cook's paletted ones carry their mip chain ([PS2](#ps2)) |
| `UTextureCube` (`Engine/TextureCube.h`) | `Faces` (six `UTexture2D` subobjects `PosX` ... `NegZ`, in `ECubeFace` order), `HorizonColor`, `SRGB`, `AssetImportData` | — ([cube maps](#cube-maps)) |
| `UFont` (`Engine/Font.h`) | `Textures` (the glyph pages: `UTexture2D` subobjects `Texture0` ..., PF_P4), `Ascent`, `Descent`, `Leading` (pixels), `Kerning` (0), `LegacyFontSize` (the pixel height), `LegacyFontName` (the TrueType family), `AssetImportData` | the characters (`int32` count, then each `FFontCharacter`: `int32` StartU, StartV, USize, VSize, `uint8` TextureIndex, `int32` VerticalOffset, HorizontalOffset, Advance; indexed by code point), then the kerning pairs (`int32` count, then each `uint32` pair, first code point in the high 16 bits, and `int32` pixels): [fonts](#fonts) |
| `UStaticMesh` (`Engine/StaticMesh.h`) | `StaticMaterials` (`FStaticMaterial`: `MaterialInterface`, `MaterialSlotName`), `BodySetup` (an inner object), `SourceModels` ([LODs](#static-mesh-lods): `FStaticMeshSourceModel`, `ReductionSettings.PercentTriangles` and `ScreenSize`; empty for one LOD) | the local bounding box (`FBox`, of the source's positions), then one bulk payload: LOD 0's render data (`FStaticMeshLODResources`), an [LPS2 v2](#lps2-v2) blob as its `int32` size and its bytes, then the collision triangles (`FTriMeshCollisionData`: the source's positions as an array of `FVector`, its `uint32` indices, three a triangle, and each triangle's material slot as an array of `uint16`, `MaterialIndices`, [ps2-shipping](PLANS/ps2-shipping.md) N30f), then each later LOD's blob (as many as `SourceModels` has entries after the first) |
| `UBodySetup` (`PhysicsEngine/BodySetup.h`) | `AggGeom` (`FKAggregateGeom`: `BoxElems`, each `FKBoxElem` Center, Rotation, X, Y, Z in cm), `CollisionTraceFlag` (`ECollisionTraceFlag`) | — |
| `UMaterialInterface` (`Materials/MaterialInterface.h`), abstract; `UMaterial` (`Materials/Material.h`) | `ShadingModel` (`MSM_Unlit`, `MSM_DefaultLit`), `BaseColor` (`FLinearColor`, linear RGB), `Opacity`, `UVScale` (`FVector2D`), `BaseColorMap` (`UTexture2D*`), `bMipmaps` (true: the map's mips, trilinear), `LodBias` (levels added to its LOD): what the GS scene renderer draws with (`FMaterial`); `PhysMaterial` (`UPhysicalMaterial*`, [physical materials](#physical-materials)) | — |
| `UPhysicalMaterial` (PhysicsCore, `PhysicalMaterials/PhysicalMaterial.h`, `PM_`) | `SurfaceType` (`EPhysicalSurface`, saved by its enumerator's name: `SurfaceType_Default`, `SurfaceType1` ... `SurfaceType62`) | — |
| `USkeleton` (`Animation/Skeleton.h`) | `Sockets` (`USkeletalMeshSocket` inner objects: `SocketName`, `BoneName`, `RelativeLocation`, `RelativeRotation`, `RelativeScale`) | `FReferenceSkeleton`: the bone names (`FName`s, in the name table), the parent indices (a parent before its children), the reference pose (each bone's local `FTransform`) and the inverse bind pose (`FMatrix` each) |
| `USkeletalMesh` (`Engine/SkeletalMesh.h`) | `Skeleton`, `Materials` (`FSkeletalMaterial`: a section draws with the slot its render data names) | the bind pose's bounding box, then one bulk payload: the render data, a [skinned LPS2 v2](#skinned-meshes) blob (`int32` size and its bytes), and each bone's bounds radius (`TArray<float>`: the farthest vertex it moves, from its origin; the pose's bounds are spheres of these around the posed bones) |
| `UAnimationAsset` → `UAnimSequenceBase` → `UAnimSequence` (`Animation/AnimSequence.h`) | `Skeleton`, `SequenceLength`, `RateScale`, `bLoop`, `Notifies` (`FAnimNotifyEvent`: `TriggerTime`, `NotifyName`, `Notify`: [notifies](#animation-notifies)), `NumFrames`, `FrameRate` | one bulk payload: the [compressed keys](#animation-keys) (`FCompressedAnimSequence`: one local-space track per bone) |
| `UAnimSequenceBase` → `UAnimMontage` (`Animation/AnimMontage.h`, `AM_`) | `SequenceLength`, `Notifies` (its own), `SlotName`, `Animation`, `BlendInTime`, `BlendOutTime`, `CompositeSections` (`FCompositeSection`: `SectionName`, `StartTime`, `NextSectionName`) | — |
| `UBlendSpaceBase` → `UBlendSpace1D` (`Animation/BlendSpace1D.h`), → `UAimOffsetBlendSpace1D` (`Animation/AimOffsetBlendSpace1D.h`, `AO_`), `UBlendSpace` (2D, `Animation/BlendSpace.h`) (`BS_`) | `Skeleton`, `BlendParameters[3]` (`FBlendParameter`: DisplayName, Min, Max, GridNum), `SampleData` (`FBlendSample`: `Animation`, `SampleValue`, `RateScale`); an aim offset's `BasePose` | — (a 2D space's triangles are rebuilt from the samples when it loads) |
| `UAnimNotify` (`Animation/AnimNotify.h`) | a game's own properties | — (an object on a timeline that handles its event: [notifies](#animation-notifies)) |
| `USoundBase` → `USoundWave` (`Sound/SoundWave.h`) | `Duration`, `Priority` (1 by default), `bLooping`; editor-only: `NumChannels`, `SampleRate` (the source's), `CompressionSampleRate` (22 050 by default), `LoopStartFrame` | uncooked: `RawPCMData`, the source's interleaved 16-bit PCM samples as bulk data; cooked (`PKG_FilterEditorOnly`): the [SPU2 ADPCM](#sound-waves) (`FName` format `SPU2ADPCM`, `int32` rate, `int32` loop start frame or −1, the blocks as bulk data) |
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
`ReleaseResources`), through `IRendererModule::ReleaseAssetResources` ([ARCHITECTURE.md §12](ARCHITECTURE.md#12-rendering-the-gs-path)).

**Deviations from UE 4.27.** No texture source, compression, LOD groups or streaming; static mesh LODs from their
source models' reduction only (no LOD groups, no imported LODs, `SourceModels` kept at run time for the screen sizes),
one skeletal mesh LOD, no mesh description or nanite; materials are fixed parameters, not an expression graph compiled to shaders; the
animation keys are compressed local position, rotation and scale keys as UE's, in Leon's own format (one codec, below)
and the reference skeleton keeps the inverse bind pose (UE: the mesh); a sound keeps its PCM16 source where UE keeps the `.wav` (RawData), and cooks to
one format for every platform, the SPU2's ADPCM, where UE cooks a format per platform;
no asset registry or primary data assets; the import data keeps a generic settings map where UE has typed
subclasses (`UFbxAssetImportData`, ...).

### Engine content

The engine's assets are `/Engine` packages in `Engine/Content`, migrated from the legacy files in P14:

| Package | Class | Made from |
| --- | --- | --- |
| `/Engine/EngineMaterials/T_Default_D` | `UTexture2D` (128 × 128, sRGB) | imported: `Engine/SourceArt/EngineMaterials/T_Default_D.png` (`Engine/SourceArt/ImportList.ini`) |
| `/Engine/EngineMaterials/M_Default`, `M_WorldGrid` | `UMaterial` | converted once from `Materials/*.lmat` (their colour, UV scale and map, `T_Default_D`); the packages are the source of truth |
| `/Engine/EngineResources/DefaultTexture` | `UTexture2D` (64 × 64 grey checker, sRGB) | saved once from the procedural generator (UE: DefaultTexture) |
| `/Engine/EngineFonts/DejaVuSansCondensed10`, `14`, `20`, `32` | `UFont` (one PF_P4 page each: 256 × 64, 128 × 128, 256 × 128, 256 × 256) | imported: `Engine/SourceArt/EngineFonts/DejaVuSansCondensed.ttf` at 10, 14, 20 and 32 pixels (`Engine/SourceArt/ImportList.ini`, [fonts](#fonts)) |
| `/Engine/BasicShapes/Cube`, `Plane`, `Sphere` | `UStaticMesh` (100 cm; the sphere 24 × 16; no material slots) | saved once from `MakeCube` / `MakePlane` / `MakeSphere` (RenderCore) |
| `/Engine/Maps/Entry`, `/Engine/Maps/Template_Default` | map (`.lmap`) | migrated once (P15) from the legacy `Blank.llev` and `Starter.llev` templates ([LEVELS.md](LEVELS.md#engine-maps)); the packages are the source of truth |
| `/Engine/Maps/AxisTest` (with its `Meshes/SM_*` and `Materials/M_*`) | map (`.lmap`) | imported: `Engine/SourceArt/Maps/AxisTest.glb`, written by `MakeAxisTest.py` (`Engine/SourceArt/ImportList.ini`) |

The config names the defaults, as UE's `BaseEngine.ini` does, and `UEngine` reads them (`UPROPERTY(GlobalConfig)`
`FSoftObjectPath`s); `UEngine::InitializeObjectReferences` loads them with `LoadObject`, and `UMaterial::GetDefaultMaterial`
loads the default material (what a mesh slot without a material draws with) and roots it:

```ini
[/Script/Engine.Engine]
DefaultMaterialName=/Engine/EngineMaterials/M_Default.M_Default
DefaultTextureName=/Engine/EngineResources/DefaultTexture.DefaultTexture
TinyFontName=/Engine/EngineFonts/DejaVuSansCondensed10.DejaVuSansCondensed10
SmallFontName=/Engine/EngineFonts/DejaVuSansCondensed14.DejaVuSansCondensed14
MediumFontName=/Engine/EngineFonts/DejaVuSansCondensed20.DejaVuSansCondensed20
LargeFontName=/Engine/EngineFonts/DejaVuSansCondensed32.DejaVuSansCondensed32
UIClickSoundName=
UIConfirmSoundName=
UIBackSoundName=
UIErrorSoundName=
```

<a id="ui-sounds"></a>**UI sounds.** `FAudioDevice::PlayUiSound` (AudioMixer, below Engine) plays the `USoundWave` each
`UI*SoundName` names (`UEngine::Init` hands its buffer to `FAudioDevice::SetUiSound`); a cue whose key is empty is
silent (the procedural tones went with the PCM path in [ps2-shipping](PLANS/ps2-shipping.md) N19). The engine ships no UI sounds, so the keys are empty: the only candidates, the `UI_*.wav` files
of the project's first commit (`ad7e00e:Projects/Zombies/Content/assets/Audio/UI/`), carry no license, readme, credit
or generator in any commit, so their origin cannot be shown to be CC0 or the project's own. A project sets the keys in
its `DefaultEngine.ini` to sound waves it imported. Sounds play from the audio device's buffers:
`UGameplayStatics::PlaySound2D` / `PlaySoundAtLocation` take a `USoundWave`, whose SPU2 ADPCM the device keeps resident
from the sound's load ([ARCHITECTURE.md](ARCHITECTURE.md#audio)).

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
  int32   FileVersionUE          ELeonPackageVersion (Core UObject/ObjectVersion.h); 6 in 0.25.0 (Versioning, below)
  int32   FileVersionLicenseeUE  0
  int32   TotalHeaderSize        summary + tables: where the export data starts
  uint32  PackageFlags           PKG_Cooked 0x200, PKG_ContainsMap 0x20000, PKG_FilterEditorOnly 0x80000000, ...
  int32   NameCount, NameOffset
  int32   ExportCount, ExportOffset
  int32   ImportCount, ImportOffset
  int32   SoftPackageReferencesCount, SoftPackageReferencesOffset
  FGuid   Guid                   4 x uint32: FGuid::NewDeterministicGuid(long package name) (MD5)
  FEngineVersion SavedByEngineVersion
          uint16 Major, Minor, Patch; uint32 Changelist; FString Branch   ("0.25.0-0+LeonEngine")
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

| Version | Value | Change |
| --- | --- | --- |
| `VER_LEON_INITIAL_PACKAGE_FORMAT` | 1 | the first package layout (P11) |
| `VER_LEON_REMOVE_VERTEX_TANGENT` | 2 | the mesh vertices lost their tangent ([ps2-shipping](PLANS/ps2-shipping.md) N4) |
| `VER_LEON_LPS2_MESH` | 3 | a static mesh's render data is [LPS2 v2](#lps2-v2) and its collision triangles are saved beside it ([ps2-shipping](PLANS/ps2-shipping.md) N12) |
| `VER_LEON_SKELETAL_LPS2_ANIM_TRACKS` | 4 | the glTF skeletal import ([ps2-shipping](PLANS/ps2-shipping.md) N21): a skeleton keeps its reference pose, a skeletal mesh's render data is [skinned LPS2 v2](#skinned-meshes) with its bones' bounds, an animation's keys are [compressed local tracks](#animation-keys); the oldest loadable version: the content was saved again (it had no skeletal assets), and an older package fails to load |
| `VER_LEON_BAKED_VERTEX_COLORS` | 5 | a static mesh component saves its [baked vertex colours](#lps2-instance-colors) after its transform ([ps2-shipping](PLANS/ps2-shipping.md) N22); the content was saved again |
| `VER_LEON_COLLISION_MATERIAL_INDICES` | 6 | a static mesh's collision triangles carry each one's material slot (`FTriMeshCollisionData::MaterialIndices`, after the indices), so a hit on a triangle has its material's [physical material](#physical-materials) ([ps2-shipping](PLANS/ps2-shipping.md) N30f); the oldest loadable version: the content was saved again (`ResavePackages`, reading the older payloads with the slots at 0, then every mesh reimported), and an older package fails to load |

### Editor-only data (D14)

Desktop builds outside Shipping have `WITH_EDITORONLY_DATA`: they save editor-only properties unless the package has `PKG_FilterEditorOnly` (the cook sets it). The PS2 and Shipping builds have no editor-only properties: every package they save is marked `PKG_FilterEditorOnly`, and when they load a package without it (an uncooked package) they log it once and skip the editor-only tags as unknown names.

A filtered package also leaves out the editor-only objects (P16, UE's `IsEditorOnlyObject`): an object whose `UObject::IsEditorOnly` is true, or one inside such an object, is not exported, and every reference to it is saved as null. `UAssetImportData` is editor-only, so a cooked asset or map has no import data, and its `AssetImportData` property (itself editor-only) is not saved either.

### In memory

`UPackage::SaveToMemory` returns the same bytes `Save` writes; `FLinkerLoad::RegisterInMemoryPackage` makes `LoadPackage` and `FPackageName::DoesPackageExist` use registered bytes instead of a file. The tests use it on every platform (the PS2 platform file is read-only); the tables of a package can be read without loading it with `FLinkerLoad::CreateLinker(nullptr, ...)` (the cooker's dependency walk).

<a id="maps--lmap"></a>

### Maps — `.lmap`

A map is a package like any other, saved with a `.lmap` file name, which sets `PKG_ContainsMap` (UE's `.umap`). It
holds the world and everything in it, each an export:

| Export | Class | What it saves |
| --- | --- | --- |
| `<Map>` | `UWorld` (`Engine/World.h`), public and standalone: the map's asset | `PersistentLevel`; editor-only `AssetImportData` for an imported map |
| `<Map>:PersistentLevel` | `ULevel` (`Engine/Level.h`) | `Actors` (the spawn order; the world settings first), `WorldSettings` |
| `<Map>:PersistentLevel.<Actor>` | `AWorldSettings`, `AStaticMeshActor`, `APlayerStart`, `ATargetPoint`, `ABlockingVolume`, `ATriggerVolume`, `APainCausingVolume`, `ADirectionalLight`, `APointLight`, `ACameraActor`, `ANavigationWaypoint`, `AVisibilityCellVolume`, `AVisibilityPortal`, ... | the actor's `UPROPERTY`s: `Tags`, `bHidden`, `RootComponent`, the class's own (`DefaultGameMode`, `KillZ`, `FogSettings`, `PlayerStartTag`, `DamagePerSec`, `Links`, `Flags`, `CellName`, `CellA` / `CellB` / `Corners`, ...) |
| `<Map>:PersistentLevel.<Actor>.<Component>` | the actor's default subobjects and the components added to it (`URotatingMovementComponent`, ...) | the transform (`RelativeLocation`, `RelativeRotation`, `RelativeScale3D`), `Mobility`, the collision (`CollisionEnabled`, `bSimulatePhysics`, `bEnableGravity`), `StaticMesh`, `OverrideMaterials`, the light and camera values, ...; a scene component's native tail is the `FQuat` of its relative transform, so a loaded transform is the saved one bit for bit (Leon; UE rebuilds it from the rotator); a static mesh component's then carries its [baked vertex colours](#lps2-instance-colors) (N22) |

<a id="cells-and-portals"></a>**Cells and portals** ([ps2-shipping](PLANS/ps2-shipping.md) N15). A map's visibility
cells and the portals between them are actors saved with it, from the glTF's `VIS_<Cell>` and `PORTAL_<CellA>_<CellB>`
nodes (the engine's rules in `BaseEditor.ini`): an `AVisibilityCellVolume` is the box of its node's mesh with
`CellName` the node's suffix; an `AVisibilityPortal` keeps `CellA`, `CellB` (split from the suffix where both sides
are cells of the map, so a cell's name may hold an underscore) and `Corners`, the rectangle of its node's quad in the
plane of its first triangle, in the world, in order around it. A portal whose suffix names no two cells is left out
with a warning. Nothing about them is cooked apart: the renderer's scene gathers the actors when the map loads and
assigns its primitives to the cells then ([ARCHITECTURE.md §12](ARCHITECTURE.md#12-rendering-the-gs-path)).

The meshes, materials and textures a map shows are imports of their own packages (an imported map's in
`<Map>/Meshes` and `<Map>/Materials`), and the class references (`DefaultGameMode`) imports of `/Script` classes.
Transient actors (the game mode, the players' controllers and pawns) are never saved. `UEngine::LoadMap` loads the
package, finds the world (`UWorld::FindWorldInPackage`), initializes it (`InitWorld`), and registers and initializes
its actors (`InitializeActorsForPlay`): [LEVELS.md](LEVELS.md).

<a id="cooked-packages"></a>

### Cooked packages

The cook (`LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64|PS2`, [TOOLS.md](TOOLS.md#the-cook)) loads each
package the game needs and saves it again with `PKG_FilterEditorOnly | PKG_Cooked` into
`<Project>/Saved/Cooked/<Platform>/`: the same layout as any package, without the editor-only properties and objects
(the import data), and with the target platform's name in the summary's `CookedPlatform` (`Win64`, `PS2`). A `/Engine`
package goes to `Engine/Content/`, a `/Game` package to `<Project>/Content/`, keeping its path and extension (UE's
cooked layout); beside them the cook stages the config (`Engine/Config/Base*.ini`, the platform's layers, the
project's `Config/Default*.ini`, never an Editor ini), the shaders (`Engine/Shaders/`) and the `.lproj`. The PS2 target
cooks its textures paletted ([PS2](#ps2)); the rest keeps the Win64 formats. Two cooks of the same content give the same bytes.

What the target never reads stays out ([ps2-shipping](PLANS/ps2-shipping.md) N23): the staged ini files lose their
comments, their blank lines and the sections only the editor and the cook read (`[/Script/UnrealEd.*]`, the
packaging settings, and `[/Script/LeonEd.*]`, the cook's budgets; `UCookCommandlet::StripConfigForTarget`), and a
static mesh whose body setup answers with its simple shapes everywhere (`CTF_UseSimpleAsComplex`) is saved without its
collision triangles, which the physics scene would never read. ShooterGame's PS2 cook went from 245 409 to 232 480
bytes (−12.9 KB: 12.1 KB of config, 0.9 KB of the blockout crates' triangles).

**The cook cache** (N23). The cook is incremental by default (`-iterate`; `-full` cooks everything): each cooked
package is kept in `<Project>/Intermediate/CookCache/<Platform>/` under a key, the SHA-1 of the package's bytes and of
everything its hard imports reach, the cooker's version (`UCookCommandlet::CookerVersion`, raised by a change that
alters the output) and the platform's settings. A package whose key has not changed is copied from the cache, byte for
byte what a full cook makes ([TOOLS.md](TOOLS.md#the-cook)). A map's baked lighting is in the map and the meshes it was
baked for are its imports, so changing a light or a mesh cooks the map again.

<a id="paks--lpak"></a>

## Paks — `.lpak`

A pak holds files under one folder, its mount point, as UE's `.pak` does (PakFile module, `IPlatformFilePak.h`; plan
decision D9). LeonPak writes it ([TOOLS.md](TOOLS.md#leonpak)), and at run time `FPakPlatformFile`, a platform file in
the `IPlatformFile` chain, serves its files to everything that opens a file ([ARCHITECTURE.md](ARCHITECTURE.md#13-content-and-paths)).
No compression, no encryption; the bytes are little-endian and written the way `FArchive` writes them (an `FString` is
its `int32` length including the terminator, then that many UTF-8 bytes).

```text
entry data     each file's bytes, raw, one after the other: first in the open order LeonPak -order= gives (the
               order a -LogFileOpenOrder run opened them in), then the others in path order (lowercased); with
               LeonPak -align=N each starts at a multiple of N (zeros in between): 2048 puts every file on a CD sector
index          at IndexOffset, IndexSize bytes:
  FString  MountPoint      the folder every entry is under, ending in '/': "../../../" for a staged build
  int32    NumEntries
  NumEntries x, sorted by PathHash, then by the lowercased Filename:
    uint32   PathHash       FCrc::MemCrc32 of Filename, lowercased, with '/'
    FString  Filename       relative to the mount point: "Engine/Content/Maps/Entry.lmap"
    int64    Offset         where the bytes start, from the start of the file
    int64    Size
    uint8    Hash[20]       the SHA-1 of the bytes (FSHA1)
FPakInfo       the last 44 bytes of the file:
  uint32   Magic          0x4B41504C: the bytes "LPAK"
  int32    Version        1 (FPakInfo::PakFile_Version_Initial, 0.17.0)
  int64    IndexOffset    right after the last entry's data
  int64    IndexSize
  uint8    IndexHash[20]  the SHA-1 of the index
```

- **Lookup.** A path is looked up by the binary search of its hash in the index, then compared (ignoring case, as UE
  does) with the entries of that hash, so a hash collision costs a string compare. `FPakFile` checks the footer, the
  index's size and SHA-1, the order of the index and every entry's range when it opens a pak; `FPakFile::Check`
  (`LeonPak -test`) reads every entry and compares its SHA-1.
- **Mount point.** LeonPak takes the mount point from the paths it is given: the longest folder they all start with
  (UnrealPak's rule). A relative mount point is taken from the executable's folder, so `../../../` is the folder above
  `<Project>/Binaries/<Platform>/`: the staged build's root, where `Engine/` and `<Project>/` are. Mounting can place a
  pak elsewhere (`FPakPlatformFile::Mount(File, Order, Path)`).
- **Determinism.** The output depends only on the files and the open order: the data is in that order (then path
  order) and the index in hash order, whatever order the response file lists them in, and nothing records a time. Two
  paks of the same cooked folder and order are the same bytes.
- **Open order** ([ps2-shipping](PLANS/ps2-shipping.md) N23). `-LogFileOpenOrder` on the game (Win64 and PS2) puts
  `FPlatformFileOpenLog` on top of the platform file chain: each file the game opens, the first time, by its staged path
  (`"ShooterGame/Content/Maps/de_leon.lmap" 12`), in the log (`LogFileOpenOrder:`) and at exit in
  `<Project>/Saved/Logs/FileOpenOrder-<Platform>.txt` (not on the PS2, which writes no file: its EE log is the order).
  `LeonPak -order=<file>` (`BuildCookRun -pakorder=`) reads either and puts those entries first, in that order, so a
  load reads the disc forward; the index keeps its format.
- **Differences from UE's `.pak`**: no per-entry header before the data, no compression blocks or encryption, no
  signature file, the path hash only in the index (UE 4.27 splits a path-hash index from a full directory index).

---

## Importing assets

LeonEd (`Engine/Source/Editor/LeonEd`, UE's UnrealEd) imports source files into packages; LeonCook runs it from the
command line (`LeonCook [<Project>.lproj] -run=ImportAssets ...`, [TOOLS.md](TOOLS.md)). A factory (`UFactory`) turns
a file into an asset; the import commandlet picks it by the file's extension and names the asset after the file with
UE's prefix for its class:

| Factory (UE name) | Sources | Asset (prefix) |
| --- | --- | --- |
| `UTextureFactory` | PNG, JPEG, TGA, BMP (stb_image) | `UTexture2D` (`T_`): RGBA8, bottom row first; sRGB unless `ColorSpaceMode=Linear` or a `_N` / `_Normal` name |
| `UGLTFImportFactory` | glTF / GLB (cgltf) | `UStaticMesh` (`SM_`); with `ImportType=SkeletalMesh` (`-type=SkeletalMesh`) a `USkeletalMesh` (`SK_`) on `Skeleton` or a `SKEL_` skeleton next to it; with `ImportType=Animation` (`-type=Animation`) a `UAnimSequence` (`A_<Animation>`) per glTF animation on `Skeleton` ([below](#skeletal-meshes-and-animations--gltf-import)) |
| `UGLTFMapFactory` (`-type=Map`) | glTF / GLB scene (cgltf) | a map (`UWorld`, `.lmap`, no prefix) with its `SM_` meshes and `M_` materials: [LEVELS.md](LEVELS.md#importing-a-map-from-gltf) |
| `UTextureCubeFactory` (`-type=TextureCube`) | Radiance `.hdr`, long-lat (stb_image) | `UTextureCube` (`T_`): six tone-mapped sRGB faces of `CubeFaceSize` ([cube maps](#cube-maps)) |
| `UTrueTypeFontFactory` (`-type=Font`) | `.ttf` (stb_truetype) | `UFont` (no prefix, as UE's engine fonts): the glyph pages and metrics at `Height` pixels ([fonts](#fonts)) |
| `USoundFactory` | `.wav`, 16-bit PCM | `USoundWave` (`S_`): the samples as they are, with `CompressionSampleRate`, `bLooping`, `LoopStartFrame` and `Priority` (import settings) |
| `UMaterialFactoryNew` | — (new) | `UMaterial` (`M_`) |
| `UPhysicalMaterialFactoryNew` | — (new: an ImportList `Type=PhysicalMaterial` section, `SurfaceType=`) | `UPhysicalMaterial` (`PM_`) |

- **Meshes** are read into mesh data with their material slots and converted to the engine world by MeshUtilities
  (`FStaticMeshBuilder::BuildFromFile`; the importer ends with `FImportCoordinateConversion`: glTF is right-handed Y up
  in metres, `(X, Z, Y) × 100` as UE's glTF importer). Each named source material gets an `M_<Name>` `UMaterial` next
  to the mesh (`bImportMaterials`, UE's default) with its values and its base colour image as a `T_` texture: an
  external file is imported as its own asset (with its import data), an image embedded in a `.glb` (a buffer view) or
  a `data:` URI becomes `T_<ImageName>` made from its bytes, without import data (the mesh's import makes it); an
  existing material of that name is reused as it is, but for its physical material: the glTF material's extras
  `{"physMaterial": "<a PM_'s long package name or object path>"}` set `PhysMaterial` on the material the import
  makes or finds again ([physical materials](#physical-materials)). glTF is the only mesh format: FBX (ufbx) and OBJ (tinyobjloader)
  were removed in [ps2-shipping](PLANS/ps2-shipping.md) N21, OBJ having no user left but the tests.
- **Import over an asset.** Importing onto an existing asset reimports it in place (the factory fills the same object:
  every reference to it stays valid); a mesh slot whose name did not change keeps its material (UE).
- **Reimport.** The import factories are `FReimportHandler`s (`FReimportManager`): an asset whose import data names a
  file one of them imports is read again from it, with the recorded settings. `ImportAssets -reimport -all` reimports
  every asset under the mount points and saves them; gate G5 (`CheckReimport.bat`) checks that this leaves the content
  unchanged.
- **Saving** writes the asset's package (and those of the materials, textures and skeletons the import made) to its
  file under the mount point, deterministically (D13).

**Identity.** The `Cube.glb` test fixture (`Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.glb`,
written by `MakeSkinnedFixture.py` next to it) imported with
`LeonCook Engine/Saved/CookIdentity/CookIdentity.lproj -run=ImportAssets -source=Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.glb -dest=/Game/Identity`
(a scratch project in the ignored `Engine/Saved`) saves `SM_Cube.lasset` with SHA-256
`EDB2BA3E5BF7E8DB269172C4B54BAA6B4FE23C9846B3BCAC022FCDB6194B7540` (2 372 bytes: its 12 collision triangles' material
slots; the same when imported again over it or reimported, on Win64; measured with engine version 0.25.0, package
version 6, [ps2-polish](PLANS/ps2-polish.md) P10). The engine and package versions are in the package summary, so a
release or a version bump changes the hash: 0.24.0 gave
`A24A188795C94680A639E1BC18822206EA48DE8135B9625E97C9FAC9630ED1FC`, 0.21.0 with package version 6 (N30f) gave
`2D2B59B8A9804FF746C1B126501A2A2DF88E2040FAD6432650F525ECC9407C56`, package version 5 (N22)
`F60454FC5600B82629A388B84599293F87E90A8FA9E4A58E34584F39ED8E29FF` and version 4 (N21)
`50EEB3471A5CB963FCDFB4EED52744E42D087A78DB8BFEC3BAFCCBE4D3C3F087`; the same cube as `Cube.obj`, before N21,
`93D5FB16E75F4FAD3436377D5A669CBD24B2FC9F6CB93A51B28E5018853D3C8E` (these three 2 344 bytes).

---

## Fonts

A `UFont` is UE's offline font ([ps2-polish](PLANS/ps2-polish.md) P5): its glyphs rasterized once, at import, into
texture pages the canvas samples; Leon has no runtime font cache, no composite fonts and no hinting.

- **Import.** `UTrueTypeFontFactory` (LeonEd, `-type=Font`, `.ttf`) reads the file with stb_truetype (the only
  TrueType reader, edit time only). Its settings, keys of an ImportList section like any factory's: `Height` (the
  pixels of the ascent plus the descent; 14 by default), `UnicodeRange` (hexadecimal ranges, `0020-007E,00A0-00FF`:
  ASCII's printable characters and Latin-1's, what Spanish needs; code points past 255 are not kept),
  `TexturePageWidth` / `TexturePageMaxHeight` (256, the GS's budget) and `XPadding` / `YPadding` (1 empty texel around
  each glyph, so a scaled glyph does not sample its neighbour).
- **Metrics.** The scale makes the font's ascent plus descent `Height` pixels; `Ascent`, `Descent` and `Leading` are
  rounded to whole pixels, and so is each glyph's advance, its left bearing (`HorizontalOffset`, the bitmap box's
  left edge) and its top (`VerticalOffset`, from the line's top). The kerning pairs are the font's (its `kern` table or
  GPOS pair adjustments) between every two kept characters, rounded, the zeros dropped, sorted by pair.
- **Pages.** Each glyph's coverage is quantized to 16 levels (`(c × 15 + 127) / 255`). The glyphs, tallest first
  (then widest, then by code point), go on shelves in the smallest power-of-two page, square or wider, that holds all
  of the rest; when none does, a full page takes what fits and the next page the rest. A page is a `UTexture2D` subobject
  (`Texture<N>`, not sRGB) in PF_P4: the CLUT image (white, the alpha the coverage level × 17, scaled to the GS's 0 to
  0x80 by `FGSTextureLayout::MakeClutImage`), then the indices bottom row first, as every texture's; the texture cache
  uploads it as it is (PSMT4, no conversion, not budgeted by the cook: it is already paletted). `StartU` / `StartV`
  count from the page's top-left corner.
- **Determinism.** The same file and settings give the same bytes (the float rasterizer on one machine, a fixed order
  everywhere, pages reused by name on a reimport): gate G5 reimports the engine's fonts with the rest of the content.

The engine's fonts are DejaVu Sans Condensed (`Engine/SourceArt/EngineFonts/`, the Bitstream Vera license with the
DejaVu changes in the public domain: `LICENSE.txt` there, [Engine/SourceArt/LICENSES.md](../Engine/SourceArt/LICENSES.md))
at 10, 14, 20 and 32 pixels: one page each (256 × 64, 128 × 128, 256 × 128 and 256 × 256: 8, 8, 16 and 32 KB of GS
memory when drawn), with 293, 430, 555 and 678 kerning pairs. Its bold face, from the same release
(`DejaVuSansCondensed-Bold.ttf`, [ps2-polish](PLANS/ps2-polish.md) P6), is imported at 14 pixels
(`DejaVuSansCondensedBold14`, Latin-1) and at 24 (`DejaVuSansCondensedBold24`, `UnicodeRange=0020-007E`: ASCII only),
the HUD's headings and numbers; nothing loads them but the config that names them (ShooterGame's HUD).

---

## Cube maps

A `UTextureCube` is UE's cube map ([ps2-polish](PLANS/ps2-polish.md) P8): six square faces around a point, what a map's
sky is drawn from (`FWorldSkySettings`, [LEVELS.md](LEVELS.md#the-world-settings)). The GS samples no cube, so each face
is a `UTexture2D` subobject of the cube (`PosX`, `NegX`, `PosY`, `NegY`, `PosZ`, `NegZ`, in `Faces` in UE's `ECubeFace`
order), clamped at its edges (`AddressX` / `AddressY` `Clamp`, the GS's CLAMP), which the PS2 cook palettes as any
texture and the texture cache binds on its own; UE keeps the faces as the slices of one platform data. Its tagged
properties are `Faces` and `HorizonColor` (the fog's colour when it follows the sky).

- **Faces.** Face F holds the directions Forward + u Right + v Up (u, v from -1 to 1), seen from the centre, in the
  world's axes (`UTextureCube::GetFaceBasis`): the side faces keep +Z up with UE's right (Up ^ Forward: +X's right is
  +Y, +Y's is -X, -X's is -Y, -Y's is +X); +Z's up is -X and -Z's is +X (a view pitched up or down from +X), both with
  +Y to the right. The texel columns go along Right and the rows along Up, the bottom row first, as every texture's.
- **Import.** `UTextureCubeFactory` (LeonEd, `-type=TextureCube`, `.hdr`) reads a Radiance RGBE long-lat panorama with
  stb_image (`UTextureFactory::DecodeHDRImage`): column x looks at the yaw (x + 0.5) / Width × 360 − 180 degrees (the
  middle column along +X, the yaw growing toward +Y), row y (the top row first) at the pitch 90 − (y + 0.5) / Height ×
  180 degrees. Each face texel averages four bilinear samples of it along its direction (a quarter of a texel apart;
  the columns wrap, the rows clamp at the poles). Settings: `CubeFaceSize` (128; a power of two from 8 to 256),
  `ExposureBias` (0; stops: the radiance times 2^ExposureBias) and `HorizonDegrees` (5).
- **Tone mapping.** The GS shows bytes, so the import bakes the display in: each channel of the exposed radiance x
  goes through the ACES filmic curve in Krzysztof Narkowicz's fit, x (2.51 x + 0.03) / (x (2.43 x + 0.59) + 0.14),
  clamped to 0..1 (1.0 becomes 0.80, a sun of 60 becomes white), then the sRGB curve, rounded to bytes, opaque.
  `HorizonColor` is the average of the side faces' texels within `HorizonDegrees` above the horizon, in bytes / 255.
- **Cook.** Each face is an RGBA8 `UTexture2D`, so the PS2 cook palettes it (PSMT8, 256 colours by median cut, with its
  mips, which the sky does not sample: it draws level 0, bilinear); the desktop's preview converts it the same way.
- **Determinism.** The same file and settings give the same bytes (a fixed order, faces reused by name on a
  reimport): gate G5 reimports ShooterGame's `T_Sky_Desert` with the rest of the content.

---

## Physical materials

[ps2-shipping](PLANS/ps2-shipping.md) N30f, UE's: a `UPhysicalMaterial` (`PM_`) says what a surface is made of, by its
`SurfaceType` (`EPhysicalSurface`). The surface types are the default and `SurfaceType1` to `SurfaceType62`; a project
names the ones it uses in its Engine config (`UPhysicsSettings`):

```ini
[/Script/Engine.PhysicsSettings]
+PhysicalSurfaces=(Type=SurfaceType1,Name=Concrete)
```

A `UMaterial`'s `PhysMaterial` names one. The collision queries report it when asked
(`FCollisionQueryParams::bReturnPhysicalMaterial`, `FHitResult::PhysMaterial`): a triangle's material (its slot in the
mesh's collision triangles, `MaterialIndices`, and the component's material of that slot), or the component's first
material for a simple shape ([ARCHITECTURE.md §11](ARCHITECTURE.md#11-physics)). The assets are made from an
ImportList section ([TOOLS.md](TOOLS.md#importlistini)):

```ini
[PM_Wood]
Type=PhysicalMaterial
Dest=/Game/PhysicalMaterials
SurfaceType=Wood
```

and a glTF material names its physical material in its extras, which the mesh and map imports set on its `M_`
material ([ART_PIPELINE.md](ART_PIPELINE.md#physical-materials)):

```json
"materials": [{"name": "Crate", "extras": {"physMaterial": "/Game/PhysicalMaterials/PM_Wood"}, ...}]
```

A `physMaterial` that is not a string, or extras that are not an object, are a warning; a physical material that does
not exist is an error (the material keeps what it had). Deviations from UE 4.27: no friction, restitution or density
(Leon's physics has none to use), no engine default physical material (none is the default surface), no body setup or
component override of the physical material.

<a id="legacy-content-migration"></a>

## Legacy content (migration)

The pre-P14 files were converted once and deleted, with the tools that read them (the history keeps both):

- **P14:** the `.lmat` materials (INI text) and `.lmesh` cooked meshes (binary `LMSH`) became `M_` / `SM_` packages,
  and the runtime images and sounds `T_` / `S_` imports, through `LeonCook -run=MigrateLegacyContent` and LeonEd's
  legacy material and mesh factories; a content key (the file's path) named the package it became.
- **P15:** the `.llev` levels (binary `LLEV`: a string table, a camera framing, actor and light records in the legacy
  Y-up metre space) became maps through the same commandlet (`-level=<file.llev> -dest=<MapPackage>`), while the
  level reader still existed: `Blank.llev` is `/Engine/Maps/Entry` and `Starter.llev` `/Engine/Maps/Template_Default`
  ([LEVELS.md](LEVELS.md#engine-maps)). Then the reader and saver, the content keys (`FLegacyAssetKeys`), the legacy
  factories and `MigrateLegacyContent` were deleted. The legacy coordinate conversion survives in the tests only, for
  the golden tables recorded before P7.

---

<a id="skeletal-meshes-and-animations--gltf-import"></a>

## Skeletal meshes and animations — glTF import

Since [ps2-shipping](PLANS/ps2-shipping.md) N21 skinned meshes and their animations come from glTF, as static meshes
do: `UGLTFImportFactory` with `ImportType=SkeletalMesh` / `Animation` (the import commandlet's `-type=SkeletalMesh` /
`-type=Animation`, an ImportList.ini's `Type=`), through MeshUtilities' `LoadSkeletalMeshFromGltf` and
`LoadAnimSequencesFromGltf` (`Public/GltfImport.h`). FBX (ufbx) is gone (plan decision D11). The cooked skeletal
formats of 0.11 (`.lskel`, `.lskm`, `.lanim`, `.lchar`, `*.blendspace1d.json`) were removed in 0.12.0.

**Skinned mesh** (`USkeletalMesh`, `SK_<File>`):

- The first skin a mesh node uses, and every mesh node skinned with it, merged: one section and material slot per
  primitive (glTF skins in the skin's space and ignores the mesh nodes' transforms). The materials are made as a static
  mesh's, embedded images included.
- **Skeleton**: the skin's joints, reordered so that a parent comes before its children (a joint's parent is its
  nearest ancestor joint), at most 96 (`MaxSkinBones`), each name once. A bone's reference pose is its joint node's
  local transform, with the transforms of the nodes between it and its parent joint folded in at rest (for a root
  joint, those above it: Blender's armature object); the inverse bind pose is the skin's `inverseBindMatrices`.
- **Weights**: `JOINTS_n` / `WEIGHTS_n` of every set; a joint named twice adds up; normalized, the **two largest kept**
  (the lower joint first on a tie) and renormalized, then quantized to 1/255 steps that add up to 255
  (`FSkinWeightInfo`). Two is the PS2's budget: the skinned LPS2 layout (below) gives a vertex two palette indices and
  two weights. An unused second weight is 0 and names the first bone.
- **The skeleton asset**: `Skeleton=<object path>` (its bones must match the file's: names, order and parents), else
  the mesh's own on a reimport, else `SKEL_<Name>` next to the mesh (`SK_Hero` → `SKEL_Hero`; `NewSkeletonName=`
  names it otherwise, so that several meshes share one: `SK_Body_CT` makes `SKEL_Body`, N27), found or made. A skeleton
  the import finds or makes takes the file's reference and inverse bind poses; one it is given keeps its own. Either
  takes the file's sockets: each `SOCKET_<Name>` node under a joint is a `USkeletalMeshSocket` on its nearest ancestor
  joint, placed relative to it at rest (an existing socket of that name is updated, the others are kept).
- The render data is built by `USkeletalMesh::BuildFromMeshData` (Engine's `IMeshBuilderModule::BuildSkinnedMesh`:
  MeshUtilities' `FLPS2MeshBuilder::BuildSkinned`), with the bind pose's box and each bone's bounds radius.

**Animations** (`UAnimSequence`, `A_<Animation>`): every glTF animation of the file (or the one `AnimationName=`
names) becomes a clip next to the asset the import was asked for, named after the animation (`Animation_<Index>`
without a name), on `Skeleton=` (or the clip's own on a reimport; required). Each clip records its `AnimationName`, so a
reimport reads that animation alone.

- The file must have the skeleton's bones: a node named after each bone whose nearest ancestor bone is the bone's
  parent, and, when the file has a skin, joints with the same names, order and parents (the mesh import's rule). A
  mismatch fails the import with an error.
- Channels: `translation`, `rotation` and `scale` with `LINEAR` (rotations slerped) or `STEP` interpolation. `CUBICSPLINE`
  fails the import with an error that says so (Blender exports linear keys with "Always Sample Animations"); morph target
  `weights` and channels of nodes that are not bones are ignored; the nodes between bones keep their rest.
- A clip is sampled at **30 Hz** from its first key to its last, rounded to whole frames (`SequenceLength` is
  `(frames − 1) / 30`), every bone's local transform in the engine's axes and centimetres. A `STEP` key lands on the
  frame at or after its time: between the frames around it the clip moves in one frame (33 ms). A clip loops (`bLoop`)
  unless its `extras` say `{"loop": 0}` (or `false`; N27, [below](#animation-notifies)): a one-shot holds its last
  frame (a jump's start and landing, a montage's clip). A `loop` that is not 1, 0, true or false fails the import.
- UVs: glTF's `TEXCOORD_0` starts at the image's top left and the engine's textures keep their bottom row first
  (`UTextureFactory`, OpenGL's order), so the import turns v over (`1 - v`), for static and skinned meshes alike
  (N27: the first painted textures showed the import had not since FBX's removal in N21).

<a id="animation-keys"></a>**Keys** (`FCompressedAnimSequence`, AnimationCore: `UAnimSequence`'s bulk data). One
local-space track per bone of the skeleton; each channel is a run of keys, a key being its frame number and its value:

| Channel | Value | Bytes a key | Kept |
| --- | --- | --- | --- |
| Rotation | `FQuantizedQuat48`, "smallest three": the largest component dropped (made positive: −Q is the same rotation), the others (within ±1/√2) 15 bits each; bit 15 of words 0 and 1 is the dropped component's index | 6 + 2 (frame) | always (one key when constant); off by less than 0.006° |
| Translation | `int16` × 3, `Quantized × TranslationScale + TranslationBias` per axis (the track's range over ±32 767) | 6 + 2 | when half a step is within a quarter of the tolerance (a range up to 16 m at 0.05 cm); else `float` × 3, 12 + 2 |
| Scale | `float` × 3 | 12 + 2 | only for a bone whose scale is not 1 (within the scale tolerance) |

The model-space `FMatrix` keys it replaces took 64 bytes a bone a frame. **Key reduction**: every channel is
quantized first, then from the first frame the next kept key is the farthest frame whose interpolation from the kept
one (the runtime's: lerp, and a normalized lerp along the shorter arc for rotations, from the quantized values) keeps
every frame between them within the tolerance of the source; the first and last frames are kept, and a channel that
never leaves its first key's tolerance keeps that key alone. The tolerances are settings of
`[/Script/Engine.AnimationSettings]` (`BaseEngine.ini`; `FAnimCompressionSettings`): `RotationErrorToleranceDegrees`
0.1, `TranslationErrorTolerance` 0.05 cm, `ScaleErrorTolerance` 0.001. So every 30 Hz frame of the source samples back
within them (`System.AnimationCore.Compression.KeyReductionBound`: 3 bones over 90 frames, 2 094 bytes instead of
17 280). The payload, in order: `int32` NumFrames, `float` FrameRate, the tracks (`int32` count; each `int32`
FirstRotationKey, NumRotationKeys, FirstTranslationKey, NumTranslationKeys, FirstTranslationValue, FirstScaleKey,
NumScaleKeys, `bool` bFloatTranslation, `FVector` TranslationScale, TranslationBias), then the `uint16` RotationFrames,
`uint16` Rotations (3 a key), `uint16` TranslationFrames, `int16` Translations, `float` FloatTranslations, `uint16`
ScaleFrames and `float` Scales, each an `int32` count and its elements. A load checks every run against its arrays.

**At run time** (the EE): `UAnimSequence::GetBonePose` samples the keys around a time into local transforms; the anim
instances blend poses in local space (`FAnimationRuntime`: translations and scales lerped, rotations by a normalized
lerp, n-way for a blend space, per bone for a layer, additive for an aim offset); `USkeletalMeshComponent` evaluates
the pose at most once per update into its component-space matrices (`FillUpComponentSpaceTransforms`), and the sockets
(by bone index), the skin matrices and the pose's bounds the renderer culls with all read that cached pose
([ARCHITECTURE.md](ARCHITECTURE.md#animation-runtime), [ps2-shipping](PLANS/ps2-shipping.md) N25).

<a id="animation-notifies"></a>**Notifies** ([ps2-shipping](PLANS/ps2-shipping.md) N25). An animation's notifies are
authored in its **glTF `extras`** (not a sidecar file: they travel with the clip and a reimport keeps them):

```json
{"name": "Walk_F", "extras": {"loop": 1, "notifies": [{"name": "Footstep_L", "time": 0.0}, {"name": "Footstep_R", "time": 0.333333}]},
 "channels": [...], "samplers": [...]}
```

- `name` is the event (`Footstep_L`, `Footstep_R`, `Fire`, `MagOut`, `MagIn`, `PlantBeep`...), `time` its seconds on
  the glTF timeline (the clip's first key is 0 in the asset; a time outside the clip is clamped to it). Anything else
  in `extras` but `loop` is ignored; a notify without both, or `notifies` that is not a list (N26's first exporter
  wrote a `Name@frame,...` string, which the import used to skip silently), fails the import with an error that
  says so.
- Blender: the exporter writes an action's custom properties as its animation's `extras` (Include → Custom
  Properties); `leon_art.add_action` (D6) sets the action's `loop` and `notifies` properties, the latter a list of
  `{"name", "time"}` from the clip's pose markers (the marker's name, its frame / 30): Blender's ID properties hold a
  list of groups and the exporter writes it as that JSON ([ART_PIPELINE.md](ART_PIPELINE.md#animations)).
- The import stores them as the `UAnimSequence`'s `Notifies` (`FAnimNotifyEvent` with the name and no `Notify`
  object), sorted by time. A montage adds its own (`AnimMontageFactory`'s `Notify`); a game may place `UAnimNotify`
  objects in code.
- A player fires a notify once each time it crosses its time going forward (`TriggerTime` in `[previous, current)`,
  split at a loop's wrap; the end included when a one-shot reaches it); an update of no time fires nothing.

<a id="blend-spaces-and-montages"></a>**Blend spaces, aim offsets and montages** have no source file: an
`ImportList.ini` section describes each one from the `A_` clips ([TOOLS.md](TOOLS.md#importlistini)), and the import
commandlet makes it with `UBlendSpaceFactoryNew` (`Type=BlendSpace`, 2D), `UBlendSpaceFactory1D` (`BlendSpace1D`),
`UAimOffsetBlendSpaceFactory1D` (`AimOffsetBlendSpace1D`) or `UAnimMontageFactory` (`AnimMontage`). Making one again
from the same section writes the same bytes (G5), and it has no import data (a reimport of the list makes it again).

- Blend space: `AxisX=Name,Min,Max` (and `AxisY` for 2D), `+Sample=<A_>,X[,Y]` per sample (a name in the same folder
  or a long object path), `Skeleton=` (default: the first sample's). The 2D space triangulates its samples (Delaunay on
  the axes normalized to their ranges); an input takes its triangle's barycentric weights, the nearest edge's outside
  them. Locomotion: speed (cm/s) by direction (degrees, −180 to 180).
- Aim offset: `+Sample=<A_>,<pitch>` for 3 to 5 poses from −90 to 90, `BasePose=<A_>` (default: the sample nearest 0):
  the first frame of each clip, applied as the blend's difference from the base pose.
- Montage: `Animation=<A_>`, `SlotName=` (`DefaultSlot`: the whole body; `UpperBody`: from the anim instance's branch
  bone up), `BlendInTime=`, `BlendOutTime=` (seconds), `+Section=Name,StartTime[,NextSection]`, `+Notify=Name,Time`.
  Naming: `BS_`, `AO_` (UE's aim offsets), `AM_` (UE's montages).

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

Examples: `Game/ShooterGame/ShooterGame.lproj`; a `.lplugin` example is in [BUILD.md](BUILD.md#lplugin-ue-uplugin)
(the engine ships no plugin).

---

<a id="ps2-cooked-lps2"></a>

## PS2

The PS2 runtime draws with the Graphics Synthesizer through the Renderer's GS scene renderer. ShooterGame on the EE ([ps2-engine](PLANS/ps2-engine.md) E1 to E3) loads its `.lasset` and `.lmap` packages cooked by the PS2 target platform, loose through `host:` or from `<Project>/Content/Paks/<Project>-PS2.lpak` (paths from the ELF's folder, entries aligned to 2048 bytes; `BuildCookRun -platform=PS2 -pak`), also from a bootable ISO (`-iso`, N23: the pak on `cdrom0:` behind the ELF), read asynchronously (N24).

**Textures** (E3). The PS2 cook (the "Paletted" texture format, `FPalettedTextureBuilder` in TextureCompressor) makes every RGBA8 texture `PF_P4` (up to 16 colours, exact) or `PF_P8` (up to 256 exact, more by a deterministic median cut), its sides the nearest powers of two between 8 and 256 (each texel the mean of what it covers). Since [ps2-shipping](PLANS/ps2-shipping.md) N13 it also makes the mip chain, saved as the texture's mips 1 and on: each mip halves both sides, down to 8 texels on the shorter side (6 levels for 256 x 256, at most the GS's 7), each texel the box average of the four under it in linear space for an sRGB texture (its colour weighted by their alpha). Every level indexes mip 0's one palette, as the GS reads a texture's levels through one CLUT: mip 0 decides the format and stays exact (its colours are the first entries), the mips' other colours fill the free entries by median cut and each takes its nearest entry; with more than 256 colours in mip 0 all the levels' colours are reduced together. A mip is its indices only (`GetPixelFormatMipDataSize`). The package layout already carried the mips, so the version does not change: a paletted texture cooked before N13 has one mip and still loads (without mips). The renderer's texture cache uploads them as `PSMT4` / `PSMT8` levels (MIPTBP1 / MIPTBP2) with a `PSMCT32` CLUT (CSM1).

<a id="ps2-texture-blob"></a>**The texture blob** ([ps2-shipping](PLANS/ps2-shipping.md) N23): a cooked paletted texture is load-in-place. Its data is what the GIF uploads, so the runtime neither converts nor copies it:

```text
mip 0 bulk data   the CLUT image, then level 0's indices
  CLUT            PSMCT32 words as the GS reads them in CSM1: 16 x 16 for PF_P8 (1 024 bytes, entries 8..15 and 16..23 of
                  every 32 trading places), 8 x 2 for PF_P4 (64 bytes); R, G, B, then the alpha scaled from 0..255 to the
                  GS's 0..0x80 ((a * 0x80 + 127) / 255, FGSTextureLayout::MakeClutImage)
  indices         the IMAGE transfer payload of PSMT8 (a byte a texel) or PSMT4 (two a byte, the first in the low nibble),
                  bottom row first
mip N bulk data   level N's indices, the same way
```

Every part is whole quadwords (the smallest level, 8 x 8 PSMT4, is 32 bytes; the CLUTs 64 and 1 024), and the mips'
bulk data is allocated 128-byte aligned when the texture loads (`FByteBulkData::SetPayloadAlignment`,
`GetPixelFormatDataAlignment`), so the CLUT and every level start on a cache line. The texture cache records each
upload with `FGSCommandList::UploadImageInPlace`: the PS2's DMA chain (N11) refers to the texture's own bytes by a REF
tag (the GS swizzles them into its memory as they arrive: pre-swizzling would only move work from the GS to the cook,
the EE does none either way); a texture released while a list still holds its data is copied into the recording frame's
list and the frame being sent is waited for (`FPS2RHI::RetireInPlaceImages`). The PS2 and Win64 cooks write the same
bytes, and the desktop's converter of uncooked textures makes the same blob, which the GS emulator reads. The cook writes `<Project>/Saved/Cooked/PS2-VramReport.txt`: the textures every map may draw (the config's defaults) and each map's own, with their levels, in GS blocks with the levels' alignment and the CLUT (`FGSTextureLayout::GetFootprint`, what the cache allocates), against the 1856 KB texture arena. Sounds are the SPU2's ADPCM on every platform ([below](#sound-waves)), and a static or skeletal mesh's render data is [LPS2 v2](#lps2-v2) on every platform, so the cook keeps it as it is.

<a id="sound-waves"></a>**Sounds** ([ps2-shipping](PLANS/ps2-shipping.md) N19). Both target platforms' wave format is
`SPU2ADPCM` (`ITargetPlatform::GetAllWaveFormats`): the cook makes each `USoundWave`'s ADPCM from its PCM source
(`USoundWave::CacheCompressedData`, AudioCompressor's `FSpuAdpcmEncoder`) and saves it instead of the PCM. The
desktop's editor builds make the same bytes from an uncooked sound the first time it plays, so Win64 plays what the
PS2 plays; the audio device uploads them to SPU2 RAM (PS2) or decodes them (desktop). The conversion:

- The channels are averaged into one (an SPU2 voice is mono; a spatialized sound is a point).
- The rate becomes `CompressionSampleRate` (22 050 Hz by default: effects) when the source is higher, never higher
  (0 keeps the source's; at most 48 000, the SPU2's); a windowed sinc (Blackman, 16 zero crossings) cuts at the lower
  Nyquist.
- A looping sound (`bLooping`) returns to `LoopStartFrame`: silence ahead of the sound puts that frame on a block, and
  the loop is resampled to whole blocks, so the voice jumps back seamlessly; the loop's first block uses filter 0.
- The blocks are the SPU2's (`FSpuAdpcm`): 16 bytes each, a header byte (predictor filter 0 to 4 in the high nibble,
  shift 0 to 12 in the low one), a flags byte (bit 0 loop end, bit 1 repeat, bit 2 loop start) and 28 4-bit samples,
  the first in the low nibble; a sample decodes as `(Nibble << 12) >> Shift` plus `(Previous1 * F0 + Previous2 * F1 +
  32) >> 6`, clamped to 16 bits, with (F0, F1) (0, 0), (60, 0), (115, −52), (98, −55), (122, −60). Each block keeps
  the filter and shift whose decoded samples miss the source least (squared error, each sample closed-loop against
  the decoder's arithmetic; the first pair on a tie), so the same samples give the same bytes. A one-shot's last block
  has the loop end flag alone (the voice goes silent); a loop's first block has loop start and repeat, the blocks after
  it repeat, and its last has loop end and repeat. The last block is padded with silence.
- 16 bytes for 28 samples is 3.5 times smaller than PCM16; de_leon's 37 sounds (ShooterGame's `make_sounds.py`,
  22 050 Hz mono: [ART_PIPELINE.md](ART_PIPELINE.md#sounds)) take 155 KB of the about 2 028 KB of SPU2 RAM audsrv leaves
  the audio device (`FSpuAdpcm::SoundRamBytes`; the cook's `<Project>/Saved/Cooked/<Platform>-SoundReport.txt` lists
  them per map and fails when a map's do not fit). A sound's `Priority` picks the voices it may take: below 1
  (ShooterGame's steps and impacts, 0.5) it leaves the last `NumLowPriorityVoices` free, above 1 (the bomb, the radio)
  it may take the last four (N19, N30f).

The cooked tail: the tagged properties (`Duration`, `Priority`, `bLooping`; the editor-only ones filtered), then the
format's `FName` (`SPU2ADPCM`; another fails the load with an error), the `int32` rate, the `int32` loop start frame
(−1 for a one-shot) and the blocks as bulk data. On the PS2 the audio device prepends audsrv's 16-byte header when it
uploads them (`APCM`, version 1, channels 1, the loop flag, the pitch `rate * 4096 / 48000` and the frame count).

<a id="lps2-v2"></a>

### Static mesh render data — `LPS2` v2

A static mesh's render data (`FStaticMeshLODResources::RenderData`, `FLPS2Mesh` in RenderCore) is one blob, the same
on every platform ([ps2-shipping](PLANS/ps2-shipping.md) D1): the PS2's VU1 microprograms read its batches as they
are (N14), and on Win64, in the tests and for the batches the EE clips, the GS scene renderer's C++ emitter reads it
(the reference). MeshUtilities builds it
when a mesh is imported (`FLPS2MeshBuilder`, with meshoptimizer: [LIBRARIES.md](LIBRARIES.md)); nothing builds one at
run time (`UStaticMesh::BuildFromMeshData` asks for Engine's `IMeshBuilderModule`, which only the editor and the test
programs link). The collision triangles are saved beside it at full precision (the physics scene reads those).

**Layout.** Little-endian; every record and every stream starts on a quadword (16 bytes), the unit of the DMA and of a
VIF UNPACK, so a batch goes to VU1 by reference with an UNPACK a stream.

| Part | Size | Fields |
| --- | --- | --- |
| Header | 48 bytes | `char[4]` `LPS2`; `u16` version 2; `u16` flags (bit 0: [skinned](#skinned-meshes)); `u32` sections; `u32` batches; `f32[3]` position scale; `u32` vertices (of every batch); `f32[3]` position bias; `u32` triangles |
| Section table | 16 bytes a section | `u32` first batch, batches, material slot (`StaticMaterials`), triangles |
| Batch table | 32 bytes a batch | `u32` data offset (from the blob's start, a multiple of 16); `u32` vertices (3 to 64); `f32[2]` texture coordinate offset (whole repeats); `f32[3]` bounding sphere centre and `f32` radius (the mesh's space, centimetres) |
| Batch data | each batch's four streams, one after the other | below |

A batch of N vertices:

| Stream | VIF UNPACK | Bytes (N = 64) | Value |
| --- | --- | --- | --- |
| Positions | V3-16, signed | 6 N, padded to 16 (384) | `int16` x 3: the position is `q × scale + bias` on each axis |
| Normals and flags | V4-8, signed | 4 N (256) | `int8` x 3: the unit normal × 127; the fourth byte, the strip flags |
| Colours | V4-8, unsigned | 4 N (256) | RGBA8, 255 = 1: the mesh's own colour, which scales the section's colour and alpha (white: the importers bring no vertex colour); an instance with [baked lighting](#lps2-instance-colors) brings its own stream in its place |
| Texture coordinates | V2-16, signed | 4 N (256) | `int16` x 2, 4.12 fixed point: `q / 4096 + offset` (±8 repeats around the batch's offset) |

18 bytes a vertex (the float `FVertex` it replaces was 32); 1 152 bytes and a 32-byte record for a full batch.

**Strips.** A batch's vertices are triangle strips one after the other. Vertex i closes the triangle (i − 2, i − 1, i)
unless its flags have `0x80` (no kick: the GS's ADC, drawn as XYZ3): a strip's first two vertices, and the degenerate
triangle a strip swap makes. With `0x01` that triangle is the source's (i − 1, i − 2, i): an odd triangle of its strip
(`meshopt_unstripify`'s parity), so back face culling sees the source's winding. A flags byte of `0x80` sign-extends to
a word with bit 15 set, the PACKED XYZ2's ADC bit (bit 111 of the quadword), which the VU can pass on as it is. A batch
starts a strip, so its first two vertices have `0x80`; a strip too long for a batch goes on in the next one from its
last two vertices again.

**Quantization.** Each axis of the mesh's bounding box is spread over −32767..32767 (`scale = half extent / 32767`,
`bias = centre`), so a drawn position is at most half a step from the source's: 0.5 cm for a mesh 655 m long.
`ShooterGame.Content.MeshQuantization` checks every vertex of the weapons, the characters and de_leon's meshes against
0.5 cm, and de_leon's meshes again through the scale of the actors that place them (the error grows with it). A normal
is within 1/254 a component, a texture coordinate within 1/8192 of a repeat; a batch ends before its texture
coordinates span more than 14 repeats.

**VU1's memory** (`VU1Memory` in `PS2VU1Encoder.h`, `VU1Programs.vsm`). VU1's data memory is 1024 quadwords: 3 shared
constants (the screen's scale, offset and limits), and VIF1's double buffer, BASE 16 and OFFSET 504, so two buffers of
504. A batch's buffer (quadwords from its TOP): its header (9 unlit; 23 lit since N29: the normal's transform, a sun,
the ambient, the position's transform to the world and up to two point lights), the four streams unpacked a quadword a
vertex at +24, +88, +152 and +216, and the GIF packet the program writes at +280 (a tag, then ST, RGBAQ and XYZF2 a
vertex): 280 + 1 + 3 × 64 = 473 ≤ 504.

<a id="skinned-meshes"></a>**Skinned meshes** ([ps2-shipping](PLANS/ps2-shipping.md) N21). A `USkeletalMesh`'s render
data is the same blob with the header's skinned flag, its positions and normals the bind pose's. Each batch's data
starts with its **palette** (32 bytes: `u8[24]` the skeleton's bones, `u32` how many are used, `u32` 0), and after the
texture coordinates comes a fifth stream, the **skin** (V4-8 unsigned, 4 bytes a vertex, padded to 16): two palette
indices, then their two weights in 1/255 steps that add up to 255. A batch holds at most 48 vertices and 24 bones: on
VU1 (`VU1SkinnedMemory`, `Skinned.vsm`) its buffer takes a 25-quadword header (the static lit header's 23, then the
quantization's scale and bias), the palette's matrices at +25 (3 quadwords a bone, 72), the five streams at +97,
+145, +193, +241 and +289, and the GIF packet at +337: 337 + 1 + 3 × 48 = 482 ≤ 504. `FLPS2MeshBuilder::BuildSkinned`
welds the corners with their bones and weights and closes a batch before a strip would bring a 25th bone (the palette
lists the bones in the order the batch first uses them), so a mesh of many bones splits into batches by palette. 22 bytes a vertex and 32 a batch. **Drawing**: the component sends
the pose's skin matrices and bounds each frame; a mesh whose pose's bounds are outside the view is culled whole; the
EE builds each batch's palette of the pose's skin matrices and places the batch by the sphere its pose keeps it in
(its bind-pose sphere moved by each palette bone); VU1's Skinned programs (N14b), or the C++ emitter, pose each vertex
with its two bones (linear blend).

**The build** (`FLPS2MeshBuilder`, one section at a time): quantize every corner and weld those that became equal
(`meshopt_generateVertexRemap`), drop the triangles left with two equal corners, order the rest for strips
(`meshopt_optimizeVertexCacheStrip`), stripify with a restart index (`meshopt_stripify`), then fill the batches with
whole strips, splitting only a strip longer than a batch. The output depends only on the source: the same source gives
the same bytes (`System.MeshUtilities.LPS2.Deterministic`; gate G5 reimports the content).

**Drawing** ([ARCHITECTURE.md §12](ARCHITECTURE.md#12-rendering-the-gs-path)): each batch's sphere against the view's
planes (D8). Wholly outside one: skipped. Inside the guard band and the near and far planes: on the PS2, VU1
(StaticUnlit / StaticLit, SkinnedUnlit / SkinnedLit) transforms, lights and culls it and XGKICKs its packet; elsewhere
(or with `-novu1`) the C++ emitter draws its strips as a TRISTRIP, only the vertices the drawn triangles use, with XYZ3
for those that close none. Otherwise, across a clip plane: each of its triangles clipped, on the PS2 by the same
programs on VU1 ([ps2-polish](PLANS/ps2-polish.md) P8b), elsewhere by the C++ emitter's clipper.

<a id="static-mesh-lods"></a>**LODs** ([ps2-shipping](PLANS/ps2-shipping.md) N15; UE: `SourceModels` and
`RenderData->ScreenSize`). A static mesh's `SourceModels` (a tagged property) lists its LODs, LOD 0 first; LOD *n*
keeps about `ReductionSettings.PercentTriangles` of LOD 0's triangles: `UStaticMesh::BuildFromMeshData` has
MeshUtilities simplify LOD 0's source section by section (`FLPS2MeshBuilder::Simplify`, `meshopt_simplify` with no
bound on the error, as UE's `MaxDeviation` 0, the seams and borders kept) and builds each as its own LPS2 v2 blob,
saved after the collision triangles (a mesh of one LOD saves no source models and its bytes are those of before). The
import makes them from ImportList.ini's `LODs=<share>@<size>,...` (`UGLTFImportFactory::LODs`: `LODs=0.5@0.3,0.25@0.1`).
The renderer draws LOD *n* while the bounds' sphere's projected diameter over the view's height is below its
`ScreenSize` (and above the next one's): `ComputeStaticMeshLOD`, with `StaticMeshLODDistanceScale`. The skinned blob
has one LOD.

The first `LPS2` (version 1, a header that no tool wrote and nothing read) is gone.

<a id="lps2-instance-colors"></a>

**An instance's colours** ([ps2-shipping](PLANS/ps2-shipping.md) N22, `FLPS2ColorStreams`; UE: the
`OverrideVertexColors` of a component's LOD data). The instances of a mesh share its batches, and the lighting LeonEd
bakes into a map ([LEVELS.md](LEVELS.md#static-lighting)) differs for each, so a Static mesh component keeps its own
colour streams (`UStaticMeshComponent::BakedVertexColors`) in the map:

| Field | Size | Value |
| --- | --- | --- |
| Mesh CRC | `u32` | `FCrc::MemCrc32` of the LPS2 v2 blob the colours were made for (`FLPS2Mesh::GetDataCrc`) |
| Streams | `int32` size, then the bytes | every batch's colour stream in batch order, each exactly as the mesh's (RGBA8 a vertex, 255 = 1, padded to a quadword: `FLPS2Mesh::GetColorStreamOffset`), so VU1 can UNPACK an instance's colours by reference in place of the mesh's; empty when never baked |

The component saves it after its relative transform (`VER_LEON_BAKED_VERTEX_COLORS`), a few hundred bytes a placed
cube (de_leon: 1 562 vertices, 6.3 KB). The streams apply only while their CRC and size match the mesh
(`HasValidBakedVertexColors`): a mesh rebuilt since draws with its own colours until the map is baked again. The
colour is the light times the mesh's own colour, clamped to 1, with the mesh's alpha; the renderer multiplies it by the
section's albedo and draws it with no light computed per frame.

### Materials and textures

The PS2 draws the same `UMaterial` and `UTexture2D` assets as the desktop, through the GS scene renderer: the
material's values (`FMaterial`), the cooked paletted textures uploaded by the Renderer's `FGSTextureCache` into the
arena `FPS2RHI::AllocateTextureArena` leaves, and the world's lights. There is no image file loading on PS2. (The
immediate `FPS2Material` / `FPS2Texture` path went in [ps2-shipping](PLANS/ps2-shipping.md) N2.)

---

## Code map

| Concern | Location |
| --- | --- |
| `.lasset` / `.lmap` packages | `Engine/Source/Runtime/CoreUObject` — `UPackage::Save` (`Private/UObject/SavePackage.cpp`), `FLinkerLoad`, `FLinkerSave`, `FPackageFileSummary`, `FObjectImport` / `FObjectExport`, `FPropertyTag`, `FByteBulkData`, `FPackageName` |
| `.lpak` paks | `Engine/Source/Runtime/PakFile` — `FPakInfo`, `FPakEntry`, `FPakFile`, `FPakPlatformFile` (`Public/IPlatformFilePak.h`), `FPakWriter` (`Public/PakWriter.h`); `Engine/Source/Programs/LeonPak` |
| The cook | `Engine/Source/Editor/LeonEd` — `UCookCommandlet`; `Engine/Source/Developer/TargetPlatform` — `ITargetPlatform`, `ITargetPlatformManagerModule` |
| Mesh data, material values | `Engine/Source/Runtime/RenderCore` — `FMeshData`, `FVertex`, `FMaterial` (`Public/MaterialShared.h`), `FLPS2Mesh` (`Public/LPS2Mesh.h`: LPS2 v2) |
| Asset classes | `Engine/Source/Runtime/Engine` — `Classes/Engine` (`UTexture`, `UTexture2D`, `UStaticMesh`, `USkeletalMesh`, `USkeletalMeshSocket`, `UDataAsset`), `Classes/Materials`, `Classes/Animation`, `Classes/PhysicsEngine` (`UBodySetup`), `Classes/Sound`, `Classes/Commandlets`, `Classes/EditorFramework` (`UAssetImportData`); `Public/StaticMeshResources.h`, `Private/AssetBulkData.h`; the plain skeletal data in `AnimationCore`; the GS copies of the textures in the Renderer's private `FGSTextureCache` |
| Maps: the world's save and load, `LoadMap` | `Engine/Source/Runtime/Engine` — `UWorld` (`FindWorldInPackage`, `InitWorld`, `UpdateWorldComponents`, `InitializeActorsForPlay`), `ULevel`, `UEngine::LoadMap` (`Private/UnrealEngine.cpp`) |
| Map import (glTF) | `Engine/Source/Editor/LeonEd` — `UGLTFMapFactory`, `UMapImportSettings`; `Engine/Source/Developer/MeshUtilities` — `LoadGltfScene` (`Public/GltfScene.h`) |
| Skeletal and animation import (glTF) | `Engine/Source/Developer/MeshUtilities` — `LoadSkeletalMeshFromGltf`, `LoadAnimSequencesFromGltf` (`Public/GltfImport.h`); `Engine/Source/Runtime/AnimationCore` — `FSkinWeightInfo`, `FReferenceSkeleton`, `FCompressedAnimSequence` / `FAnimCompression` (`Public/AnimCompression.h`), `FAnimationRuntime` |
| Content paths | `Engine/Source/Runtime/Core` — `FPaths` |
| DCC → mesh data | `Engine/Source/Developer/MeshUtilities` — `FStaticMeshBuilder`, `GltfImport`; `FLPS2MeshBuilder` (LPS2 v2, static and skinned, meshoptimizer), Engine's `IMeshBuilderModule` |
| Factories, reimport, commandlets | `Engine/Source/Editor/LeonEd` + `Engine/Source/Programs/LeonCook` ([TOOLS.md](TOOLS.md)) |
| PS2 drawing, materials, textures | `Engine/Source/Runtime/Renderer` — `FGSSceneRenderer`, `FGSTextureCache`; `Engine/Platforms/PS2/Source/Runtime/PS2RHI` — `FPS2RHI` |
