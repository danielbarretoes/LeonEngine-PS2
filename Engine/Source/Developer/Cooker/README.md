# Cooker

Developer module (UE: the UnrealEd cooker / `UCookCommandlet`). Turns DCC sources into Leon runtime files. Desktop only; never linked into a PS2 target or a shipping game.

Canonical docs: **[Docs/TOOLS.md](../../../../Docs/TOOLS.md)** (modes, flags, recipe schema) and [Docs/ASSET_FORMATS.md](../../../../Docs/ASSET_FORMATS.md) (what gets written).

| API | Header | Role |
| --- | --- | --- |
| `UCookCommandlet::Main` | `Public/Commandlets/CookCommandlet.h` | `staticmesh` / `character` / `anim` / `recipe` modes; returns a process exit code |
| `FCookRecipe::RunFile` | `Public/CookRecipe.h` | Runs a JSON recipe (`steps` of `character` / `anim` / `staticmesh`) |
| `FCookPaths::ResolveBeside` | `Public/CookPaths.h` | Resolves recipe paths next to the recipe file |

Dependencies (`Cooker.Build.cmake`): public `Core`; private `Engine` (CookedSkeletal character / anim cook), `MeshUtilities` (`FStaticMeshBuilder`), `NlohmannJson`.

Executable: the `LeonCook` program (`Engine/Source/Programs/LeonCook`) calls `UCookCommandlet::Main` from `main`. Build and run it with `Engine\Build\BatchFiles\Cook.bat <arguments>`, or `Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development` and then `Engine\Binaries\Win64\LeonCook.exe`.
