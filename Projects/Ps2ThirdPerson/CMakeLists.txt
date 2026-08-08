# Projects/Ps2ThirdPerson — PS2 EE third-person gameplay starter (primitives level).
#   Scripts/build-ps2.sh tp
#   Scripts/build-ps2-docker.ps1 tp

cmake_minimum_required(VERSION 3.20)
project(LeonPs2ThirdPerson VERSION 0.1.0 LANGUAGES C CXX)

if(NOT LEON_REPO_ROOT)
    set(_leon_probe "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(_i RANGE 0 12)
        if(EXISTS "${_leon_probe}/Build/Dependencies.cmake")
            set(LEON_REPO_ROOT "${_leon_probe}")
            break()
        endif()
        get_filename_component(_leon_probe "${_leon_probe}/.." ABSOLUTE)
    endforeach()
endif()
get_filename_component(LEON_REPO_ROOT "${LEON_REPO_ROOT}" ABSOLUTE)

set(LEON_PLATFORM "PS2" CACHE STRING "" FORCE)
set(LEON_RHI "PS2" CACHE STRING "" FORCE)
set(LEON_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(LEON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEON_WITH_JOLT OFF CACHE BOOL "" FORCE)
set(LEON_ENGINE_ASSETS "${LEON_REPO_ROOT}/Engine/Assets" CACHE PATH "" FORCE)
set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")
add_subdirectory("${LEON_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")

add_executable(leon-Ps2ThirdPerson
    main.cpp
    Ps2ThirdPersonDemo.cpp
)
set_target_properties(leon-Ps2ThirdPerson PROPERTIES OUTPUT_NAME "leon-Ps2ThirdPerson" SUFFIX ".elf")
target_link_libraries(leon-Ps2ThirdPerson PRIVATE leon_engine)
