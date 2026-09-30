# Offline tools (LeonCook, LeonPak, build scripts)

The command-line editor (LeonCook and its commandlets), the pak tool (LeonPak), the staging script (BuildCookRun) and the helper scripts around LeonBuildTool. Build setup itself (Setup, toolchains, Docker for PS2) is in [SETUP.md](SETUP.md); file formats and the import pipeline are in [ASSET_FORMATS.md](ASSET_FORMATS.md).

## Where tools live

The layout mirrors Unreal Engine 4.27: edit-time code is in **Developer** and **Editor** modules, and executables are **Program** targets that link them.

| Path | Kind | Role |
| --- | --- | --- |
| `Engine/Source/Developer/MeshUtilities/` | Developer module | glTF import, the only mesh format: static mesh data (`FStaticMeshBuilder`), skinned meshes and animations (`LoadSkeletalMeshFromGltf`, `LoadAnimSequencesFromGltf`, `GltfImport.h`), scenes (`LoadGltfScene`, `GltfScene.h`); the LPS2 v2 build, static and skinned (`FLPS2MeshBuilder`) |
| `Engine/Source/Developer/TargetPlatform/` | Developer module | The platforms the cook targets (UE: TargetPlatform and the `<Platform>TargetPlatform` modules): `ITargetPlatform`, `ITargetPlatformManagerModule` (`GetTargetPlatformManager`), Win64 and PS2 (both cook the GS's paletted textures; only Win64 has shader formats) |
| `Engine/Source/Editor/LeonEd/` | Editor module | The editor module (UE: UnrealEd): the factories (`UFactory` and its subclasses, the map importer `UGLTFMapFactory` with `UMapImportSettings`), reimport (`FReimportHandler`, `FReimportManager`), `FAssetImportUtils` and the commandlets (`ImportAssets`, `ResavePackages`, `ValidateAssets`, `Cook`). [README](../Engine/Source/Editor/LeonEd/README.md) |
| `Engine/Source/Programs/LeonCook/` | Program target | `LeonCook` executable: the engine with LeonEd and no renderer, running one commandlet (UE: `UE4Editor-Cmd`) |
| `Engine/Source/Programs/LeonPak/` | Program target | `LeonPak` executable (UE: UnrealPak): creates, lists, tests and extracts `.lpak` files through the PakFile module ([below](#leonpak)) |
| `Engine/Source/Programs/LeonBuildTool/` | Build tool (CMake script) | Builds every target (UnrealBuildTool equivalent) |
| `Engine/Source/Programs/LeonHeaderTool/` | Host program (std-only C++17, its own `CMakeLists.txt`) | Reflection code generator (UnrealHeaderTool equivalent). LeonBuildTool builds it into `Engine/Intermediate/Build/HostTools/<Host>/` and runs it for reflected modules; `LeonHeaderTool -Test` runs its golden tests. Contract: its [README](../Engine/Source/Programs/LeonHeaderTool/README.md) |
| `Engine/Source/Programs/LeonAutomationTests/` | Program target | Runs every desktop module's `Private/Tests/**` automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`), LeonEd's included |
| `Engine/Source/Programs/TestPAL/` | Program target (all platforms) | Runs the Core, CoreUObject, Json, Projects and PakFile automation tests and prints `TestPAL: PASSED (N test(s), 0 failed)` plus memory / name-pool numbers (UE: `Programs/TestPAL`) |
| `Engine/Source/Programs/BlankProgram/` | Program target | Minimal program: starts the statically linked modules |
| `Engine/Platforms/PS2/Source/Programs/GSConformance/`, `.../VU1Conformance/` | Program targets (PS2) | The GS conformance scenes drawn on the console's GS; VU1's microprograms against the C++ emitter ([TESTING.md](TESTING.md#automated)) |
| `Engine/Build/BatchFiles/` | Scripts | Build / Clean / Rebuild / Cook / BuildCookRun / RunTests / FormatCode / Lint / CheckBannedApis / CheckReimport / SmokeTest / BotMatch / MeasurePS2 / RunGates / GenerateProjectFiles |
| `Engine/Platforms/PS2/Build/BatchFiles/` | Scripts | `RunPCSX2.ps1` (launch a project's or an engine program's PS2 build), `MeasurePS2.ps1` (`MeasurePS2.bat`'s steps), `DockerEntry.sh` (used by LeonBuildTool) |
| `Engine/Platforms/PS2/Build/PCSX2/Measure.ini` | PCSX2 settings | What `MeasurePS2` lays over the user's `PCSX2.ini`: the console's EE and VU timings, `host:` on, the EE and IOP consoles, a memory card of its own |
| `Engine/Platforms/PS2/Build/PlayRunner/` | Linux host tool | `BuildPlayRunner.sh` builds `leonrun` (`LeonRun.cpp`) in a pinned Play! checkout: an EE ELF booted headless on Play!'s HLE BIOS, no console BIOS needed ([TESTING.md](TESTING.md#automated)) |
| `Game/ShooterGame/SourceArt/check_art_determinism.py` | Blender script | Runs the art's `make_*.py` scripts twice in Blender, headless, and fails unless both runs export the same `.glb` bytes as the committed ones ([ART_PIPELINE.md](ART_PIPELINE.md)) |

The Developer and Editor modules and the LeonCook and LeonPak targets never build for PS2 (`PLATFORMS Desktop`; an Editor module is desktop-only by its type and can never be linked by a game target), and LeonEd needs `WITH_EDITORONLY_DATA` (not Shipping).

### LeonCook link graph

```text
LeonCook (Program)
  ├─ Engine, CoreUObject, Projects (the .lproj)
  └─ LeonEd (Editor)
       ├─ Engine (the asset classes, the world and the map actors, UAssetImportData, UCommandlet)
       ├─ TargetPlatform (Developer: ITargetPlatform, the cook's Win64 and PS2 platforms)
       ├─ MeshUtilities (Developer: FStaticMeshBuilder, LoadGltfScene, the glTF skeletal import)
       │    └─ RenderCore, AnimationCore (the mesh and skeletal data)
       ├─ Json (the map nodes' extras)
       └─ STB (stb_image: the texture factory)
```

Rules from `Engine/Source/Programs/LeonCook/LeonCook.Build.cmake`, `Engine/Source/Editor/LeonEd/LeonEd.Build.cmake` and `Engine/Source/Developer/MeshUtilities/MeshUtilities.Build.cmake`. LeonCook links no Renderer or RHI (it never opens a window) and no plugin (programs get plugins only when their target enables them, see [BUILD.md](BUILD.md#lplugin-ue-uplugin); the engine ships none).

## LeonCook

Output: `Engine/Binaries/Win64/LeonCook.exe` (Win64 Development). Entry point: `Engine/Source/Programs/LeonCook/Private/LeonCookMain.cpp`.

```bat
Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development
Engine\Binaries\Win64\LeonCook.exe [<Project>.lproj | -project=<Project>.lproj] -run=<Commandlet> [arguments]
Engine\Binaries\Win64\LeonCook.exe -help
```

It is UE's `UE4Editor-Cmd.exe <Project>.uproject -run=<Commandlet>`: it reads the command line and the project (the first argument ending in `.lproj`, or `-project=`; without one it runs engine-only and `/Game` is its program folder `Engine/Programs/LeonCook/Content`), loads the config, starts the modules, finds the commandlet class by name through reflection (`-run=ImportAssets` makes a `UImportAssetsCommandlet`; `U<Name>` and `U<Name>Commandlet` both match, ignoring case) and calls its `Main` with the rest of the command line. The exit code is `Main`'s (0: success). Without `-run=` it lists the commandlets (`-help`: with exit code 0). The log goes to the console and to `<Project>/Saved/Logs/<Project>.log` (engine-only: `Engine/Programs/LeonCook/Saved/Logs/LeonCook.log` — that `Saved/` tree is runtime output, not source, and is gitignored; use `-LogDir=` or `LEON_LOG_DIR` to redirect), and ends with `Success - N error(s), M warning(s)` (or `Failure`).

The wrapper builds LeonCook first and forwards every argument:

```bat
Engine\Build\BatchFiles\Cook.bat -run=ImportAssets -reimport -all
```

> In PowerShell, quote switches that contain a `.` (`"-source=Cube.glb"`): PowerShell splits an unquoted `-name.ext` argument in two.

### Commandlets

| `-run=` | Class (LeonEd) | Arguments | Does |
| --- | --- | --- | --- |
| `ImportAssets` | `UImportAssetsCommandlet` | `-source=<file> -dest=<LongPackagePath> [-name=<Asset>] [-type=<Type>] [-<Setting>=<Value>...]` | Imports one file into the folder `-dest` (`/Game/Meshes`); the asset is named after the file with its class prefix (`Cube.glb` → `SM_Cube`) unless `-name`; `-type` is `Texture`, `StaticMesh`, `SkeletalMesh`, `Animation`, `Sound`, `Map` or `Font` (default: by extension; a `.gltf` / `.glb` is a static mesh, a `.ttf` a font). `-type=SkeletalMesh` makes `SK_<File>` on `-Skeleton=` or a `SKEL_<File>` next to it; `-type=Animation -Skeleton=<path>` makes an `A_<Animation>` per glTF animation of the file next to the folder's other assets (`-AnimationName=<name>`: that one only, named by `-name`). The other switches set the factory's properties (`-ColorSpaceMode=Linear`, `-Skeleton=/Game/Hero/SKEL_Hero.SKEL_Hero`, `-bImportMaterials=False`). glTF is the only mesh and animation format ([ASSET_FORMATS.md](ASSET_FORMATS.md#skeletal-meshes-and-animations--gltf-import)) |
| | | `-type=Map -source=<file.glb> -dest=/Game/Maps/<Map>` | Imports a glTF scene as the map `-dest` names (its package, UE's map path), with its meshes in `<Map>/Meshes` and materials in `<Map>/Materials`, by the naming rules of `[/Script/LeonEd.MapImportSettings]` ([LEVELS.md](LEVELS.md#importing-a-map-from-gltf)); fails when the project's `RequiredTags` are not met |
| | | `-type=BlendSpace\|BlendSpace1D\|AimOffsetBlendSpace1D\|AnimMontage\|PhysicalMaterial -dest=<LongPackagePath> -name=<Asset> [-<Setting>=<Value>...]` | Makes an animation asset or a physical material from a description, without a source file ([ps2-shipping](PLANS/ps2-shipping.md) N25, N30f; the settings as in an ImportList section, one value each) |
| | | `-importlist=<ImportList.ini>` | Imports every section of the list ([below](#importlistini)) |
| | | `-reimport -all` / `-reimport -package=<LongPackageName>[,...]` | Reimports every asset under the mount points (`/Engine`, and `/Game` with a project), or of those packages, whose import data names a source file that exists (the others are skipped), and saves them |
| `ResavePackages` | `UResavePackagesCommandlet` | `[-package=<LongPackageName>[,...]] [-packagefolder=<LongPackagePath>] [-buildlighting]` | Loads and saves packages in the current format (every package under the mount points by default); `-buildlighting` (UE's) bakes a map's static lighting again before saving it ([LEVELS.md](LEVELS.md#static-lighting)) |
| `ValidateAssets` | `UValidateAssetsCommandlet` | same as ResavePackages | Reads each package's tables and loads it: every import must resolve (its package exists, the object is in it) and every export must be made with a valid, non-abstract class |
| `Cook` | `UCookCommandlet` | `-TargetPlatform=Win64\|PS2 [-map=<Map>+<Map>] [-package=<LongPackageName>[,...]] [-packagefolder=<LongPackagePath>]` | Cooks the maps the game opens and everything they use, without editor-only data, with the config and the shaders, into `<Project>/Saved/Cooked/<Platform>/` ([below](#the-cook)) |

An import over an existing asset reimports it in place (the same object, so its references hold). Every package an import makes or changes is saved, deterministically: the same source gives the same bytes (gate G5).

### ImportList.ini

A folder of source art lists its imports in an `ImportList.ini` (`Engine/SourceArt/ImportList.ini` for the engine, `<Project>/SourceArt/ImportList.ini` for a project), one section per asset:

| Key | Required | Value |
| --- | --- | --- |
| `Source` | yes, except for the animation assets below | the source file, relative to the ini's folder |
| `Dest` | yes | the long package path of the asset's folder (`/Engine/EngineMaterials`); for a map, its package (`/Engine/Maps/AxisTest`) |
| `Name` | no | the asset name; default: the file name with its class prefix |
| `Type` | no | as `-type` |
| any other | no | an import setting: a property of the factory, set from its text (`ColorSpaceMode=Linear`, `Skeleton=<object path>`, `AnimationName=<glTF animation>`, a sound's `CompressionSampleRate=44100`, `bLooping=True`, `LoopStartFrame=<frame>`, `Priority=2`, `bImportMaterials=False`, a font's `Height=14`, `UnicodeRange=0020-007E,00A0-00FF`) |

```ini
[T_Default_D]
Source=EngineMaterials/T_Default_D.png
Dest=/Engine/EngineMaterials

[AxisTest]
Source=Maps/AxisTest.glb
Dest=/Engine/Maps/AxisTest
Type=Map

; A character (not in the engine's list): the mesh, then its animations on the skeleton the first entry makes.
[SK_Hero]
Source=Characters/Hero.glb
Dest=/Game/Characters/Hero
Type=SkeletalMesh

[HeroAnimations]
Source=Characters/Hero_Animations.glb
Dest=/Game/Characters/Hero
Type=Animation
Skeleton=/Game/Characters/Hero/SKEL_Hero.SKEL_Hero
```

**Blend spaces, aim offsets and montages** ([ps2-shipping](PLANS/ps2-shipping.md) N25) have no source file: a section
without `Source` whose `Type` is `BlendSpace` (2D), `BlendSpace1D`, `AimOffsetBlendSpace1D` or `AnimMontage` describes
the asset, named after the section (or `Name`), from the `A_` clips of the list's earlier sections (a sample names a clip
in the same folder or a long object path). A key given several times keeps every value (`+Sample=`: the `+` adds a
value, as in UE's config files). The keys are the factory's settings ([ASSET_FORMATS.md](ASSET_FORMATS.md#blend-spaces-and-montages)):

| Type | Keys |
| --- | --- |
| `BlendSpace`, `BlendSpace1D` | `AxisX=Name,Min,Max`, `AxisY=Name,Min,Max` (2D), `+Sample=<A_>,X[,Y]`, `Skeleton=` (default: the first sample's) |
| `AimOffsetBlendSpace1D` | `AxisX` (default `Pitch,-90,90`), `+Sample=<A_>,<pitch>` (3 to 5), `BasePose=<A_>` (default: the sample nearest 0) |
| `AnimMontage` | `Animation=<A_>`, `SlotName=DefaultSlot\|UpperBody`, `BlendInTime=`, `BlendOutTime=`, `+Section=Name,StartTime[,NextSection]`, `+Notify=Name,Time` |
| `PhysicalMaterial` ([ps2-shipping](PLANS/ps2-shipping.md) N30f, `PM_`) | `SurfaceType=` a name the Engine config gives a surface type (`[/Script/Engine.PhysicsSettings] +PhysicalSurfaces=`), an enumerator (`SurfaceType4`) or `Default`; list them before the meshes whose materials name them |

```ini
[BS_Locomotion]
Type=BlendSpace
Dest=/Game/Characters/Animations
AxisX=Speed,0,250
AxisY=Direction,-180,180
+Sample=A_Idle,0,0
+Sample=A_Run_Fwd,250,0
+Sample=A_Run_Left,250,-90
+Sample=A_Run_Right,250,90
+Sample=A_Run_Bwd,250,180
+Sample=A_Run_Bwd,250,-180

[AO_Rifle]
Type=AimOffsetBlendSpace1D
Dest=/Game/Characters/Animations
+Sample=A_Aim_Down,-90
+Sample=A_Aim_Center,0
+Sample=A_Aim_Up,90

[AM_Rifle_Reload]
Type=AnimMontage
Dest=/Game/Characters/Animations
Animation=A_Rifle_Reload
SlotName=UpperBody
BlendInTime=0.1
BlendOutTime=0.2
```

The settings an import applied are kept in the asset's `UAssetImportData` with its source (relative to the engine or project folder) and MD5, so a reimport applies them again. A map keeps them in its world's; the meshes and materials a map import makes have no import data of their own (the map's reimport rebuilds them).

### Examples

```bat
Engine\Binaries\Win64\LeonCook.exe -run=ImportAssets -importlist=Engine/SourceArt/ImportList.ini
Engine\Binaries\Win64\LeonCook.exe -run=ImportAssets -reimport -all
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -source=Game/MyGame/SourceArt/Crate.glb -dest=/Game/Props
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -source=Game/MyGame/SourceArt/Hero.glb -dest=/Game/Hero -type=SkeletalMesh
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -source=Game/MyGame/SourceArt/Hero.glb -dest=/Game/Hero -type=Animation -Skeleton=/Game/Hero/SKEL_Hero.SKEL_Hero
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -type=Map -source=Game/MyGame/SourceArt/Maps/de_leon.glb -dest=/Game/Maps/de_leon
Engine\Binaries\Win64\LeonCook.exe -run=ImportAssets -type=Map -source=Engine/SourceArt/Maps/AxisTest.glb -dest=/Engine/Maps/AxisTest
Engine\Binaries\Win64\LeonCook.exe -run=ValidateAssets
```

The import identity: the `Cube.glb` fixture imported into a scratch project in the ignored `Engine/Saved`
(`LeonCook Engine/Saved/CookIdentity/CookIdentity.lproj -run=ImportAssets
-source=Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.glb -dest=/Game/Identity`, any minimal
`.lproj`) saves `SM_Cube.lasset` with SHA-256 `A24A188795C94680A639E1BC18822206EA48DE8135B9625E97C9FAC9630ED1FC` (2 372 bytes; engine version 0.24.0, package version 6, on Win64:
the summary records both; [ASSET_FORMATS.md](ASSET_FORMATS.md#importing-assets) keeps the earlier hashes), run after
run.

### The cook

`LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64|PS2 [-iterate | -full]` (UE: cook by the book,
`UCookCommandlet`) writes what a game of the project needs into `<Project>/Saved/Cooked/<Platform>/`, emptied first:

1. **Seeds**, read from the target platform's config layers (its `IniPlatformName`: `Windows`, `PS2`):
   - the maps: `-map=A+B` (a long package name, or a name under `/Game/Maps`); else `+MapsToCook=(FilePath="/Game/Maps/X")`
     of `[/Script/UnrealEd.ProjectPackagingSettings]` in the Game config; else every map under `/Game/Maps` (UE cooks
     them all when none is listed);
   - `+DirectoriesToAlwaysCook=(Path="/Game/...")`: every package under the folder. `Engine/Config/BaseGame.ini` lists
     `/Engine/BasicShapes`, which the engine loads by path;
   - every package the Engine and Game config name by path: `GameDefaultMap`, `ServerDefaultMap`,
     `DefaultMaterialName`, `DefaultTextureName`, the UI sounds;
   - `-package=` / `-packagefolder=` replace all of these with the packages they name.
2. **Dependency closure**: from the seeds, every package reached through the hard imports and the soft package
   references of the packages' tables (`FLinkerLoad::CreateLinker(nullptr, ...)`: nothing is loaded). A missing seed or
   import fails the cook; a soft reference to a missing package is a warning.
3. **Cooked packages**, sorted: each is loaded and saved with `PKG_FilterEditorOnly | PKG_Cooked` and the platform's name
   ([ASSET_FORMATS.md](ASSET_FORMATS.md#cooked-packages)): `/Engine/X` → `Engine/Content/X.lasset`, `/Game/X` →
   `<Project>/Content/X.lasset` (`.lmap` for a map). The editor-only properties and objects (the assets' and the worlds'
   `UAssetImportData`) stay out.
4. **Staged files**: `Engine/Config/Base*.ini`, the platform's `Engine/Config/<Ini>/` and
   `Engine/Platforms/<Ini>/Config/` files, the project's `Config/Default*.ini` and platform files (never an Editor
   ini), the `.lproj`, and `Engine/Shaders/**` for a platform with shader formats
   (`ITargetPlatform::GetAllTargetedShaderFormats`: Win64's GS emulator compiles them; the PS2 has none).

Both targets cook the textures to the GS's paletted formats (PSMT8 / PSMT4 with a CLUT, at most 256 x 256) and write
`<Project>/Saved/Cooked/<Platform>-VramReport.txt` beside the RAM report, and both save the sounds as the SPU2's ADPCM
([ps2-shipping](PLANS/ps2-shipping.md) N19; `<Platform>-SoundReport.txt`). The PS2 target logs it (`Cook: PS2:
paletted textures (PSMT8 / PSMT4, at most 256 x 256) and SPU2 ADPCM sounds, as Win64's; the meshes are LPS2 v2 on every
platform`): the meshes' render data is [LPS2 v2](ASSET_FORMATS.md#lps2-v2) since the import, the same for both targets.
Nothing in the output depends on the time or the machine: two cooks give the same bytes.

**Incremental cook** ([ps2-shipping](PLANS/ps2-shipping.md) N23). `-iterate` (the default) keeps every cooked package
in `<Project>/Intermediate/CookCache/<Platform>/`, at its cooked path, with a `.cookinfo` beside it (its key and what
the reports and budgets need: its textures, sounds and meshes). The key (`UCookCommandlet::GetCookKey`) is the SHA-1 of
the cooker version (`UCookCommandlet::CookerVersion`, bumped when a change to the cook changes its output), the package
format version, the platform's settings (its names and texture and wave formats) and the name and source bytes of the
package and of every package its hard imports reach. A package whose key is the cached one is copied from the cache,
not loaded; the others are cooked and cached. N22's baked lighting is in the map's own package (its lights too) and
its meshes are its imports, so a light or a mesh that changes cooks the map again; a map whose lighting needs
rebuilding repeats that warning from the cache. `-full` cooks everything (and refreshes the cache). The output folder is
still emptied first, so a full and an incremental cook give the same bytes (measured at N23: ShooterGame's PS2 cook took
2.4 s full, cold, and 0.12 s from the cache, the same files).

**Hard budgets** (N23). `[/Script/LeonEd.CookSettings]` of the target's Game config (`Engine/Config/BaseGame.ini` has
the defaults, a project's `DefaultGame.ini` or a platform's `<Platform>Game.ini` overrides them; `-<Key>=<value>` on the
command line wins) makes the cook fail, with an error naming the asset or the map, its figure and the setting:

| Key | Default | Checks |
| --- | --- | --- |
| `MapVramKB` | 0: the GS texture arena, 1 856 KB | Per map with the common assets: the textures' GS blocks (the VRAM report) |
| `MapRamKB` | 0: `[Core.MemoryBudgets] Total` of the platform's Engine config (24 576 KB on the PS2; none on Win64) | Per map: `RuntimeBaseKB` + the cooked bytes x `RuntimeExpansionPercent` / 100 (the RAM report) |
| `RuntimeBaseKB`, `RuntimeExpansionPercent` | 3 072, 200 | The RAM estimate: the engine and game without content (calibrated on de_leon's measured peak in N29), the objects over their cooked bytes |
| `MapSoundRamKB` | 0: the 2 028 KB of SPU2 RAM audsrv leaves | Per map: the SPU2 ADPCM (the sound report) |
| `MaxMeshTriangles`, `MaxMeshBones` | 4 096, 64 | Per static or skeletal mesh |
| `MaxTextureSize`, `MaxTextureBitsPerPixel` | 256, 8 | Per texture once cooked: its longer side (8 to 256) and 4 (PSMT4 only) or 8 bits |

```text
LogCook: Error: Cook: over budget: /Game/T_Big.T_Big is 256x256, over [/Script/LeonEd.CookSettings] MaxTextureSize=128
LogCook: Error: Cook: over budget: the textures of /Game/Maps/de_leon do not fit [/Script/LeonEd.CookSettings] MapVramKB=1856 (...-VramReport.txt)
```

The reports (`<Platform>-VramReport.txt`, `-RamReport.txt` with the estimate, `-SoundReport.txt`) show each map
against its budget. The staged config never carries the section.

```bat
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=Cook -TargetPlatform=Win64
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=Cook -TargetPlatform=PS2 -map=de_leon
```

### Reproducible reimport (gate G5)

`Engine\Build\BatchFiles\CheckReimport.bat [<Project>.lproj ...]` builds LeonCook, reimports the engine content (and each project's) with `-reimport -all`, maps included (`/Engine/Maps/AxisTest` from its `.glb`), and fails when `git diff --exit-code -- Engine/Content Game/*/Content/*` finds a change or a new file appears there. It needs a clean checkout of the content; run it with `Game\ShooterGame\ShooterGame.lproj` to cover the project too. The engine's maps skip a project's `RequiredTags` ([LEVELS.md](LEVELS.md#required-tags)), so a project that requires tags reimports the engine content too. A release that bumps the engine version changes every saved package's summary: resave the content (`-run=ResavePackages`) in that release.

## LeonPak

`LeonPak` (UE: UnrealPak; `Engine/Source/Programs/LeonPak`, Win64 Development: `Engine\Binaries\Win64\LeonPak.exe`)
writes and reads `.lpak` files ([ASSET_FORMATS.md](ASSET_FORMATS.md#paks--lpak)):

```bat
LeonPak <out.lpak> -create=<response file> [-align=<bytes>] [-order=<order file>]
LeonPak <in.lpak> -list
LeonPak <in.lpak> -test
LeonPak <in.lpak> -extract=<dir>
```

| Switch | Does |
| --- | --- |
| `-create=<file>` | Paks the files the response file lists, one per line: `"<source path>" "<path in the pak>"` (quotes optional without spaces; blank lines and `;` / `#` comments skipped). The mount point is the longest folder every pak path starts with (`../../../` for BuildCookRun's lists) and each entry keeps the rest. The output is deterministic: data in path order, index sorted by path hash, no time recorded |
| `-align=<bytes>` | With `-create`: every entry's data starts at a multiple of it (`2048`: CD sectors, for a PS2 pak on `cdrom0:`) |
| `-order=<file>` | With `-create`: the entries' data in the order the file gives, then the others in path order ([ps2-shipping](PLANS/ps2-shipping.md) N23). Every line with `"<path>" <rank>` counts, whatever is before it: a `-LogFileOpenOrder` run's `<Project>/Saved/Logs/FileOpenOrder-<Platform>.txt`, or a PS2 EE log's `LogFileOpenOrder:` lines; the paths are relative to the mount point (`Engine/...`, `<Project>/...`). The index keeps its format |
| `-list` | The mount point, then every entry sorted by path: `"<path>" offset: N, size: N bytes, sha1: <hex>`, and the totals |
| `-test` | Reads every entry and checks its SHA-1 against the index; exit code 1 and an error per corrupt entry otherwise (the index's own SHA-1 is checked when the pak opens) |
| `-extract=<dir>` | Writes every entry under `<dir>`, at its path relative to the mount point; an entry whose path is absolute or leads out of `<dir>` (`..`) is refused with an error and not written |

Exit code 0 on success, 1 on any error (a malformed response line, an unreadable source, two files with the same path,
a pak that does not open). Lint builds it with the other Win64 targets.

## BuildCookRun

`Engine\Build\BatchFiles\BuildCookRun.bat` (UE: `RunUAT BuildCookRun`; the steps are in `BuildCookRun.ps1`) builds,
cooks, stages and paks a project, and can run the staged game:

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=<Project>.lproj -platform=Win64|PS2 [-configuration=Shipping|Development]
    [-build] [-cook] [-stage] [-pak] [-iso] [-run] [-addcmdline="<game arguments>"] [-align=<bytes>]
    [-pakorder=<order file>] [-fullcook] [-region=NTSC|PAL] [-discserial=<SLUS_XXX.XX>]
```

| Switch | Does |
| --- | --- |
| `-build` | `Build.bat <Game> Win64 <Configuration>` (default `Shipping`), then LeonCook and LeonPak in Development. The game is the project's `Game` target (`<Project>/Source/*.Target.cmake`), or `LeonGame` for a content-only project (UE: `UE4Game`) |
| `-cook` | `LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64` into `<Project>/Saved/Cooked/Win64/`, incremental ([the cook](#the-cook)); `-fullcook` passes `-full` |
| `-stage` | Empties `<Project>/Saved/StagedBuilds/Win64/` and copies the game there as `<Project>/Binaries/Win64/<Project>.exe` (Development) or `<Project>-Win64-<Configuration>.exe` (UE's names). Needs `-pak`: a staged build reads its content from its pak |
| `-pak` | Lists every file of the cooked folder as `"<file>" "../../../<path under Cooked/Win64>"` (`<Project>/Saved/Cooked/PakList_<Project>-Win64.txt`) and runs LeonPak into `<Project>/Content/Paks/<Project>-Win64.lpak` of the staged folder (`-align=` is passed on) |
| `-pakorder=<file>` | With `-pak`: LeonPak `-order=<file>`, the entries in the order a `-LogFileOpenOrder` run opened them |
| `-iso` | PS2 only, after `-stage -pak`: the disc image `<Project>/Saved/StagedBuilds/PS2/<Project>.iso` ([below](#ps2-disc-image)); `-region=NTSC|PAL` (NTSC) sets `VMODE` and the default serial, `-discserial=` another 8.3 serial |
| `-run` | Starts the staged game with the `-addcmdline` arguments (split on spaces outside double quotes) and returns its exit code |

```text
<Project>/Saved/StagedBuilds/Win64/            the staged build (UE's layout)
  <Project>/Binaries/Win64/<Project>-Win64-Shipping.exe
  <Project>/Content/Paks/<Project>-Win64.lpak  mount point ../../../ = this folder: Engine/Content, Engine/Config,
                                               Engine/Shaders, <Project>/Content, <Project>/Config, <Project>.lproj
```

The game finds its engine and project from the layout ([BUILD.md](BUILD.md#staging-and-shipping)); a Shipping build
reads nothing but its pak. `-platform=PS2` (Development only, [ps2-engine](PLANS/ps2-engine.md) E1 and E3) cooks the textures paletted, stages no `Engine/Shaders` and writes `<Project>/Saved/Cooked/PS2-VramReport.txt` (every platform's cook writes `<Platform>-RamReport.txt` too: each map's cooked packages); `-stage` copies the ELF to `<Project>/Saved/StagedBuilds/PS2/<Project>.elf` with the cooked folder loose beside it (PCSX2's `host:` is that folder), or with `-pak` the pak `<Project>/Content/Paks/<Project>-PS2.lpak` (paths from the ELF's folder, `Engine/...` and `<Project>/...`, entries aligned to 2048 bytes unless `-align=` says otherwise) and writes `-addcmdline` into `LeonCommandLine.txt` there, which the PS2 launch reads (UE: `UECommandLine.txt`); `-run` starts it in PCSX2 without waiting (`RunPCSX2.ps1 -StagedElf`), so the result is read from the EE log.

<a id="ps2-disc-image"></a>**PS2 disc image** ([ps2-shipping](PLANS/ps2-shipping.md) N23). `-platform=PS2 -stage -pak -iso`
lays the staged game out as a disc in `<Project>/Intermediate/PS2Disc/` and makes it an ISO 9660 level 1 image with
xorriso in the PS2 build image (the Dockerfile adds it; the image is the one LeonBuildTool derives,
`leon/ps2-build:<hash>`, built here if missing), system `PLAYSTATION`, volume the project's name. On the disc, in LBA
order (`--sort-weight-list`): `SYSTEM.CNF` (`BOOT2 = cdrom0:\SLUS_990.01;1`, `VER = 1.00`, `VMODE = NTSC`; `SLES_990.01`
and `PAL` with `-region=PAL`), the ELF renamed to that 8.3 serial, the pak as `SHOOTERG\CONTENT\PAKS\SHOOTERG.LPA`, then
`AUDSRV.IRX` and `LEONCOMM.TXT` (the `-addcmdline` arguments). Every name is the ISO 9660 name of the staged path
(`FPaths::ToIso9660Path`): the PS2 file layer asks `cdrom0:` for the same names (upper case, 8.3, `;1`), so the game
boots from the disc with the same code as from `host:`. The dates are fixed (`SOURCE_DATE_EPOCH`): the same stage gives
the same image. ShooterGame's is 7 122 944 bytes at 0.24.0 (5 056 512 at N23, before the real art);
`MeasurePS2 -Iso` boots it in PCSX2 ([TESTING.md](TESTING.md#ps2-disc-boot)).

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -iso
```

That BuildCookRun stage is the **release** PS2 layout. Day-to-day iteration uses a second path: `RunPCSX2.ps1` stages config beside `<…>/Binaries/PS2/` without cooking. Both end under the owner's `Packages\` via `Publish-Package` (`Package.bat`: `Game\<Name>\Packages\PS2\` or `Engine\Packages\PS2\<Name>\`). Full matrix: [BUILD.md — PS2 staging matrix](BUILD.md#ps2-staging-matrix).

Example, the
engine's own content staged as a content-only project (GameDefaultMap `/Engine/Maps/Template_Default`):

```bat
mkdir Engine\Saved\StagingTest
> Engine\Saved\StagingTest\StagingTest.lproj echo { "FileVersion": 3, "EngineAssociation": "", "Modules": [] }
Engine\Build\BatchFiles\BuildCookRun.bat -project=Engine\Saved\StagingTest\StagingTest.lproj -platform=Win64 -build -cook -stage -pak -run "-addcmdline=-Screenshot=C:\Temp\staged.bmp -ExitAfterFrames=30"
```

A code project stages its own game target the same way. ShooterGame (P17) cooks de_leon with its meshes and
materials, the characters, weapons and sounds (`DirectoriesToAlwaysCook` `/Game/Characters`, `/Game/Weapons` and
`/Game/Sounds`: they are loaded by path) and the engine's defaults, and runs the G6 command line from the staged folder:

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=Win64 -configuration=Development -build -cook -stage -pak -run "-addcmdline=-nullrhi -ExecCmds=bot_fill -ExitAfterFrames=120"
```

### C++ API

| API | Header | Role |
| --- | --- | --- |
| `UFactory::StaticImportObject`, `FindFactoryClassForFile`, `ApplyImportSettings` | `LeonEd/Classes/Factories/Factory.h` | Import a file with a factory (the best one for its extension by default) |
| `UImportAssetsCommandlet::ImportAsset`, `ImportList`, `ReimportPackages` | `LeonEd/Classes/Commandlets/ImportAssetsCommandlet.h` | The commandlet's steps, for code and tests |
| `FReimportManager::Instance()->Reimport(Obj)` | `LeonEd/Public/EditorReimportHandler.h` | Reimport an asset from its source |
| `FAssetImportUtils` | `LeonEd/Public/AssetImportUtils.h` | Prefixes and names, package files and saves, content scans |
| `CommandletHelpers::FindCommandletClass` | `LeonEd/Public/CommandletHelpers.h` | The `-run=` lookup |
| `UCookCommandlet::GatherCookSeeds`, `CollectDependencies`, `CookPackage`, `StageNonPackageFiles`, `GetCookedFilename` | `LeonEd/Classes/Commandlets/CookCommandlet.h` | The cook's steps, for code and tests |
| `GetTargetPlatformManagerRef()`, `ITargetPlatform` | `TargetPlatform/Public/Interfaces/` | The cook's platforms |
| `FPakWriter`, `FPakFile`, `FPakPlatformFile` | `PakFile/Public/PakWriter.h`, `IPlatformFilePak.h` | Writing, reading and mounting paks |

## Build scripts

All scripts forward to LeonBuildTool (`cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- ...`). Win64 builds set up the MSVC environment through `GetVSEnv.bat`; PS2 builds run inside the pinned ps2dev Docker image unless `PS2DEV` is set on the host.

| Script | Usage | Does |
| --- | --- | --- |
| `Setup.bat` (root) | `Setup.bat` | Downloads every pinned third-party dependency (`-Mode=Setup`) |
| `Engine\Build\BatchFiles\Build.bat` | `<Target> <Platform> <Configuration> [-Project=<file.lproj>] [-Mode=...]` | Builds a target (`Build.bat BlankProgram Win64 Development`) |
| `Engine\Build\BatchFiles\Clean.bat` | same arguments as Build | `-Mode=Clean` |
| `Engine\Build\BatchFiles\Rebuild.bat` | same arguments as Build | `-Mode=Rebuild` |
| `Engine\Build\BatchFiles\Cook.bat` | `<LeonCook arguments>` | Builds LeonCook (Win64 Development) and runs it |
| `Engine\Build\BatchFiles\CheckReimport.bat` | `[<Project>.lproj ...]` | Gate G5: reimports the engine content (and the projects') and fails when git sees a change under a `Content` folder |
| `Engine\Build\BatchFiles\BuildCookRun.bat` | `-project=<.lproj> -platform=Win64\|PS2 [-configuration=...] [-build] [-cook] [-stage] [-pak] [-iso] [-run] [-addcmdline="..."] [-pakorder=<file>] [-fullcook] [-region=NTSC\|PAL] [-discserial=<serial>]` | Builds, cooks, stages and paks a project into `<Project>\Saved\StagedBuilds\<Platform>\`, makes the PS2 disc image, and runs it ([above](#buildcookrun)) |
| `Engine\Build\BatchFiles\RunTests.bat` | `[-automation=<filter>]` | Builds LeonAutomationTests (Win64 Development) and runs it from the repo root: every automation test, or those whose name contains `<filter>`; then the LeonHeaderTool golden tests, then ShooterGame's test program (`ShooterGameTests`, the same filter), then `TestPAL` (built for Win64 Development); fails if any fails. At 0.24.0: 575, 35 golden cases, 99 and 171 |
| `Engine\Build\BatchFiles\RunGates.bat` | `[-PS2] [-Measure]` | Every local gate in order: Lint, RunTests, CheckReimport, SmokeTest, BotMatch 10 7, ValidateAssets (engine and ShooterGame); `-PS2` adds `Package.bat -NoWin64` (G3), `-Measure` MeasurePS2; logs in `Engine\Saved\Gates\`, `RunGates OK` or the failed gates ([TESTING.md](TESTING.md#automated)) |
| `Engine\Build\BatchFiles\MeasurePS2.bat` | `[-Project <dir>] [-Rounds 2] [-Seed 7] [-Seconds 120] [-NoBuild] [-TimeoutSeconds 900] [-Label <text>] [-Iso] [-PakOrder <file>] [-LogFileOpenOrder] [-ExtraArgs <args>]` | The PS2 frame in PCSX2, unattended ([ps2-shipping](PLANS/ps2-shipping.md) N1): stages the game with its measuring command line (a spectated bot match, `-LogFrameTimes`, `-ExitAfterSeconds`), runs PCSX2 without its window on `Measure.ini`, reads the EE log to `ProfileSummary:`, closes PCSX2, appends to `<Project>\Saved\Profiling\PS2Frame.csv` and prints the Budgets.md row labelled `-Label`. `-Iso` boots the disc, `-PakOrder` orders its pak, `-LogFileOpenOrder` records the order, `-ExtraArgs` adds game switches (`-novu1`), `-NoBuild` keeps the stage ([TESTING.md](TESTING.md#ps2-disc-boot)) |
| `Engine\Build\BatchFiles\SmokeTest.bat` | | Gate G6: builds ShooterGame, runs it headless on de_leon with `-ExecCmds=bot_fill -ExitAfterFrames=120` and fails unless it exits with 0 and logs ten pawns, five a team (`SmokeTest OK: 10 pawns, CT 5, T 5, exit code 0`) |
| `Engine\Build\BatchFiles\BotMatch.bat` | `[Rounds] [Seed]` | Builds ShooterGame, plays a headless bot match twice (`-nullrhi -benchmark -botmatch -rounds=<Rounds> -seed=<Seed>`, 10 and 7) and fails unless both exit with 0 and log the same `Botmatch OK` line (P21) |
| `Engine\Build\BatchFiles\FormatCode.bat` | `[--check]` | clang-format on every `.cpp` / `.h` / `.inl` under `Engine\Source`, `Engine\Platforms`, `Engine\Plugins` and `Game` (skips `ThirdParty`, `Intermediate`, `Binaries`); `--check` is a dry run that fails on unformatted files. It runs `LEON_CLANG_FORMAT`, else Visual Studio's LLVM `clang-format`, else the one on `PATH`, and warns when its major version is not 20 (the repository's; 20.1.8 is the reference, `pip install clang-format==20.1.8`) |
| `Engine\Build\BatchFiles\Lint.bat` | | `FormatCode.bat --check`, then `CheckBannedApis.ps1`, then builds LeonAutomationTests, LeonCook, LeonPak, LeonGame and BlankProgram, and ShooterGame and ShooterGameTests, for Win64 Development |
| `Engine\Build\BatchFiles\CheckBannedApis.ps1` | | Gate G4: fails when engine or game code (`Engine\Source`, `Engine\Platforms`, `Engine\Plugins`, `Game`; comments ignored) uses glm, nlohmann, a `std::` container, string, `string_view`, stream, function or smart pointer or its header (D2), iostream, the `printf` family (`vfprintf`, `_snprintf`, ...), `LegacyGL` / `FLegacyTransform` / `LegacyAxes`, a removed API of the ps2-shipping plan (the FBX / OBJ importers and the model-space animation of N21, ...), or `FLegacyCoordinateConversion` outside the tests (`Public/Tests`, `Private/Tests`); the allowed places are listed in [CODING_STANDARD.md §4](CODING_STANDARD.md#4-language). Violations print `<file>:<line>: G4 <rule>: <code> -> <replacement>`; `-Root <dir>` scans another tree. `Lint.bat` runs it (`pwsh` works too) |
| `Engine\Build\BatchFiles\GetVSEnv.bat` | (no arguments) | Called by `Build.bat` and `GenerateProjectFiles.bat`: runs Visual Studio's `vcvars64.bat` (18, then 2022) and checks that CMake and Ninja are on `PATH` |
| `GenerateProjectFiles.bat` (root) → `Engine\Build\BatchFiles\GenerateProjectFiles.bat` | `[-Project=<file.lproj>]` | Visual Studio solution in `<Engine or Project>\Intermediate\ProjectFiles` plus the root `compile_commands.json` for clangd; builds keep using Build.bat |
| `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1` | `[-Project <dir or .lproj> \| -Program <Name> \| -StagedElf <file.elf>] [-Configuration Debug\|Development\|Shipping] [-Build] [-NoStage] [-StageOnly]` | Optionally builds the project (or engine program) for PS2, then starts PCSX2 on `<Project>\Binaries\PS2\<Name>.elf` (`Engine\Binaries\PS2\<Name>.elf` with `-Program`), or on an ELF already staged with its content (`-StagedElf`) |

The batch files are for Windows: Win64 is the development and editor platform, PS2 the target, and there are no shell equivalents. LeonBuildTool itself is a CMake script, so a PS2 build can also be started with `cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- <Target> PS2 <Configuration>` from a Linux host (WSL, a CI runner); it runs the container as root and gives the outputs back to the calling user (`LEON_HOST_UID` / `LEON_HOST_GID`, [BUILD.md](BUILD.md#ps2-builds-in-docker)).

LeonBuildTool options accepted after the positional arguments: `-Project=<file>`, `-Mode=Build|Clean|Rebuild|GenerateClangDatabase|GenerateProjectFiles|Setup`, `-NoDocker`, `-KeepGoing`.

Outputs go to `<Project or Engine>/Binaries/<Platform>/<Target><suffix>` for Development and `<Target>-<Platform>-<Configuration><suffix>` for other configurations; every target builds in the one tree of its platform and configuration, `Engine/Intermediate/Build/<Platform>/<Configuration>` ([BUILD.md](BUILD.md#build-trees-and-outputs)).

### RunPCSX2.ps1

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -StagedElf Game\ShooterGame\Saved\StagedBuilds\PS2\ShooterGame.elf
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build
```

`-StagedElf <file.elf>` starts an ELF that is already staged with its content (`BuildCookRun -platform=PS2 -stage`,
whose `-run` calls it) as it is. ShooterGame reads its content from its pak, so its staged build is the one to boot.
`-Project` defaults to `Game\ShooterGame` and takes a folder (first `*.lproj` in it) or a `.lproj` file; it stages
the config (not the content) beside `<Project>\Binaries\PS2\` and starts that ELF, `-StageOnly` only stages, and
`-NoStage` stages nothing (the game runs on its compiled defaults). `-Program <Name>` runs an engine program instead: with `-Build` it calls `Build.bat <Name> PS2 <Configuration>`, and it starts `Engine\Binaries\PS2\<Name>.elf` (`<Name>-PS2-<Configuration>.elf` outside Development). PCSX2 is looked up in `$env:LEON_PCSX2`, then `pcsx2-qt.exe` on `PATH`, then the default install folders; it starts with `-fastboot -elf <ELF>`. Program output (`UE_LOG`, `printf`) goes to the EE console, saved in `%USERPROFILE%\Documents\PCSX2\logs\emulog.txt`.

## TestPAL

Runs the automation tests linked into it (the `Private/Tests` of Core, CoreUObject, Json, Projects and PakFile, `COLLECT_AUTOMATION_TESTS`) on Win64 and PS2, then logs GMalloc usage and the `FName` pool size. Before the tests it logs the reflected types, the heap their construction used and the `GUObjectArray` capacity; after them, the live object count. Exit code `0` when every test passes, `1` otherwise. `-filter=<text>` runs only the tests whose name contains `<text>`.

```bat
Engine\Build\BatchFiles\Build.bat TestPAL Win64 Development
Engine\Binaries\Win64\TestPAL.exe [-filter=System.Core.Containers]
```

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build
```

`RunTests.bat` builds and runs it on Win64 (171 tests at 0.24.0). On PS2 (fewer: the desktop-only file, config and log tests stay out; 157 at [ps2-shipping](PLANS/ps2-shipping.md) N15, the last EE run) the verdict (`TestPAL: PASSED (N test(s), 0 failed)`) and the `LogTestPAL` numbers are read from the PCSX2 log; the numbers are recorded in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md). `RunPCSX2.ps1 -Program VU1Conformance -Build` runs VU1Conformance the same way (`VU1Conformance: PASSED (84 batch(es), 0 failed)`).

## Related docs

[SETUP.md](SETUP.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [LEVELS.md](LEVELS.md) · [ART_PIPELINE.md](ART_PIPELINE.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARD.md](CODING_STANDARD.md) · [TESTING.md](TESTING.md)
