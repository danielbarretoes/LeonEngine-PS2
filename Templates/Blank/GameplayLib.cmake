# Fixed Blank gameplay library (RegisterModes only — no rename on New Project).

if(TARGET leon_blank_gameplay)
    return()
endif()

if(NOT TARGET leon_engine)
    message(FATAL_ERROR "leon_blank_gameplay requires leon_engine")
endif()

set(_leon_blank_root "${CMAKE_CURRENT_LIST_DIR}")

add_library(leon_blank_gameplay STATIC
    "${_leon_blank_root}/src/RegisterModes.cpp"
)
target_include_directories(leon_blank_gameplay
    PUBLIC
        "${_leon_blank_root}/include"
)
target_link_libraries(leon_blank_gameplay PUBLIC leon_engine)
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${_leon_blank_root}/../.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_blank_gameplay)
if(MSVC)
    target_compile_definitions(leon_blank_gameplay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
