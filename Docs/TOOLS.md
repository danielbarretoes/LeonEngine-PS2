# Offline tools (LeonCook, build scripts)

Offline cooking and the helper scripts around LeonBuildTool. Build setup itself (Setup, toolchains, Docker for PS2) is in [SETUP.md](SETUP.md); file formats are in [ASSET_FORMATS.md](ASSET_FORMATS.md).

## Where tools live

The layout mirrors Unreal Engine 4.27: edit-time code is in **Developer** modules, and executables are **Program** targets that link them.

| Path | Kind | Role |
| --- | --- | --- |
| `Engine/Source/Developer/Cooker/` | Developer module | Cook commandlet (`UCookCommandlet`), recipes (`FCookRecipe`), path helpers (`FCookPaths`) |
| `Engine/Source/Developer/MeshUtilities/` | Developer module | OBJ / FBX / glTF import and `FStaticMeshBuilder` (source → `.lmesh`); FBX skeletal import (`FbxSkeletalImport.h`) |
| `Engine/Source/Programs/LeonCook/` | Program target | `LeonCook` executable: `main` forwards to `UCookCommandlet::Main` |
| `Engine/Source/Programs/LeonBuildTool/` | Build tool (CMake script) | Builds every target (UnrealBuildTool equivalent) |
| `Engine/Source/Programs/LeonHeaderTool/` | Host program (std-only C++17, its own `CMakeLists.txt`) | Reflection code generator (UnrealHeaderTool equivalent). LeonBuildTool builds it into `Engine/Intermediate/Build/HostTools/<Host>/` and runs it for reflected modules; `LeonHeaderTool -Test` runs its golden tests. Contract: its [README](../Engine/Source/Programs/LeonHeaderTool/README.md) |
| `Engine/Source/Programs/LeonAutomationTests/` | Program target | Runs every desktop module's `Private/Tests/**` automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) |
| `Engine/Source/Programs/TestPAL/` | Program target (all platforms) | Runs the Core, Json and Projects automation tests and prints `TestPAL: PASSED (N test(s), 0 failed)` plus memory / name-pool numbers (UE: `Programs/TestPAL`) |
| `Engine/Source/Programs/BlankProgram/` | Program target | Minimal program: starts the statically linked modules |
| `Engine/Build/BatchFiles/` | Scripts | Build / Clean / Rebuild / Cook / RunTests / FormatCode / Lint / CheckBannedApis / GenerateProjectFiles |
| `Engine/Platforms/PS2/Build/BatchFiles/` | Scripts | `RunPCSX2.ps1` (launch a project's or an engine program's PS2 build), `DockerEntry.sh` (used by LeonBuildTool) |

Both Developer modules and the LeonCook target are `PLATFORMS Desktop`: they never build for PS2.

### LeonCook link graph

```text
LeonCook (Program)
  └─ Cooker (Developer)
       ├─ MeshUtilities  (FStaticMeshBuilder, ObjImport, FbxStaticMesh, GltfImport, FbxSkeletalImport)
       │    ├─ AnimationCore (skeleton / animation types filled by the FBX skeletal import)
       │    └─ Renderer  (LeonMaterialFormat: .lmat written by glTF import)
       └─ Json           (recipe parsing, native UE-style module)
```

Rules from `Engine/Source/Programs/LeonCook/LeonCook.Build.cmake`, `Engine/Source/Developer/Cooker/Cooker.Build.cmake` and `Engine/Source/Developer/MeshUtilities/MeshUtilities.Build.cmake`. Programs do not get plugins unless their target enables them (see [JoltPhysics](../Engine/Plugins/Runtime/JoltPhysics/README.md)), so LeonCook has no physics backend plugin.

## LeonCook

Output: `Engine/Binaries/Win64/LeonCook.exe` (Win64 Development). Entry point: `UCookCommandlet::Main` in `Engine/Source/Developer/Cooker/Private/Commandlets/CookCommandlet.cpp` (UE: `UE4Editor-Cmd -run=cook`).

```bat
Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development
Engine\Binaries\Win64\LeonCook.exe <mode> [options]
```

Or use the wrapper, which builds LeonCook first and forwards every argument:

```bat
Engine\Build\BatchFiles\Cook.bat staticmesh --obj Mesh.obj --out Content\Meshes\Mesh.lmesh
Engine\Build\BatchFiles\Cook.bat recipe Content\CookRecipe.json
```

### Modes

| Mode | Required | Optional | Writes |
| --- | --- | --- | --- |
| `staticmesh` | `--out <m.lmesh>` and exactly one of `--obj <m.obj>`, `--fbx <m.fbx>`, `--gltf <m.gltf>` | `--materials <dir>` (glTF only: writes `M_*.lmat` and copies textures) | `.lmesh` |
| `recipe` | `<file.json>` | | Whatever the steps write |
| `help`, `-h`, `--help` | | | Prints usage |

Exit codes: `0` success, `1` bad arguments or recipe, `2` cook failure. Running without a mode prints usage and returns `1`. Unknown flags are rejected.

The `character` and `anim` modes (cooked skeletal formats) were removed in 0.12.0; skeletal assets return as `.lasset` packages.

Implementation: `FStaticMeshBuilder::CookFromObj` / `CookFromFbx` / `CookFromGltf` (`Engine/Source/Developer/MeshUtilities/Public/StaticMeshBuilder.h`). Output is `.lmesh` version 2 in the engine world (X forward, Y right, Z up, left-handed, centimetres): OBJ and glTF are read as right-handed Y up in metres, FBX through ufbx as right-handed Z up in the file's unit ([ASSET_FORMATS.md](ASSET_FORMATS.md#static-mesh--lmesh)). Cooking the `Cube.obj` fixture gives a file with SHA-256 `EFF1459AE46710C6F1B44C0B1ECB2D739CB590F2492B9DF3EC11A03ECA7757C9`.

