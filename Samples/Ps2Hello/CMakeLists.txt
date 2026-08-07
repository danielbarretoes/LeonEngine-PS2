# Toolchain-only smoke (no Engine). Validate ps2dev before Leon Ps2Lab.
#   Scripts/build-ps2.sh hello

cmake_minimum_required(VERSION 3.20)
project(LeonPs2Hello C)

if(NOT DEFINED ENV{PS2SDK})
    message(FATAL_ERROR "PS2SDK not set")
endif()

add_executable(leon-Ps2Hello main.c)
set_target_properties(leon-Ps2Hello PROPERTIES OUTPUT_NAME "leon-Ps2Hello" SUFFIX ".elf")
# Link like PS2SDK samples (crt0 comes from the toolchain defaults + linkfile).
target_link_libraries(leon-Ps2Hello PRIVATE kernel c)
