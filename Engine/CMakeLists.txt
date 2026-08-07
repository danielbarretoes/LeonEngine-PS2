# Leon Engine modules + facade. Apps add_subdirectory(Engine) or configure from Editor/Projects.
# Prefer: cmake -S Editor | cmake -S Projects/Ps2Lab | cmake -S Tools
#
# Physical layout matches Docs/ARCHITECTURE.md:
#   Core/Platform → Assets/Net/RHI-iface/Serialization → Renderer/Anim/Scene → Gameplay
#   Consumers add Runtime (shipping) or Editor / Tools separately — Engine does not pull them.

cmake_minimum_required(VERSION 3.20)

set(_LEON_ENGINE_IS_TOPLEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_LEON_ENGINE_IS_TOPLEVEL ON)
endif()

get_filename_component(LEON_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

if(_LEON_ENGINE_IS_TOPLEVEL)
    project(LeonEngine VERSION 0.9.0 LANGUAGES C CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(CMAKE_CONFIGURATION_TYPES)
        foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
            string(TOUPPER "${_cfg}" _cfg_up)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_BINARY_DIR}/${_cfg}")
        endforeach()
    else()
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif()

    option(LEON_BUILD_TESTS "Build Catch2 unit tests (leon_tests)" ON)
    option(LEON_BUILD_CLIENT "Fetch ImGui (needed if a consumer builds the Editor)" OFF)
    option(LEON_WITH_JOLT "Fetch and build Jolt Physics backend" ON)

    set(LEON_ENGINE_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Assets" CACHE PATH "Engine runtime assets root")
    set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "Projects root (replaces games/)")

    set(LEON_RHI "OpenGL" CACHE STRING "RHI plugin (OpenGL|PS2)")
    set_property(CACHE LEON_RHI PROPERTY STRINGS OpenGL PS2)

    include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")

    if(LEON_BUILD_TESTS)
        enable_testing()
    endif()
else()
    if(NOT DEFINED LEON_BUILD_CLIENT)
        set(LEON_BUILD_CLIENT OFF)
    endif()
    if(NOT DEFINED LEON_BUILD_TESTS)
        set(LEON_BUILD_TESTS OFF)
    endif()
    if(NOT DEFINED LEON_ENGINE_ASSETS)
        set(LEON_ENGINE_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Assets" CACHE PATH "Engine runtime assets root")
    endif()
    if(NOT DEFINED LEON_PROJECTS_ROOT)
        set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "Projects root")
    endif()
    if(NOT DEFINED LEON_RHI)
        set(LEON_RHI "OpenGL" CACHE STRING "RHI plugin")
    endif()
    # Consumer apps include Dependencies before add_subdirectory(Engine); ensure platform flags exist.
    if(NOT DEFINED LEON_PLATFORM_HOST AND NOT DEFINED LEON_PLATFORM_PS2)
        include("${LEON_REPO_ROOT}/Build/LeonPlatform.cmake")
    endif()
endif()

set(_LEON_PUBLIC_INC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/ThirdParty/stb>
    $<INSTALL_INTERFACE:include>
)

if(LEON_PLATFORM_PS2)
    include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.Ps2.txt")
    return()
endif()

# --- Interface-only contracts ---
add_library(leon_rhi INTERFACE)
target_include_directories(leon_rhi INTERFACE ${_LEON_PUBLIC_INC})
add_library(Leon::RHI ALIAS leon_rhi)

add_library(leon_physics_iface INTERFACE)
target_include_directories(leon_physics_iface INTERFACE ${_LEON_PUBLIC_INC})
add_library(Leon::PhysicsIface ALIAS leon_physics_iface)

# --- Core ---
add_library(leon_core STATIC
    Core/src/Camera.cpp
    Core/src/FileIO.cpp
    Core/src/Input.cpp
    Core/src/InputMapping.cpp
    Core/src/Paths.cpp
    Core/src/Transform.cpp
)
target_include_directories(leon_core PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_core PUBLIC glm::glm glfw)
if(WIN32)
    target_link_libraries(leon_core PUBLIC Psapi)
endif()
add_library(Leon::Core ALIAS leon_core)

# --- Platform (Host GLFW) ---
add_library(leon_platform STATIC
    Platform/src/Host/WindowGlfw.cpp
    Platform/src/Host/MemoryStatsHost.cpp
)
target_include_directories(leon_platform PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_platform PUBLIC
    leon_core
    leon_rhi
    glfw
)
if(WIN32)
    target_link_libraries(leon_platform PUBLIC dxgi)
elseif(UNIX)
    find_package(Threads REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(leon_platform PUBLIC Threads::Threads OpenGL::GL ${CMAKE_DL_LIBS})
endif()
add_library(Leon::Platform ALIAS leon_platform)

# --- Serialization ---
add_library(leon_serialization STATIC
    Serialization/src/JsonUtil.cpp
)
target_include_directories(leon_serialization PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_serialization PUBLIC nlohmann_json::nlohmann_json glm::glm)
add_library(Leon::Serialization ALIAS leon_serialization)

# --- Utilities (AHUD / UserWidget; GPU text/mesh in OpenGL DebugOverlay) ---
add_library(leon_utilities STATIC
    Utilities/src/HUD.cpp
    Utilities/src/WidgetPaintContext.cpp
    Utilities/src/TextBlockWidget.cpp
    Utilities/src/MenuListWidget.cpp
    Utilities/src/ButtonWidget.cpp
    Utilities/src/VerticalBoxWidget.cpp
    Utilities/src/ProgressBarWidget.cpp
    Utilities/src/ImageWidget.cpp
    Utilities/src/InteractionPromptWidget.cpp
)
target_include_directories(leon_utilities PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_utilities PUBLIC leon_core leon_platform glm::glm)
add_library(Leon::Utilities ALIAS leon_utilities)

# --- Assets / content (code in Content/; cooked data in Assets/) ---
add_library(leon_assets STATIC
    Content/src/ContentValidator.cpp
    Content/src/CookedSkeletal.cpp
    Content/src/LevelClassNames.cpp
)
target_include_directories(leon_assets PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_assets PUBLIC
    leon_core
    leon_serialization
    nlohmann_json::nlohmann_json
)
add_library(Leon::Assets ALIAS leon_assets)

# --- Animation ---
add_library(leon_animation STATIC
    Animation/src/SkeletalAnimation.cpp
    Animation/src/CharacterAnimInstance.cpp
)
target_include_directories(leon_animation PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_animation PUBLIC leon_core glm::glm ufbx)
add_library(Leon::Animation ALIAS leon_animation)

# --- Network ---
add_library(leon_network STATIC
    Network/src/NetDriver.cpp
    Network/src/NetUtil.cpp
    Network/src/SnapshotCodec.cpp
)
target_include_directories(leon_network PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_network PUBLIC leon_core enet)
if(WIN32)
    target_link_libraries(leon_network PUBLIC ws2_32 winmm)
endif()
add_library(Leon::Network ALIAS leon_network)

# --- Audio (Unreal-like AudioDevice; miniaudio backend) ---
add_library(leon_audio STATIC
    Audio/src/AudioDevice.cpp
)
target_include_directories(leon_audio PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_audio PUBLIC leon_core miniaudio)
add_library(Leon::Audio ALIAS leon_audio)

# --- Renderer (CPU helpers; GPU in Plugins/RHI/OpenGL) ---
# Shipping mesh path: primitives + `.lmesh` / `.lmat`. DCC import lives in leon_import.
add_library(leon_renderer STATIC
    Renderer/src/Frustum.cpp
    Renderer/src/LeonMaterialFormat.cpp
    Renderer/src/LeonMeshFormat.cpp
    Renderer/src/MaterialAsset.cpp
    Renderer/src/MeshData.cpp
    Renderer/src/Primitives.cpp
)
target_include_directories(leon_renderer PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_renderer PUBLIC
    leon_core
    leon_serialization
    glm::glm
    nlohmann_json::nlohmann_json
)
add_library(Leon::Renderer ALIAS leon_renderer)

# --- Import (edit-time / cook): OBJ FBX glTF → MeshData / `.lmesh` ---
# Not linked by leon_engine. Consumers: leon_editor, leon_engine_cook (+ tests).
add_library(leon_import STATIC
    Import/src/FbxStaticMesh.cpp
    Import/src/GltfImport.cpp
    Import/src/ObjImport.cpp
    Import/src/StaticMeshCook.cpp
)
target_include_directories(leon_import PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_import PUBLIC
    leon_renderer
    leon_core
    glm::glm
    tinyobjloader
    ufbx
    cgltf
    nlohmann_json::nlohmann_json
)
add_library(Leon::Import ALIAS leon_import)
# Only build when Editor / cook / tests link it — not when Projects build `all`.
set_property(TARGET leon_import PROPERTY EXCLUDE_FROM_ALL TRUE)

# --- Scene ---
add_library(leon_scene STATIC
    Scene/src/BasicShape.cpp
    Scene/src/BasicLight.cpp
    Scene/src/Level.cpp
    Scene/src/LevelCatalog.cpp
    Scene/src/LevelDirector.cpp
    Scene/src/LevelLoader.cpp
    Scene/src/LeonLevelFormat.cpp
    Scene/src/LightmapIO.cpp
)
target_include_directories(leon_scene PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_scene PUBLIC
    leon_core
    leon_renderer
    leon_assets
    leon_serialization
    nlohmann_json::nlohmann_json
)
add_library(Leon::Scene ALIAS leon_scene)

# --- Gameplay ---
add_library(leon_gameplay STATIC
    Gameplay/src/Actor.cpp
    Gameplay/src/ActorComponent.cpp
    Gameplay/src/AIController.cpp
    Gameplay/src/ArenaCamera.cpp
    Gameplay/src/Character.cpp
    Gameplay/src/Controller.cpp
    Gameplay/src/Damage.cpp
    Gameplay/src/DefaultGameMode.cpp
    Gameplay/src/DefaultPlayerController.cpp
    Gameplay/src/GameplayRouter.cpp
    Gameplay/src/GameInstance.cpp
    Gameplay/src/GameMode.cpp
    Gameplay/src/NavMesh.cpp
    Gameplay/src/NavigationSystem.cpp
    Gameplay/src/Pawn.cpp
    Gameplay/src/PlayerController.cpp
    Gameplay/src/SceneComponent.cpp
    Gameplay/src/SkeletalMeshComponent.cpp
    Gameplay/src/SpringArmComponent.cpp
    Gameplay/src/VolumeHelpers.cpp
    Gameplay/src/World.cpp
)
target_include_directories(leon_gameplay PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_gameplay PUBLIC
    leon_core
    leon_scene
    leon_animation
    leon_physics_iface
    leon_utilities
    leon_renderer
)
add_library(Leon::Gameplay ALIAS leon_gameplay)

# --- Engine shell ---
add_library(leon_engine_shell STATIC
    Gameplay/src/Engine.cpp
)
target_include_directories(leon_engine_shell PUBLIC ${_LEON_PUBLIC_INC})
target_link_libraries(leon_engine_shell PUBLIC
    leon_platform
    leon_gameplay
    leon_network
    leon_audio
    leon_utilities
    leon_scene
    leon_renderer
)
add_library(Leon::EngineShell ALIAS leon_engine_shell)

add_library(leon_engine_core INTERFACE)
target_link_libraries(leon_engine_core INTERFACE
    leon_core
    leon_platform
    leon_utilities
    leon_assets
    leon_animation
    leon_network
    leon_audio
    leon_renderer
    leon_scene
    leon_gameplay
    leon_engine_shell
    leon_serialization
)
add_library(Leon::EngineCore ALIAS leon_engine_core)

# --- Plugins ---
if(LEON_RHI STREQUAL "PS2")
    add_subdirectory("${LEON_REPO_ROOT}/Plugins/RHI/PS2" "${CMAKE_BINARY_DIR}/_leon_rhi_ps2")
    set(_LEON_RHI_TARGET leon_rhi_ps2)
else()
    add_subdirectory("${LEON_REPO_ROOT}/Plugins/RHI/OpenGL" "${CMAKE_BINARY_DIR}/_leon_rhi_opengl")
    set(_LEON_RHI_TARGET leon_rhi_opengl)
endif()
add_subdirectory("${LEON_REPO_ROOT}/Plugins/Physics/Arcade" "${CMAKE_BINARY_DIR}/_leon_physics_arcade")
if(LEON_WITH_JOLT)
    add_subdirectory("${LEON_REPO_ROOT}/Plugins/Physics/Jolt" "${CMAKE_BINARY_DIR}/_leon_physics_jolt")
    target_link_libraries(leon_physics_arcade PUBLIC leon_physics_jolt)
endif()

add_library(leon_engine INTERFACE)
target_link_libraries(leon_engine INTERFACE
    leon_engine_core
    ${_LEON_RHI_TARGET}
    leon_physics_arcade
)
if(LEON_WITH_JOLT)
    target_link_libraries(leon_engine INTERFACE leon_physics_jolt)
endif()
add_library(Leon::Engine ALIAS leon_engine)

# Offline cook / Tools — assets + animation + renderer + import + RHI cache (no gameplay/physics/net/shell).
add_library(leon_engine_cook INTERFACE)
target_link_libraries(leon_engine_cook INTERFACE
    leon_assets
    leon_animation
    leon_renderer
    leon_import
    ${_LEON_RHI_TARGET}
)
add_library(Leon::EngineCook ALIAS leon_engine_cook)

# Runtime is opt-in: Projects/Templates add_subdirectory(Runtime) after Engine.
# Tools do not build leon_runtime. Editor may add it for in-process PIE.

include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")

leon_apply_compile_options(
    leon_core
    leon_platform
    leon_utilities
    leon_serialization
    leon_assets
    leon_animation
    leon_network
    leon_audio
    leon_renderer
    leon_import
    leon_scene
    leon_gameplay
    leon_engine_shell
    ${_LEON_RHI_TARGET}
    leon_physics_arcade
)
if(LEON_WITH_JOLT AND TARGET leon_physics_jolt)
    leon_apply_compile_options(leon_physics_jolt)
endif()

if(LEON_BUILD_TESTS AND _LEON_ENGINE_IS_TOPLEVEL)
    add_subdirectory("${LEON_REPO_ROOT}/Tests" "${CMAKE_BINARY_DIR}/_leon_tests")
endif()