### Examples

```bat
Engine\Binaries\Win64\LeonCook.exe staticmesh --obj mesh.obj --out mesh.lmesh
Engine\Binaries\Win64\LeonCook.exe staticmesh --fbx mesh.fbx --out mesh.lmesh
Engine\Binaries\Win64\LeonCook.exe staticmesh --gltf mesh.gltf --out mesh.lmesh --materials Materials
Engine\Binaries\Win64\LeonCook.exe recipe CookRecipe.json
```

## Recipes

`FCookRecipe::RunFile` (`Engine/Source/Developer/Cooker/Private/CookRecipe.cpp`) runs a JSON file with a `steps` array, in order, and stops at the first failing step. Relative paths resolve next to the recipe file with `FCookPaths::ResolveBeside` (absolute paths are kept as is).

```json
{
  "steps": [
    {
      "type": "staticmesh",
      "gltf": "Prop.gltf",
      "out": "Prop.lmesh",
      "materials": "Materials"
    },
    {
      "type": "staticmesh",
      "obj": "Crate.obj",
      "out": "Crate.lmesh"
    }
  ]
}
```

| Step `type` | Required fields | Optional |
| --- | --- | --- |
| `staticmesh` | `out` + exactly one of `obj` / `fbx` / `gltf` | `materials` (glTF) |

The repository has no sample recipe or source FBX / glTF files; the only mesh source is the OBJ test fixture `Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj`.

### C++ API

| API | Header | Role |
| --- | --- | --- |
| `UCookCommandlet::Main(ArgC, ArgV)` | `Cooker/Public/Commandlets/CookCommandlet.h` | Command-line entry; returns a process exit code |
| `FCookRecipe::RunFile(RecipePath)` | `Cooker/Public/CookRecipe.h` | Runs a recipe; `0` on success |
| `FCookPaths::ResolveBeside(BaseDir, Relative)` | `Cooker/Public/CookPaths.h` | Absolute paths unchanged, otherwise `BaseDir / Relative` (normalized) |

## Build scripts

All scripts forward to LeonBuildTool (`cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- ...`). Win64 builds set up the MSVC environment through `GetVSEnv.bat`; PS2 builds run inside the pinned ps2dev Docker image unless `PS2DEV` is set on the host.

| Script | Usage | Does |
| --- | --- | --- |
| `Setup.bat` / `Setup.sh` (root) | `Setup.bat` | Downloads every pinned third-party dependency (`-Mode=Setup`) |
| `Engine\Build\BatchFiles\Build.bat` | `<Target> <Platform> <Configuration> [-Project=<file.lproj>] [-Mode=...]` | Builds a target (`Build.bat BlankProgram Win64 Development`) |
| `Engine\Build\BatchFiles\Clean.bat` | same arguments as Build | `-Mode=Clean` |
| `Engine\Build\BatchFiles\Rebuild.bat` | same arguments as Build | `-Mode=Rebuild` |
| `Engine\Build\BatchFiles\Cook.bat` | `<LeonCook arguments>` | Builds LeonCook (Win64 Development) and runs it |
| `Engine\Build\BatchFiles\RunTests.bat` | `[-automation=<filter>]` | Builds LeonAutomationTests (Win64 Development) and runs it from the repo root: every automation test (231), or those whose name contains `<filter>`; fails if any fails |
| `Engine\Build\BatchFiles\FormatCode.bat` | `[--check]` | clang-format on every `.cpp` / `.h` / `.inl` under `Engine\Source`, `Engine\Platforms`, `Engine\Plugins` and `Game` (skips `ThirdParty`, `Intermediate`, `Binaries`); `--check` is a dry run that fails on unformatted files |
| `Engine\Build\BatchFiles\Lint.bat` | | `FormatCode.bat --check`, then `CheckBannedApis.ps1`, then builds LeonAutomationTests, LeonCook, LeonGame and BlankProgram for Win64 Development |
| `Engine\Build\BatchFiles\CheckBannedApis.ps1` | | Gate G4: fails when engine or game code (`Engine\Source`, `Engine\Platforms`, `Engine\Plugins`, `Game`; comments ignored) uses glm, nlohmann, `std::vector` / `string` / `map` / `unordered_map` / `function` / `shared_ptr` / `unique_ptr`, iostream, the `printf` family, `LegacyGL` / `FLegacyTransform` / `LegacyAxes`, or `FLegacyCoordinateConversion` outside the legacy readers and tests; the allowed places are listed in [CODING_STANDARD.md §4](CODING_STANDARD.md#4-language). Violations print `<file>:<line>: G4 <rule>: <code> -> <replacement>`; `-Root <dir>` scans another tree. CI runs it with `pwsh` |
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

Runs the automation tests linked into it (the `Private/Tests` of Core, Json and Projects, `COLLECT_AUTOMATION_TESTS`) on any platform, then logs GMalloc usage and the `FName` pool size. Exit code `0` when every test passes, `1` otherwise. `-filter=<text>` runs only the tests whose name contains `<text>`.

```bat
Engine\Build\BatchFiles\Build.bat TestPAL Win64 Development
Engine\Binaries\Win64\TestPAL.exe [-filter=System.Core.Containers]
```

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build
```

On PS2 the verdict (`TestPAL: PASSED (46 test(s), 0 failed)`) and the `LogTestPAL` numbers are read from the PCSX2 log; the numbers are recorded in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md).

## Related docs

[SETUP.md](SETUP.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [LEVELS.md](LEVELS.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARD.md](CODING_STANDARD.md) · [TESTING.md](TESTING.md)
