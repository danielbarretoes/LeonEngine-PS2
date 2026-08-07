# Idempotent CoopTp gameplay library (shipping exe + Editor PIE).
# Requires: leon_engine (and preferably leon_runtime for consumers that link both).

if(TARGET leon_coop_tp_gameplay)
    return()
endif()

if(NOT TARGET leon_engine)
    message(FATAL_ERROR "leon_coop_tp_gameplay requires leon_engine")
endif()

set(_leon_coop_tp_root "${CMAKE_CURRENT_LIST_DIR}")

add_library(leon_coop_tp_gameplay STATIC
    "${_leon_coop_tp_root}/src/RegisterModes.cpp"
    "${_leon_coop_tp_root}/src/CoopTpAnimInstance.cpp"
    "${_leon_coop_tp_root}/src/CoopTpCharacter.cpp"
    "${_leon_coop_tp_root}/src/CoopTpPlayerController.cpp"
    "${_leon_coop_tp_root}/src/CoopTpGameMode.cpp"
    "${_leon_coop_tp_root}/src/CoopMenuGameMode.cpp"
    "${_leon_coop_tp_root}/src/CoopLobbyGameMode.cpp"
)
target_include_directories(leon_coop_tp_gameplay
    PUBLIC
        "${_leon_coop_tp_root}/include"
    PRIVATE
        "${_leon_coop_tp_root}/src"
)
target_link_libraries(leon_coop_tp_gameplay PUBLIC leon_engine)
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${_leon_coop_tp_root}/../.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_coop_tp_gameplay)
if(MSVC)
    target_compile_definitions(leon_coop_tp_gameplay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
