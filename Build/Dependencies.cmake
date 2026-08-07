# Third-party dependencies fetched at configure time.
# Profiles: HOST (GLFW/GL/Jolt/…) vs PS2 (math headers only).
include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/LeonPlatform.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/LeonCompileOptions.cmake")

get_filename_component(_LEON_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_LEON_TP "${_LEON_REPO_ROOT}/ThirdParty")

if(LEON_PLATFORM_PS2)
    # Lean EE: no FetchContent (GLM/GLFW/Jolt…). Host cooks assets offline.
    return()
endif()

# --- GLM (headers) ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _leon_skip_cmake
)
FetchContent_MakeAvailable(glm)
if(NOT TARGET glm::glm)
    add_library(leon_glm INTERFACE)
    add_library(glm::glm ALIAS leon_glm)
    target_include_directories(leon_glm INTERFACE ${glm_SOURCE_DIR})
endif()

# ===== HOST profile =====

# --- GLFW (window / input / OpenGL context) ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(glfw)

# --- tinyobjloader (Wavefront OBJ importer) ---
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
set(TINYOBJLOADER_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinyobjloader)

# --- nlohmann_json (scene files) ---
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# --- GLAD (vendored OpenGL 3.3 core loader) ---
add_library(glad STATIC
    ${_LEON_TP}/glad/src/glad.c
)
target_include_directories(glad PUBLIC
    ${_LEON_TP}/glad/include
)
leon_enable_msvc_mp(glad)

# --- ufbx (vendored FBX loader for skeletal meshes / animation) ---
add_library(ufbx STATIC
    ${_LEON_TP}/ufbx/ufbx.c
)
target_include_directories(ufbx PUBLIC
    ${_LEON_TP}/ufbx
)
if(MSVC)
    target_compile_options(ufbx PRIVATE /W0)
else()
    target_compile_options(ufbx PRIVATE -w)
endif()
leon_enable_msvc_mp(ufbx)

# --- cgltf (header-only glTF 2.0 loader) ---
add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${_LEON_TP}/cgltf)
add_library(Leon::cgltf ALIAS cgltf)

# --- ENet (UDP multiplayer LAN) ---
set(ENET_DIR ${_LEON_TP}/enet)
add_library(enet STATIC
    ${ENET_DIR}/callbacks.c
    ${ENET_DIR}/compress.c
    ${ENET_DIR}/host.c
    ${ENET_DIR}/list.c
    ${ENET_DIR}/packet.c
    ${ENET_DIR}/peer.c
    ${ENET_DIR}/protocol.c
    ${ENET_DIR}/unix.c
    ${ENET_DIR}/win32.c
)
target_include_directories(enet PUBLIC ${ENET_DIR}/include)
if(MSVC)
    target_compile_options(enet PRIVATE /W0)
    target_compile_definitions(enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    target_compile_options(enet PRIVATE -w)
endif()
leon_enable_msvc_mp(enet)

# --- miniaudio ---
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()
if(NOT TARGET miniaudio)
    add_library(miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
    target_include_directories(miniaudio PUBLIC ${miniaudio_SOURCE_DIR})
    if(MSVC)
        target_compile_options(miniaudio PRIVATE /W0)
    else()
        target_compile_options(miniaudio PRIVATE -w)
    endif()
    if(WIN32)
        target_link_libraries(miniaudio PUBLIC winmm)
    endif()
    leon_enable_msvc_mp(miniaudio)
endif()
add_library(Leon::miniaudio ALIAS miniaudio)

# --- Jolt Physics (optional) ---
option(LEON_WITH_JOLT "Fetch and build Jolt Physics backend" ON)
if(LEON_WITH_JOLT)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER_SAMPLES OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.3.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(JoltPhysics)
    if(TARGET Jolt)
        add_library(Leon::Jolt ALIAS Jolt)
    endif()
endif()

# --- Catch2 ---
if(LEON_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# --- Dear ImGui + ImGuizmo (editor) ---
if(LEON_BUILD_CLIENT)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.8-docking
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw glad)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    if(MSVC)
        target_compile_options(imgui PRIVATE /W0)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()
    leon_enable_msvc_mp(imgui)

    FetchContent_Declare(
        imguizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG 1.10
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _leon_skip_cmake
    )
    FetchContent_MakeAvailable(imguizmo)

    add_library(imguizmo STATIC
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
    )
    target_include_directories(imguizmo PUBLIC ${imguizmo_SOURCE_DIR}/src)
    target_link_libraries(imguizmo PUBLIC imgui)
    if(MSVC)
        target_compile_options(imguizmo PRIVATE /W0)
    else()
        target_compile_options(imguizmo PRIVATE -w)
    endif()
    leon_enable_msvc_mp(imguizmo)
endif()
