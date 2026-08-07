# Idempotent Furytoon gameplay library (shipping exe + Editor PIE).

if(TARGET leon_furytoon_gameplay)
    return()
endif()

if(NOT TARGET leon_engine)
    message(FATAL_ERROR "leon_furytoon_gameplay requires leon_engine")
endif()

set(_leon_furytoon_root "${CMAKE_CURRENT_LIST_DIR}")

add_library(leon_furytoon_gameplay STATIC
    "${_leon_furytoon_root}/src/RegisterModes.cpp"
    "${_leon_furytoon_root}/src/FurytoonCharacter.cpp"
    "${_leon_furytoon_root}/src/FurytoonPlayerController.cpp"
    "${_leon_furytoon_root}/src/FurytoonGameMode.cpp"
    "${_leon_furytoon_root}/src/FurytoonMenuGameMode.cpp"
    "${_leon_furytoon_root}/src/FurytoonLobbyGameMode.cpp"
)
target_include_directories(leon_furytoon_gameplay
    PUBLIC
        "${_leon_furytoon_root}/include"
    PRIVATE
        "${_leon_furytoon_root}/src"
)
target_link_libraries(leon_furytoon_gameplay PUBLIC leon_engine)
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${_leon_furytoon_root}/../.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_furytoon_gameplay)
if(MSVC)
    target_compile_definitions(leon_furytoon_gameplay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
