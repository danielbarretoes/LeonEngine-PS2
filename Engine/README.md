# Leon Engine

Reusable engine modules for Leon. Public headers: `include/leon/`.

Physical layout matches [Docs/ARCHITECTURE.md](../Docs/ARCHITECTURE.md).

## Build

Prefer configuring an **app** that pulls this tree:

- Editor: `cmake -S ../Editor`
- PS2 Lab: `Scripts/build-ps2.sh lab`
- Tools: `cmake -S ../Tools`

## Folders

| Folder | Target |
| --- | --- |
| `Core/` `Platform/` | `leon_core` `leon_platform` |
| `Serialization/` | `leon_serialization` |
| `Renderer/` | `leon_renderer` (CPU `.lmesh`/mats); GPU in `Plugins/RHI/OpenGL` |
| `Import/` | `leon_import` (OBJ/FBX/glTF cook — Editor/Tools only) |
| `Content/` | `leon_assets` (cooked data stays in `Assets/`) |
| `Animation/` `Network/` `Scene/` `Gameplay/` `Utilities/` | matching libs |
| `RHI/` `Physics/` | iface docs; impl in Plugins |

Facade: `leon_engine` = modules + default plugins.

Play-time host `leon_runtime` lives under `../Runtime/` — project CMake and Editor PIE add it after Engine (Tools do not).

See [Docs/ARCHITECTURE.md](../Docs/ARCHITECTURE.md).
