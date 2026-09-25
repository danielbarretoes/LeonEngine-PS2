# LeonHeaderTool (UnrealHeaderTool counterpart): a std-only C++17 host program.
#
# Not a LeonBuildTool module: LeonBuildTool's System/HostTools.cmake configures this folder with the host compiler
# (MSVC on Windows, g++ in the ps2dev Docker image) into Engine/Intermediate/Build/HostTools/<Host> and builds it
# before configuring any target, then passes -DLEON_HEADER_TOOL=<path> to the target build tree.
cmake_minimum_required(VERSION 3.24)
project(LeonHeaderTool LANGUAGES CXX)

add_executable(LeonHeaderTool
	Private/CodeGenerator.cpp
	Private/CodeGenerator.h
	Private/Diagnostics.cpp
	Private/Diagnostics.h
	Private/HeaderParser.cpp
	Private/HeaderParser.h
	Private/LeonHeaderToolMain.cpp
	Private/Manifest.cpp
	Private/Manifest.h
	Private/Tokenizer.cpp
	Private/Tokenizer.h
	Private/TypeModel.cpp
	Private/TypeModel.h)

set_target_properties(LeonHeaderTool PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# Default folder of `LeonHeaderTool -Test`.
target_compile_definitions(LeonHeaderTool PRIVATE LHT_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests")
if(MSVC)
	target_compile_options(LeonHeaderTool PRIVATE /W4 /permissive- /utf-8 /EHsc /we4456 /we4457 /we4458 /we4459)
	target_compile_definitions(LeonHeaderTool PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(LeonHeaderTool PRIVATE -Wall -Wextra -Wshadow)
endif()
