# Offline Tools — AssetPipeline (leon-cook), ResourceTools, Cli.
#   cmake -S Tools -B Tools/build

cmake_minimum_required(VERSION 3.20)
project(LeonTools VERSION 0.10.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_cfg}" _cfg_up)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_BINARY_DIR}/${_cfg}")
    endforeach()
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

get_filename_component(LEON_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(LEON_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(LEON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEON_ENGINE_ASSETS "${LEON_REPO_ROOT}/Engine/Assets" CACHE PATH "Engine runtime assets root")

include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")
add_subdirectory("${LEON_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")

# Shared offline helpers (recipe runner, path resolve) — no Editor UI surface.
add_library(leon_resource_tools STATIC
    ResourceTools/src/CookPaths.cpp
    ResourceTools/src/CookRecipe.cpp
)
target_include_directories(leon_resource_tools PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ResourceTools/include>
)
target_link_libraries(leon_resource_tools PUBLIC leon_engine_cook nlohmann_json::nlohmann_json)
add_library(Leon::ResourceTools ALIAS leon_resource_tools)

# Offline cook — lean Engine cook facade (no gameplay / physics / network / shell).
add_executable(leon-cook
    AssetPipeline/leon-cook/main.cpp
)
target_link_libraries(leon-cook PRIVATE leon_resource_tools)

# Thin CLI — no Engine libraries (forwards to sibling leon-cook).
add_executable(leon-cli
    Cli/main.cpp
)

# Incremental asset sync (same helper as Editor POST_BUILD).
set(_leon_sync "${LEON_REPO_ROOT}/Build/SyncDirectory.cmake")
add_custom_command(TARGET leon-cook POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${LEON_ENGINE_ASSETS}
        -DDST=$<TARGET_FILE_DIR:leon-cook>/assets
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
    COMMENT "Sync Engine/Assets beside leon-cook"
)

if(MSVC)
    foreach(tgt leon_resource_tools leon-cook leon-cli)
        target_compile_options(${tgt} PRIVATE /W4 /permissive- /Zc:__cplusplus)
        target_compile_definitions(${tgt} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endforeach()
else()
    foreach(tgt leon_resource_tools leon-cook leon-cli)
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()
