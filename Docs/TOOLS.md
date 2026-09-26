# Offline tools (LeonCook, LeonPak, build scripts)

The command-line editor (LeonCook and its commandlets), the pak tool (LeonPak), the staging script (BuildCookRun) and the helper scripts around LeonBuildTool. Build setup itself (Setup, toolchains, Docker for PS2) is in [SETUP.md](SETUP.md); file formats and the import pipeline are in [ASSET_FORMATS.md](ASSET_FORMATS.md).

## Where tools live

The layout mirrors Unreal Engine 4.27: edit-time code is in **Developer** and **Editor** modules, and executables are **Program** targets that link them.

| Path | Kind | Role |
| --- | --- | --- |
| `Engine/Source/Developer/MeshUtilities/` | Developer module | OBJ / FBX / glTF import to mesh data (`FStaticMeshBuilder`); glTF scenes (`LoadGltfScene`, `GltfScene.h`); FBX skeletal import (`FbxSkeletalImport.h`) |
| `Engine/Source/Developer/TargetPlatform/` | Developer module | The platforms the cook targets (UE: TargetPlatform and the `<Platform>TargetPlatform` modules): `ITargetPlatform`, `ITargetPlatformManagerModule` (`GetTargetPlatformManager`), Win64 and PS2 (a stub) |
| `Engine/Source/Editor/LeonEd/` | Editor module | The editor module (UE: UnrealEd): the factories (`UFactory` and its subclasses, the map importer `UGLTFMapFactory` with `UMapImportSettings`), reimport (`FReimportHandler`, `FReimportManager`), `FAssetImportUtils` and the commandlets (`ImportAssets`, `ResavePackages`, `ValidateAssets`, `Cook`). [README](../Engine/Source/Editor/LeonEd/README.md) |
| `Engine/Source/Programs/LeonCook/` | Program target | `LeonCook` executable: the engine with LeonEd and no renderer, running one commandlet (UE: `UE4Editor-Cmd`) |
| `Engine/Source/Programs/LeonPak/` | Program target | `LeonPak` executable (UE: UnrealPak): creates, lists, tests and extracts `.lpak` files through the PakFile module ([below](#leonpak)) |
| `Engine/Source/Programs/LeonBuildTool/` | Build tool (CMake script) | Builds every target (UnrealBuildTool equivalent) |
| `Engine/Source/Programs/LeonHeaderTool/` | Host program (std-only C++17, its own `CMakeLists.txt`) | Reflection code generator (UnrealHeaderTool equivalent). LeonBuildTool builds it into `Engine/Intermediate/Build/HostTools/<Host>/` and runs it for reflected modules; `LeonHeaderTool -Test` runs its golden tests. Contract: its [README](../Engine/Source/Programs/LeonHeaderTool/README.md) |
| `Engine/Source/Programs/LeonAutomationTests/` | Program target | Runs every desktop module's `Private/Tests/**` automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`), LeonEd's included |
| `Engine/Source/Programs/TestPAL/` | Program target (all platforms) | Runs the Core, CoreUObject, Json, Projects and PakFile automation tests and prints `TestPAL: PASSED (N test(s), 0 failed)` plus memory / name-pool numbers (UE: `Programs/TestPAL`) |
| `Engine/Source/Programs/BlankProgram/` | Program target | Minimal program: starts the statically linked modules |
| `Engine/Build/BatchFiles/` | Scripts | Build / Clean / Rebuild / Cook / BuildCookRun / RunTests / FormatCode / Lint / CheckBannedApis / CheckReimport / GenerateProjectFiles |
| `Engine/Platforms/PS2/Build/BatchFiles/` | Scripts | `RunPCSX2.ps1` (launch a project's or an engine program's PS2 build), `DockerEntry.sh` (used by LeonBuildTool) |

The Developer and Editor modules and the LeonCook and LeonPak targets never build for PS2 (`PLATFORMS Desktop`; an Editor module is desktop-only by its type and can never be linked by a game target), and LeonEd needs `WITH_EDITORONLY_DATA` (not Shipping).

### LeonCook link graph

```text
LeonCook (Program)
  ├─ Engine, CoreUObject, Projects (the .lproj)
  └─ LeonEd (Editor)
       ├─ Engine (the asset classes, the world and the map actors, UAssetImportData, UCommandlet)
       ├─ TargetPlatform (Developer: ITargetPlatform, the cook's Win64 and PS2 platforms)
       ├─ MeshUtilities (Developer: FStaticMeshBuilder, LoadGltfScene, FbxSkeletalImport)
       │    └─ RenderCore, AnimationCore (the mesh and skeletal data)
       ├─ Json (the map nodes' extras)
       └─ STB (stb_image: the texture factory)
```

Rules from `Engine/Source/Programs/LeonCook/LeonCook.Build.cmake`, `Engine/Source/Editor/LeonEd/LeonEd.Build.cmake` and `Engine/Source/Developer/MeshUtilities/MeshUtilities.Build.cmake`. LeonCook links no Renderer or RHI (it never opens a window) and no plugin (programs get plugins only when their target enables them, see [JoltPhysics](../Engine/Plugins/Runtime/JoltPhysics/README.md)).

## LeonCook

Output: `Engine/Binaries/Win64/LeonCook.exe` (Win64 Development). Entry point: `Engine/Source/Programs/LeonCook/Private/LeonCookMain.cpp`.

```bat
Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development
Engine\Binaries\Win64\LeonCook.exe [<Project>.lproj | -project=<Project>.lproj] -run=<Commandlet> [arguments]
Engine\Binaries\Win64\LeonCook.exe -help
```

It is UE's `UE4Editor-Cmd.exe <Project>.uproject -run=<Commandlet>`: it reads the command line and the project (the first argument ending in `.lproj`, or `-project=`; without one it runs engine-only and `/Game` is its program folder `Engine/Programs/LeonCook/Content`), loads the config, starts the modules, finds the commandlet class by name through reflection (`-run=ImportAssets` makes a `UImportAssetsCommandlet`; `U<Name>` and `U<Name>Commandlet` both match, ignoring case) and calls its `Main` with the rest of the command line. The exit code is `Main`'s (0: success). Without `-run=` it lists the commandlets (`-help`: with exit code 0). The log goes to the console and to `<Project>/Saved/Logs/<Project>.log` (engine-only: `Engine/Programs/LeonCook/Saved/Logs/LeonCook.log`), and ends with `Success - N error(s), M warning(s)` (or `Failure`).

The wrapper builds LeonCook first and forwards every argument:

```bat
Engine\Build\BatchFiles\Cook.bat -run=ImportAssets -reimport -all
```

> In PowerShell, quote switches that contain a `.` (`"-source=Cube.obj"`): PowerShell splits an unquoted `-name.ext` argument in two.

### Commandlets

| `-run=` | Class (LeonEd) | Arguments | Does |
| --- | --- | --- | --- |
| `ImportAssets` | `UImportAssetsCommandlet` | `-source=<file> -dest=<LongPackagePath> [-name=<Asset>] [-type=<Type>] [-<Setting>=<Value>...]` | Imports one file into the folder `-dest` (`/Game/Meshes`); the asset is named after the file with its class prefix (`Cube.obj` → `SM_Cube`) unless `-name`; `-type` is `Texture`, `StaticMesh`, `SkeletalMesh`, `Animation`, `Sound` or `Map` (default: by extension); the other switches set the factory's properties (`-ColorSpaceMode=Linear`, `-Skeleton=/Game/Hero/SKEL_Hero.SKEL_Hero`, `-bImportMaterials=False`) |
| | | `-type=Map -source=<file.glb> -dest=/Game/Maps/<Map>` | Imports a glTF scene as the map `-dest` names (its package, UE's map path), with its meshes in `<Map>/Meshes` and materials in `<Map>/Materials`, by the naming rules of `[/Script/LeonEd.MapImportSettings]` ([LEVELS.md](LEVELS.md#importing-a-map-from-gltf)); fails when the project's `RequiredTags` are not met |
| | | `-importlist=<ImportList.ini>` | Imports every section of the list ([below](#importlistini)) |
| | | `-reimport -all` / `-reimport -package=<LongPackageName>[,...]` | Reimports every asset under the mount points (`/Engine`, and `/Game` with a project), or of those packages, whose import data names a source file that exists (the others are skipped), and saves them |
| `ResavePackages` | `UResavePackagesCommandlet` | `[-package=<LongPackageName>[,...]] [-packagefolder=<LongPackagePath>]` | Loads and saves packages in the current format (every package under the mount points by default) |
| `ValidateAssets` | `UValidateAssetsCommandlet` | same as ResavePackages | Reads each package's tables and loads it: every import must resolve (its package exists, the object is in it) and every export must be made with a valid, non-abstract class |
| `Cook` | `UCookCommandlet` | `-TargetPlatform=Win64\|PS2 [-map=<Map>+<Map>] [-package=<LongPackageName>[,...]] [-packagefolder=<LongPackagePath>]` | Cooks the maps the game opens and everything they use, without editor-only data, with the config and the shaders, into `<Project>/Saved/Cooked/<Platform>/` ([below](#the-cook)) |

An import over an existing asset reimports it in place (the same object, so its references hold). Every package an import makes or changes is saved, deterministically: the same source gives the same bytes (gate G5).

### ImportList.ini

A folder of source art lists its imports in an `ImportList.ini` (`Engine/SourceArt/ImportList.ini` for the engine, `<Project>/SourceArt/ImportList.ini` for a project), one section per asset:

| Key | Required | Value |
| --- | --- | --- |
| `Source` | yes | the source file, relative to the ini's folder |
| `Dest` | yes | the long package path of the asset's folder (`/Engine/EngineMaterials`); for a map, its package (`/Engine/Maps/AxisTest`) |
| `Name` | no | the asset name; default: the file name with its class prefix |
| `Type` | no | as `-type` |
| any other | no | an import setting: a property of the factory, set from its text (`ColorSpaceMode=Linear`, `MeshTypeToImport=FBXIT_SkeletalMesh`, `Skeleton=<object path>`, `bImportMaterials=False`) |

```ini
[T_Default_D]
Source=EngineMaterials/T_Default_D.png
Dest=/Engine/EngineMaterials

[AxisTest]
Source=Maps/AxisTest.glb
Dest=/Engine/Maps/AxisTest
Type=Map
```

The settings an import applied are kept in the asset's `UAssetImportData` with its source (relative to the engine or project folder) and MD5, so a reimport applies them again. A map keeps them in its world's; the meshes and materials a map import makes have no import data of their own (the map's reimport rebuilds them).

### Examples

```bat
Engine\Binaries\Win64\LeonCook.exe -run=ImportAssets -importlist=Engine/SourceArt/ImportList.ini
Engine\Binaries\Win64\LeonCook.exe -run=ImportAssets -reimport -all
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -source=Game/MyGame/SourceArt/Crate.fbx -dest=/Game/Props
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -source=Game/MyGame/SourceArt/Hero.fbx -dest=/Game/Hero -type=SkeletalMesh
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -type=Map -source=Game/MyGame/SourceArt/Maps/de_leon.glb -dest=/Game/Maps/de_leon
Engine\Binaries\Win64\LeonCook.exe -run=ImportAssets -type=Map -source=Engine/SourceArt/Maps/AxisTest.glb -dest=/Engine/Maps/AxisTest
Engine\Binaries\Win64\LeonCook.exe -run=ValidateAssets
```

The import identity: the `Cube.obj` fixture imported into a scratch project in the ignored `Engine/Saved`
(`LeonCook Engine/Saved/CookIdentity/CookIdentity.lproj -run=ImportAssets
-source=Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj -dest=/Game/Identity`, any minimal
`.lproj`) saves `SM_Cube.lasset` with SHA-256 `D74B95FEBE0C84509B2DA318660E0726A8762FF35C43C12233D09E88EC258115`
(engine version 0.17.0; `2EAE6C7DE3B209D7E77A0257D2E94D996A4013BF8C25F681A1AF1A7976AA149C` with 0.16.0: the summary
records the engine version), run after run.

### The cook

`LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64|PS2` (UE: cook by the book, `UCookCommandlet`) writes what a
game of the project needs into `<Project>/Saved/Cooked/<Platform>/`, emptied first:

1. **Seeds**, read from the target platform's config layers (its `IniPlatformName`: `Windows`, `PS2`):
   - the maps: `-map=A+B` (a long package name, or a name under `/Game/Maps`); else `+MapsToCook=(FilePath="/Game/Maps/X")`
     of `[/Script/UnrealEd.ProjectPackagingSettings]` in the Game config; else every map under `/Game/Maps` (UE cooks
     them all when none is listed);
   - `+DirectoriesToAlwaysCook=(Path="/Game/...")`: every package under the folder. `Engine/Config/BaseGame.ini` lists
     `/Engine/BasicShapes`, which the engine loads by path;
   - every package the Engine and Game config name by path: `GameDefaultMap`, `ServerDefaultMap`,
     `DefaultMaterialName`, `DefaultTextureName`, `DefaultBumpNormalTextureName`, the UI sounds;
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
   ini), `Engine/Shaders/**` and the `.lproj`.

The PS2 target cooks the Win64 formats for now and logs `Cook: PS2 is a stub target: ...` (the PSMT8 / PSMT4, `LPS2` v2
and ADPCM conversions come with the Engine port). Nothing in the output depends on the time or the machine: two cooks
give the same bytes. There is no `-iterate`: every cook starts from an empty folder.

```bat
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=Cook -TargetPlatform=Win64
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=Cook -TargetPlatform=PS2 -map=de_leon
```

### Reproducible reimport (gate G5)

`Engine\Build\BatchFiles\CheckReimport.bat [<Project>.lproj ...]` builds LeonCook, reimports the engine content (and each project's) with `-reimport -all`, maps included (`/Engine/Maps/AxisTest` from its `.glb`), and fails when `git diff --exit-code -- Engine/Content Game/*/Content/*` finds a change or a new file appears there. It needs a clean checkout of the content (CI runs it after the build, with `Game\ThirdPerson\ThirdPerson.lproj` and `Game\ShooterGame\ShooterGame.lproj`). The engine's maps skip a project's `RequiredTags` ([LEVELS.md](LEVELS.md#required-tags)), so a project that requires tags reimports the engine content too. A release that bumps the engine version changes every saved package's summary: resave the content (`-run=ResavePackages`) in that release.

## LeonPak

`LeonPak` (UE: UnrealPak; `Engine/Source/Programs/LeonPak`, Win64 Development: `Engine\Binaries\Win64\LeonPak.exe`)
writes and reads `.lpak` files ([ASSET_FORMATS.md](ASSET_FORMATS.md#paks--lpak)):

```bat
LeonPak <out.lpak> -create=<response file> [-align=<bytes>]
LeonPak <in.lpak> -list
LeonPak <in.lpak> -test
LeonPak <in.lpak> -extract=<dir>
```

| Switch | Does |
| --- | --- |
| `-create=<file>` | Paks the files the response file lists, one per line: `"<source path>" "<path in the pak>"` (quotes optional without spaces; blank lines and `;` / `#` comments skipped). The mount point is the longest folder every pak path starts with (`../../../` for BuildCookRun's lists) and each entry keeps the rest. The output is deterministic: data in path order, index sorted by path hash, no time recorded |
| `-align=<bytes>` | With `-create`: every entry's data starts at a multiple of it (`2048`: CD sectors, for a PS2 pak on `cdrom0:`) |
| `-list` | The mount point, then every entry sorted by path: `"<path>" offset: N, size: N bytes, sha1: <hex>`, and the totals |
| `-test` | Reads every entry and checks its SHA-1 against the index; exit code 1 and an error per corrupt entry otherwise (the index's own SHA-1 is checked when the pak opens) |
| `-extract=<dir>` | Writes every entry under `<dir>`, at its path relative to the mount point |

Exit code 0 on success, 1 on any error (a malformed response line, an unreadable source, two files with the same path,
a pak that does not open). Lint builds it with the other Win64 targets.

## BuildCookRun

`Engine\Build\BatchFiles\BuildCookRun.bat` (UE: `RunUAT BuildCookRun`; the steps are in `BuildCookRun.ps1`) builds,
cooks, stages and paks a project, and can run the staged game:

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=<Project>.lproj -platform=Win64 [-configuration=Shipping|Development]
    [-build] [-cook] [-stage] [-pak] [-run] [-addcmdline="<game arguments>"] [-align=<bytes>]
```

| Switch | Does |
| --- | --- |
| `-build` | `Build.bat <Game> Win64 <Configuration>` (default `Shipping`), then LeonCook and LeonPak in Development. The game is the project's `Game` target (`<Project>/Source/*.Target.cmake`), or `LeonGame` for a content-only project (UE: `UE4Game`) |
| `-cook` | `LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64` into `<Project>/Saved/Cooked/Win64/` |
| `-stage` | Empties `<Project>/Saved/StagedBuilds/Win64/` and copies the game there as `<Project>/Binaries/Win64/<Project>.exe` (Development) or `<Project>-Win64-<Configuration>.exe` (UE's names). Needs `-pak`: a staged build reads its content from its pak |
| `-pak` | Lists every file of the cooked folder as `"<file>" "../../../<path under Cooked/Win64>"` (`<Project>/Saved/Cooked/PakList_<Project>-Win64.txt`) and runs LeonPak into `<Project>/Content/Paks/<Project>-Win64.lpak` of the staged folder (`-align=` is passed on) |
| `-run` | Starts the staged game with the `-addcmdline` arguments (split on spaces outside double quotes) and returns its exit code |

```text
<Project>/Saved/StagedBuilds/Win64/            the staged build (UE's layout)
  <Project>/Binaries/Win64/<Project>-Win64-Shipping.exe
  <Project>/Content/Paks/<Project>-Win64.lpak  mount point ../../../ = this folder: Engine/Content, Engine/Config,
                                               Engine/Shaders, <Project>/Content, <Project>/Config, <Project>.lproj
```

The game finds its engine and project from the layout ([BUILD.md](BUILD.md#staging-and-shipping)); a Shipping build
reads nothing but its pak. Only Win64 stages today (`-platform=PS2` is refused until the Engine port). Example, the
engine's own content staged as a content-only project (GameDefaultMap `/Engine/Maps/Template_Default`):

```bat
mkdir Engine\Saved\StagingTest
> Engine\Saved\StagingTest\StagingTest.lproj echo { "FileVersion": 3, "EngineAssociation": "", "Modules": [] }
Engine\Build\BatchFiles\BuildCookRun.bat -project=Engine\Saved\StagingTest\StagingTest.lproj -platform=Win64 -build -cook -stage -pak -run "-addcmdline=-Screenshot=C:\Temp\staged.bmp -ExitAfterFrames=30"
```

A code project stages its own game target the same way. ShooterGame (P17) cooks de_leon with its meshes and
materials, the team bodies (`DirectoriesToAlwaysCook=/Game/Characters`: they are loaded by path) and the engine's
defaults, 53 files in its pak, and runs the G6 command line from the staged folder:

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
| `Setup.bat` / `Setup.sh` (root) | `Setup.bat` | Downloads every pinned third-party dependency (`-Mode=Setup`) |
| `Engine\Build\BatchFiles\Build.bat` | `<Target> <Platform> <Configuration> [-Project=<file.lproj>] [-Mode=...]` | Builds a target (`Build.bat BlankProgram Win64 Development`) |
| `Engine\Build\BatchFiles\Clean.bat` | same arguments as Build | `-Mode=Clean` |
| `Engine\Build\BatchFiles\Rebuild.bat` | same arguments as Build | `-Mode=Rebuild` |
| `Engine\Build\BatchFiles\Cook.bat` | `<LeonCook arguments>` | Builds LeonCook (Win64 Development) and runs it |
| `Engine\Build\BatchFiles\CheckReimport.bat` | `[<Project>.lproj ...]` | Gate G5: reimports the engine content (and the projects') and fails when git sees a change under a `Content` folder |
| `Engine\Build\BatchFiles\BuildCookRun.bat` | `-project=<.lproj> -platform=Win64 [-configuration=...] [-build] [-cook] [-stage] [-pak] [-run] [-addcmdline="..."]` | Builds, cooks, stages and paks a project into `<Project>\Saved\StagedBuilds\Win64\`, and runs it ([above](#buildcookrun)) |
| `Engine\Build\BatchFiles\RunTests.bat` | `[-automation=<filter>]` | Builds LeonAutomationTests (Win64 Development) and runs it from the repo root: every automation test (371), or those whose name contains `<filter>`; then the LeonHeaderTool golden tests, then ShooterGame's test program (`ShooterGameTests`, 10 tests, the same filter); fails if any fails |
| `Engine\Build\BatchFiles\SmokeTest.bat` | | Gate G6: builds ShooterGame, runs it headless on de_leon with `-ExecCmds=bot_fill -ExitAfterFrames=120` and fails unless it exits with 0 and logs ten pawns, five a team (`SmokeTest OK: 10 pawns, CT 5, T 5, exit code 0`) |
| `Engine\Build\BatchFiles\FormatCode.bat` | `[--check]` | clang-format on every `.cpp` / `.h` / `.inl` under `Engine\Source`, `Engine\Platforms`, `Engine\Plugins` and `Game` (skips `ThirdParty`, `Intermediate`, `Binaries`); `--check` is a dry run that fails on unformatted files |
| `Engine\Build\BatchFiles\Lint.bat` | | `FormatCode.bat --check`, then `CheckBannedApis.ps1`, then builds LeonAutomationTests, LeonCook, LeonPak, LeonGame and BlankProgram, and ShooterGame and ShooterGameTests, for Win64 Development |
| `Engine\Build\BatchFiles\CheckBannedApis.ps1` | | Gate G4: fails when engine or game code (`Engine\Source`, `Engine\Platforms`, `Engine\Plugins`, `Game`; comments ignored) uses glm, nlohmann, `std::vector` / `string` / `map` / `unordered_map` / `function` / `shared_ptr` / `unique_ptr`, iostream, the `printf` family, `LegacyGL` / `FLegacyTransform` / `LegacyAxes`, or `FLegacyCoordinateConversion` outside the tests (`Public/Tests`, `Private/Tests`); the allowed places are listed in [CODING_STANDARD.md §4](CODING_STANDARD.md#4-language). Violations print `<file>:<line>: G4 <rule>: <code> -> <replacement>`; `-Root <dir>` scans another tree. CI runs it with `pwsh` |
| `GenerateProjectFiles.bat` (root) → `Engine\Build\BatchFiles\GenerateProjectFiles.bat` | `[-Project=<file.lproj>]` | Visual Studio solution in `<Engine or Project>\Intermediate\ProjectFiles` plus the root `compile_commands.json` for clangd; builds keep using Build.bat |
| `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1` | `[-Project <dir or .lproj> \| -Program <Name>] [-Configuration Debug\|Development\|Shipping] [-Build]` | Optionally builds the project (or engine program) for PS2, then starts PCSX2 on `<Project>\Binaries\PS2\<Name>.elf` (`Engine\Binaries\PS2\<Name>.elf` with `-Program`) |

Linux equivalents: `Engine/Build/BatchFiles/Linux/Build.sh`, `Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh`, root `GenerateProjectFiles.sh` and `Setup.sh`.

LeonBuildTool options accepted after the positional arguments: `-Project=<file>`, `-Mode=Build|Clean|Rebuild|GenerateClangDatabase|GenerateProjectFiles|Setup`, `-NoDocker`, `-KeepGoing`.

Outputs go to `<Project or Engine>/Binaries/<Platform>/<Target><suffix>` for Development and `<Target>-<Platform>-<Configuration><suffix>` for other configurations; build trees go to `<Project or Engine>/Intermediate/Build/<Platform>/<Configuration>`.

### RunPCSX2.ps1

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson -Build
```

`-Project` defaults to `Game\ThirdPerson` and takes a folder (first `*.lproj` in it) or a `.lproj` file. `-Program <Name>` runs an engine program instead: with `-Build` it calls `Build.bat <Name> PS2 <Configuration>`, and it starts `Engine\Binaries\PS2\<Name>.elf` (`<Name>-PS2-<Configuration>.elf` outside Development). PCSX2 is looked up in `$env:LEON_PCSX2`, then `pcsx2-qt.exe` on `PATH`, then the default install folders; it starts with `-fastboot -elf <ELF>`. Program output (`UE_LOG`, `printf`) goes to the EE console, saved in `%USERPROFILE%\Documents\PCSX2\logs\emulog.txt`.

## TestPAL

Runs the automation tests linked into it (the `Private/Tests` of Core, CoreUObject, Json, Projects and PakFile, `COLLECT_AUTOMATION_TESTS`) on any platform, then logs GMalloc usage and the `FName` pool size. Before the tests it logs the reflected types, the heap their construction used and the `GUObjectArray` capacity; after them, the live object count. Exit code `0` when every test passes, `1` otherwise. `-filter=<text>` runs only the tests whose name contains `<text>`.

```bat
Engine\Build\BatchFiles\Build.bat TestPAL Win64 Development
Engine\Binaries\Win64\TestPAL.exe [-filter=System.Core.Containers]
```

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build
```

On PS2 the verdict (`TestPAL: PASSED (73 test(s), 0 failed)`) and the `LogTestPAL` numbers are read from the PCSX2 log; the numbers are recorded in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md).

## Related docs

[SETUP.md](SETUP.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [LEVELS.md](LEVELS.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARD.md](CODING_STANDARD.md) · [TESTING.md](TESTING.md)
