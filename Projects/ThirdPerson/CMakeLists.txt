# Projects/ThirdPerson — Unreal-style fixed Third Person sample pack (no {{NAME}} rename).
#   Scripts\build-project.bat Projects\ThirdPerson
#   cmake -S Projects/ThirdPerson -B Projects/ThirdPerson/build

cmake_minimum_required(VERSION 3.20)
project(LeonThirdPerson VERSION 0.1.0 LANGUAGES C CXX)

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

# Prefer -DLEON_REPO_ROOT=... (build-project.bat). Else walk up for Build/Dependencies.cmake
# (Projects/<name> or projects created beside the editor under build/*/Projects/).
if(NOT LEON_REPO_ROOT)
    set(_leon_probe "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(_i RANGE 0 12)
        if(EXISTS "${_leon_probe}/Build/Dependencies.cmake")
            set(LEON_REPO_ROOT "${_leon_probe}")
            break()
        endif()
        get_filename_component(_leon_probe "${_leon_probe}/.." ABSOLUTE)
    endforeach()
    unset(_leon_probe)
endif()
get_filename_component(LEON_REPO_ROOT "${LEON_REPO_ROOT}" ABSOLUTE)
if(NOT EXISTS "${LEON_REPO_ROOT}/Build/Dependencies.cmake")
    message(FATAL_ERROR
        "Leon Engine root not found (Build/Dependencies.cmake). "
        "Pass -DLEON_REPO_ROOT=<path> or place the project under <repo>/Projects/.")
endif()

set(LEON_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(LEON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# FORCE: stale CACHE from a wrong LEON_REPO_ROOT (e.g. Editor/build/Release) breaks POST_BUILD sync.
set(LEON_ENGINE_ASSETS "${LEON_REPO_ROOT}/Engine/Assets" CACHE PATH "Engine runtime assets root" FORCE)
set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "Projects root" FORCE)

include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")
add_subdirectory("${LEON_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")
add_subdirectory("${LEON_REPO_ROOT}/Runtime" "${CMAKE_BINARY_DIR}/_leon_runtime")
include("${CMAKE_CURRENT_SOURCE_DIR}/GameplayLib.cmake")

add_executable(leon-ThirdPerson
    main.cpp
)
target_link_libraries(leon-ThirdPerson PRIVATE leon_third_person_gameplay leon_runtime)

set(_leon_sync "${LEON_REPO_ROOT}/Build/SyncDirectory.cmake")
add_custom_command(TARGET leon-ThirdPerson POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${LEON_REPO_ROOT}/Engine/Assets
        -DDST=$<TARGET_FILE_DIR:leon-ThirdPerson>/assets
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Content/assets
        -DDST=$<TARGET_FILE_DIR:leon-ThirdPerson>/assets
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:leon-ThirdPerson>/Projects/ThirdPerson/Content/Levels
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:leon-ThirdPerson>/Projects/ThirdPerson/Content/materials
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/leon.game.json
        $<TARGET_FILE_DIR:leon-ThirdPerson>/Projects/ThirdPerson/leon.game.json
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Content/Levels
        $<TARGET_FILE_DIR:leon-ThirdPerson>/Projects/ThirdPerson/Content/Levels
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Content/materials
        $<TARGET_FILE_DIR:leon-ThirdPerson>/Projects/ThirdPerson/Content/materials
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Content/assets
        $<TARGET_FILE_DIR:leon-ThirdPerson>/Projects/ThirdPerson/Content/assets
)

if(MSVC)
    target_compile_options(leon-ThirdPerson PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(leon-ThirdPerson PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(leon-ThirdPerson PRIVATE -Wall -Wextra -Wpedantic)
endif()
