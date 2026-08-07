# ResourceTools

Offline mesh / recipe helpers used by `leon-cook` (not Editor UI).

Canonical docs: **[Docs/TOOLS.md](../../Docs/TOOLS.md)**.

| API | Role |
| --- | --- |
| `leon::tools::ResolveBeside` | Resolve paths next to a recipe / base dir |
| `leon::tools::RunCookRecipeFile` | Run `character` / `anim` / `staticmesh` recipe steps |

CMake: `leon_resource_tools` → `leon_engine_cook` (assets + animation + renderer + OpenGL `ResourceCache`; **no** gameplay / physics / network / Engine shell).

Lightmap **load** (`.lm`) is Engine `LightmapIO`; **bake** is Editor-only — [Docs/LEVELS.md](../../Docs/LEVELS.md).
