# Offline tools (LeonCook, build scripts)

Offline cooking and the helper scripts around LeonBuildTool. Build setup itself (Setup, toolchains, Docker for PS2) is in [SETUP.md](SETUP.md); file formats are in [ASSET_FORMATS.md](ASSET_FORMATS.md).

## Where tools live

The layout mirrors Unreal Engine 4.27: edit-time code is in **Developer** modules, and executables are **Program** targets that link them.

| Path | Kind | Role |
| --- | --- | --- |
| `Engine/Source/Developer/Cooker/` | Developer module | Cook commandlet (`UCookCommandlet`), recipes (`FCookRecipe`), path helpers (`FCookPaths`) |
| `Engine/Source/Developer/MeshUtilities/` | Developer module | OBJ / FBX / glTF import and `FStaticMeshBuilder` (source → `.lmesh`) |
| `Engine/Source/Programs/LeonCook/` | Program target | `LeonCook` executable: `main` forwards to `UCookCommandlet::Main` |
| `Engine/Source/Programs/LeonBuildTool/` | Build tool (CMake script) | Builds every target (UnrealBuildTool equivalent) |
| `Engine/Source/Programs/LeonAutomationTests/` | Program target | Runs every module's `Private/Tests/**` (Catch2) |
| `Engine/Source/Programs/BlankProgram/` | Program target | Minimal program: starts the statically linked modules |
| `Engine/Build/BatchFiles/` | Scripts | Build / Clean / Rebuild / Cook / RunTests / FormatCode / Lint / GenerateProjectFiles |
| `Engine/Platforms/PS2/Build/BatchFiles/` | Scripts | `RunPCSX2.ps1` (launch a PS2 build), `DockerEntry.sh` (used by LeonBuildTool) |

Both Developer modules and the LeonCook target are `PLATFORMS Desktop`: they never build for PS2.

### LeonCook link graph

