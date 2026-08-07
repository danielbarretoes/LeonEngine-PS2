# Idempotent Zombies gameplay library (shipping exe + Editor PIE).

if(TARGET leon_zombies_gameplay)
    return()
endif()

if(NOT TARGET leon_engine)
    message(FATAL_ERROR "leon_zombies_gameplay requires leon_engine")
endif()

set(_leon_zombies_root "${CMAKE_CURRENT_LIST_DIR}")

add_library(leon_zombies_gameplay STATIC
    "${_leon_zombies_root}/src/RegisterModes.cpp"
    "${_leon_zombies_root}/src/ZombiesCharacter.cpp"
    "${_leon_zombies_root}/src/ZombiesPlayerController.cpp"
    "${_leon_zombies_root}/src/ZombiesCrosshairWidget.cpp"
    "${_leon_zombies_root}/src/ZombiesMatchHudWidget.cpp"
    "${_leon_zombies_root}/src/ZombiesWeapon.cpp"
    "${_leon_zombies_root}/src/ZombiesInteract.cpp"
    "${_leon_zombies_root}/src/ZombiesGameMode.cpp"
    "${_leon_zombies_root}/src/ZombiesMenuGameMode.cpp"
    "${_leon_zombies_root}/src/ZombiesLobbyGameMode.cpp"
)
target_include_directories(leon_zombies_gameplay
    PUBLIC
        "${_leon_zombies_root}/include"
    PRIVATE
        "${_leon_zombies_root}/src"
)
target_link_libraries(leon_zombies_gameplay PUBLIC leon_engine)
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${_leon_zombies_root}/../.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_zombies_gameplay)
if(MSVC)
    target_compile_definitions(leon_zombies_gameplay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
