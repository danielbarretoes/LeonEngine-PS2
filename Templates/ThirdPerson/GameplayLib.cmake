# Fixed ThirdPerson gameplay library (template seed — class names do not rename on New Project).

if(TARGET leon_third_person_gameplay)
    return()
endif()

if(NOT TARGET leon_engine)
    message(FATAL_ERROR "leon_third_person_gameplay requires leon_engine")
endif()

set(_leon_third_person_root "${CMAKE_CURRENT_LIST_DIR}")

add_library(leon_third_person_gameplay STATIC
    "${_leon_third_person_root}/src/RegisterModes.cpp"
    "${_leon_third_person_root}/src/ThirdPersonAnimInstance.cpp"
    "${_leon_third_person_root}/src/ThirdPersonCharacter.cpp"
    "${_leon_third_person_root}/src/ThirdPersonPlayerController.cpp"
    "${_leon_third_person_root}/src/ThirdPersonGameMode.cpp"
)
target_include_directories(leon_third_person_gameplay
    PUBLIC
        "${_leon_third_person_root}/include"
    PRIVATE
        "${_leon_third_person_root}/src"
)
target_link_libraries(leon_third_person_gameplay PUBLIC leon_engine)
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${_leon_third_person_root}/../.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_third_person_gameplay)
if(MSVC)
    target_compile_definitions(leon_third_person_gameplay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
