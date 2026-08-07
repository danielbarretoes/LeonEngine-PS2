# Projects/CoopTp — LAN 2P coop (listen / join / dedicated).
#   Scripts\build-project.bat Projects\CoopTp
#   Scripts\build-project.bat Projects\CoopTp leon-CoopTp --with-server
#   leon-CoopTp.exe
#   leon-CoopTp-server.exe                 (headless dedicated default)
#   leon-CoopTp.exe --dedicated --port 7777

cmake_minimum_required(VERSION 3.20)
project(LeonCoopTp VERSION 0.1.0 LANGUAGES C CXX)

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
set(LEON_ENGINE_ASSETS "${LEON_REPO_ROOT}/Engine/Assets" CACHE PATH "Engine runtime assets root" FORCE)
set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "Projects root" FORCE)

include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")
add_subdirectory("${LEON_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")
add_subdirectory("${LEON_REPO_ROOT}/Runtime" "${CMAKE_BINARY_DIR}/_leon_runtime")
include("${CMAKE_CURRENT_SOURCE_DIR}/GameplayLib.cmake")

set(_coop_sources
    main.cpp
)

function(leon_coop_sync_runtime_assets target_name)
    set(_leon_sync "${LEON_REPO_ROOT}/Build/SyncDirectory.cmake")
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${LEON_REPO_ROOT}/Engine/Assets
            -DDST=$<TARGET_FILE_DIR:${target_name}>/assets
            -DREMOVE_ORPHANS=ON
            -P ${_leon_sync}
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Content/assets
            -DDST=$<TARGET_FILE_DIR:${target_name}>/assets
            -P ${_leon_sync}
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:${target_name}>/Projects/CoopTp/Content/Levels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:${target_name}>/Projects/CoopTp/Content/materials
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/leon.game.json
            $<TARGET_FILE_DIR:${target_name}>/Projects/CoopTp/leon.game.json
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Content/Levels
            -DDST=$<TARGET_FILE_DIR:${target_name}>/Projects/CoopTp/Content/Levels
            -DREMOVE_ORPHANS=ON
            -P ${_leon_sync}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Content/materials
            $<TARGET_FILE_DIR:${target_name}>/Projects/CoopTp/Content/materials
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Content/assets
            $<TARGET_FILE_DIR:${target_name}>/Projects/CoopTp/Content/assets
    )
endfunction()

function(leon_coop_configure_exe target_name)
    target_link_libraries(${target_name} PRIVATE leon_coop_tp_gameplay leon_runtime)
    leon_coop_sync_runtime_assets(${target_name})
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /Zc:__cplusplus)
        target_compile_definitions(${target_name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Client / listen host / join (windowed).
add_executable(leon-CoopTp ${_coop_sources})
leon_coop_configure_exe(leon-CoopTp)

# Headless dedicated shipping binary (LEON_DEDICATED_DEFAULT — still accepts --port / --tick).
add_executable(leon-CoopTp-server ${_coop_sources})
leon_coop_configure_exe(leon-CoopTp-server)
target_compile_definitions(leon-CoopTp-server PRIVATE LEON_DEDICATED_DEFAULT=1)

# Authoring helpers: stamp arenas / front-end maps.
add_executable(make-coop-arena tools/MakeCoopArena.cpp)
target_link_libraries(make-coop-arena PRIVATE leon_runtime)
add_executable(make-coop-levels tools/MakeCoopLevels.cpp)
target_link_libraries(make-coop-levels PRIVATE leon_runtime)
if(MSVC)
    target_compile_options(make-coop-arena PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(make-coop-arena PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(make-coop-levels PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(make-coop-levels PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
