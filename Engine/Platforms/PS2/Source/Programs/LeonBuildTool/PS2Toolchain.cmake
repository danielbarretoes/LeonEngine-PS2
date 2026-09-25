# CMake toolchain: PlayStation 2 Emotion Engine (ps2dev / PS2SDK).
# Registered as the PS2 platform's TOOLCHAIN_FILE (LeonBuildPS2.cmake); LeonBuildTool applies it inside the
# pinned ps2dev Docker image (PS2DEV set), e.g. Engine\Build\BatchFiles\Build.bat BlankProgram PS2 Development.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mips)

if(NOT DEFINED ENV{PS2DEV} OR "$ENV{PS2DEV}" STREQUAL "")
    message(FATAL_ERROR "PS2DEV is not set. Install ps2dev and export PS2DEV / PS2SDK (see Docs/SETUP.md).")
endif()
if(NOT DEFINED ENV{PS2SDK} OR "$ENV{PS2SDK}" STREQUAL "")
    message(FATAL_ERROR "PS2SDK is not set. Install ps2dev and export PS2DEV / PS2SDK (see Docs/SETUP.md).")
endif()

set(PS2DEV "$ENV{PS2DEV}")
set(PS2SDK "$ENV{PS2SDK}")
set(EE_PREFIX "mips64r5900el-ps2-elf-")

set(CMAKE_C_COMPILER   "${PS2DEV}/ee/bin/${EE_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${PS2DEV}/ee/bin/${EE_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${PS2DEV}/ee/bin/${EE_PREFIX}gcc")
set(CMAKE_AR           "${PS2DEV}/ee/bin/${EE_PREFIX}ar" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${PS2DEV}/ee/bin/${EE_PREFIX}ranlib" CACHE FILEPATH "" FORCE)
set(CMAKE_STRIP        "${PS2DEV}/ee/bin/${EE_PREFIX}strip" CACHE FILEPATH "" FORCE)

set(CMAKE_FIND_ROOT_PATH "${PS2DEV}/ee" "${PS2SDK}/ee")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Match PS2SDK samples/Makefile.eeglobal; one section per function / object so the linker drops unused code
# (-Wl,--gc-sections).
set(CMAKE_C_FLAGS_INIT   "-D_EE -G0 -O2 -Wall -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "-D_EE -G0 -O2 -Wall -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-T${PS2SDK}/ee/startup/linkfile -L${PS2SDK}/ee/lib -Wl,-zmax-page-size=128 -Wl,--gc-sections")

include_directories(SYSTEM "${PS2SDK}/ee/include" "${PS2SDK}/common/include")
link_directories("${PS2SDK}/ee/lib")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
