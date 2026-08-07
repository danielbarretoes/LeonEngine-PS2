# Leon third-party libraries

Each dependency solves one problem so the engine can stay focused. Prefer English docs; this file replaces the old Spanish `LIBRERIES.md`.

| Library | Role |
| --- | --- |
| **CMake** | Build / FetchContent |
| **MSVC / clang / gcc** | C++20 toolchain |
| **GLFW** | Window, input, OpenGL context |
| **GLAD** | OpenGL 3.3 loader (vendored) |
| **GLM** | Math (headers) |
| **stb_image** | Texture decode (vendored) |
| **tinyobjloader** | Wavefront OBJ |
| **ufbx** | FBX skeletal / static / animation (vendored) |
| **cgltf** | glTF / GLB import → cook (vendored) |
| **nlohmann/json** | Level / character / BlendSpace JSON |
| **ENet** | UDP multiplayer (vendored) |
| **miniaudio** | Audio playback / decode (Unreal-like `AudioDevice`) |
| **Dear ImGui** (+ docking) | Editor UI (FetchContent) |
| **ImGuizmo** | Editor transform gizmos |
| **Catch2** | Unit tests |

OpenGL GPU code lives under `Plugins/RHI/OpenGL`. Arcade physics under `Plugins/Physics/Arcade` (AABB + optional `TriangleMesh` ComplexAsSimple lite for static CPU meshes). Optional Jolt (`LEON_WITH_JOLT`, FetchContent v5.3.0) under `Plugins/Physics/Jolt` for rigid-body `PhysScene::Step` (incremental sync + MeshShape statics) and Line/Sphere/Capsule narrow-phase traces; CMC side resolve / floor+slope overlays stay Arcade; default backend remains Arcade.

Build helpers that pull these deps: `Build/Dependencies.cmake` (FetchContent + vendored libs), `Build/LeonCompileOptions.cmake` (MSVC `/MP`).

See also: [SETUP.md](SETUP.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [NAMING.md](NAMING.md) · [TOOLS.md](TOOLS.md) · [LEVELS.md](LEVELS.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [EDITOR.md](EDITOR.md)
