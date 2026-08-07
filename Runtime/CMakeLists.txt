# Thin Runtime host library.
# Consumers (Projects / Templates / Editor PIE) add_subdirectory(Runtime) AFTER Engine so leon_engine exists.
# Tools do not pull this tree.

if(NOT TARGET leon_engine)
    message(FATAL_ERROR
        "leon_runtime requires leon_engine. "
        "add_subdirectory(Engine) before add_subdirectory(Runtime).")
endif()

add_library(leon_runtime STATIC
    Application/RunLeonGame.cpp
    Application/GameApplication.cpp
    Application/GameHostSession.cpp
    Project/ProjectPack.cpp
    WorldRuntime/WorldRuntime.cpp
)

target_include_directories(leon_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(leon_runtime PUBLIC leon_engine)

add_library(Leon::Runtime ALIAS leon_runtime)

# Same warning /MP flags as Engine modules.
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_runtime)