```text
LeonCook (Program)
  └─ Cooker (Developer)
       ├─ Engine         (CookedSkeletal: character / anim cook, .lskel / .lskm / .lanim / .lchar writers)
       │    └─ AnimationCore (FBX skeleton / animation import through ufbx)
       ├─ MeshUtilities  (FStaticMeshBuilder, ObjImport, FbxStaticMesh, GltfImport)
       │    └─ Renderer  (LeonMaterialFormat: .lmat written by glTF import)
       └─ NlohmannJson   (recipe parsing)
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
| `character` | `--name <Name>`, `--mesh <idle.fbx>`, `--run <run.fbx>`, `--out <dir>` | `--jump <JumpingUp.fbx>`, `--fall <FallingIdle.fbx>`, `--land <Land.fbx>` | Character folder (below) |
| `anim` | `--fbx <clip.fbx>`, `--skeleton <X.lskel>`, `--out <Anims/Clip.lanim>` | `--name <ClipName>`, `--noloop` (clips loop by default) | `.lanim` |
| `recipe` | `<file.json>` | | Whatever the steps write |
| `help`, `-h`, `--help` | | | Prints usage |

Exit codes: `0` success, `1` bad arguments or recipe, `2` cook failure. Running without a mode prints usage and returns `1`. Unknown flags are rejected.

`character` writes, under `--out`:

```text
<Name>.lskel
<Name>.lskm
Materials/M_<Name>.lmat
Anims/BreathingIdle.lanim
Anims/Running.lanim
Anims/JumpingUp.lanim          (--jump)
Anims/FallingIdle.lanim        (--fall)
Anims/FallingToLanding.lanim   (--land)
<Name>_Locomotion.blendspace1d.json
<Name>.lchar
```

Implementation: `FStaticMeshBuilder::CookFromObj` / `CookFromFbx` / `CookFromGltf` (`Engine/Source/Developer/MeshUtilities/Public/StaticMeshBuilder.h`), `CookCharacterFromFbx` / `CookAnimSequenceFromFbx` (`Engine/Source/Runtime/Engine/Public/Animation/CookedSkeletal.h`).

### Examples

```bat
Engine\Binaries\Win64\LeonCook.exe staticmesh --obj mesh.obj --out mesh.lmesh
Engine\Binaries\Win64\LeonCook.exe staticmesh --fbx mesh.fbx --out mesh.lmesh
Engine\Binaries\Win64\LeonCook.exe staticmesh --gltf mesh.gltf --out mesh.lmesh --materials Materials
Engine\Binaries\Win64\LeonCook.exe character --name Bot --mesh BreathingIdle.fbx --run Running.fbx --out Characters\Bot
Engine\Binaries\Win64\LeonCook.exe anim --fbx Wave.fbx --skeleton Bot.lskel --name Wave --out Anims\Wave.lanim
Engine\Binaries\Win64\LeonCook.exe recipe CookRecipe.json
```

## Recipes

`FCookRecipe::RunFile` (`Engine/Source/Developer/Cooker/Private/CookRecipe.cpp`) runs a JSON file with a `steps` array, in order, and stops at the first failing step. Relative paths resolve next to the recipe file with `FCookPaths::ResolveBeside` (absolute paths are kept as is).

```json
{
  "steps": [
    {
      "type": "character",
      "name": "Bot",
      "mesh": "BreathingIdle.fbx",
      "run": "Running.fbx",
      "jump": "JumpingUp.fbx",
      "fall": "FallingIdle.fbx",
      "land": "FallingToLanding.fbx",
      "out": "."
    },
    {
      "type": "staticmesh",
      "gltf": "Prop.gltf",
      "out": "Prop.lmesh",
      "materials": "Materials"
    },
    {
      "type": "anim",
      "fbx": "Wave.fbx",
      "skeleton": "Bot.lskel",
      "name": "Wave",
      "out": "Anims/Wave.lanim",
      "loop": true
    }
  ]
}
```

| Step `type` | Required fields | Optional |
| --- | --- | --- |
| `character` | `name`, `mesh`, `run` | `out` (default `.`), `jump`, `fall`, `land` |
| `anim` | `fbx`, `skeleton`, `out` | `name`, `loop` (default `true`) |
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
| `Engine\Build\BatchFiles\RunTests.bat` | `[Catch2 args]` | Builds LeonAutomationTests (Win64 Development) and runs it from the repo root |
| `Engine\Build\BatchFiles\FormatCode.bat` | `[--check]` | clang-format on every `.cpp` / `.h` / `.inl` under `Engine\Source`, `Engine\Platforms`, `Engine\Plugins` and `Game` (skips `ThirdParty`, `Intermediate`, `Binaries`); `--check` is a dry run that fails on unformatted files |
| `Engine\Build\BatchFiles\Lint.bat` | | `FormatCode.bat --check`, then builds LeonAutomationTests, LeonCook, LeonGame and BlankProgram for Win64 Development |
| `GenerateProjectFiles.bat` (root) → `Engine\Build\BatchFiles\GenerateProjectFiles.bat` | `[-Project=<file.lproj>]` | Visual Studio solution in `<Engine or Project>\Intermediate\ProjectFiles` plus the root `compile_commands.json` for clangd; builds keep using Build.bat |
| `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1` | `[-Project <dir or .lproj>] [-Configuration Debug\|Development\|Shipping] [-Build]` | Optionally builds the project for PS2, then starts PCSX2 on `<Project>\Binaries\PS2\<Name>.elf` |

Linux equivalents: `Engine/Build/BatchFiles/Linux/Build.sh`, `Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh`, root `GenerateProjectFiles.sh` and `Setup.sh`.

LeonBuildTool options accepted after the positional arguments: `-Project=<file>`, `-Mode=Build|Clean|Rebuild|GenerateClangDatabase|GenerateProjectFiles|Setup`, `-NoDocker`, `-KeepGoing`.

Outputs go to `<Project or Engine>/Binaries/<Platform>/<Target><suffix>` for Development and `<Target>-<Platform>-<Configuration><suffix>` for other configurations; build trees go to `<Project or Engine>/Intermediate/Build/<Platform>/<Configuration>`.

### RunPCSX2.ps1

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson -Build
```

`-Project` defaults to `Game\ThirdPerson` and takes a folder (first `*.lproj` in it) or a `.lproj` file. PCSX2 is looked up in `$env:LEON_PCSX2`, then `pcsx2-qt.exe` on `PATH`, then the default install folders; it starts with `-fastboot -elf <ELF>`.

## Related docs

[SETUP.md](SETUP.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [LEVELS.md](LEVELS.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARD.md](CODING_STANDARD.md)
