# LeonBuildTool build-tree generator (the part of UnrealBuildTool that turns rules into a build).
#
# Do not configure this directly — use Engine/Build/BatchFiles/Build.bat (or LeonBuildTool.cmake):
#   Build.bat <Target> <Platform> <Configuration> [-Project=<path>.leonproject]
#
# Cache inputs: LEON_PLATFORM, LEON_CONFIGURATION (Debug|Development|Shipping), LEON_PROJECT_FILE.
# Without a project the tree contains every engine target (programs, LeonGame) for the platform.
cmake_minimum_required(VERSION 3.24)

get_filename_component(LEON_LBT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(LEON_ENGINE_DIR "${LEON_LBT_DIR}/../../.." ABSOLUTE)
get_filename_component(LEON_ROOT_DIR "${LEON_ENGINE_DIR}/.." ABSOLUTE)

include("${LEON_LBT_DIR}/System/PlatformRegistry.cmake")
leon_load_platforms("${LEON_ENGINE_DIR}")

get_property(_LeonPlatforms GLOBAL PROPERTY LEON_PLATFORMS)
if(NOT LEON_PLATFORM OR NOT LEON_PLATFORM IN_LIST _LeonPlatforms)
	message(FATAL_ERROR "LEON_PLATFORM must be one of: ${_LeonPlatforms} (got '${LEON_PLATFORM}')")
endif()
if(NOT LEON_CONFIGURATION)
	set(LEON_CONFIGURATION Development)
endif()
if(NOT LEON_CONFIGURATION MATCHES "^(Debug|Development|Shipping)$")
	message(FATAL_ERROR "LEON_CONFIGURATION must be Debug, Development or Shipping")
endif()

leon_platform_get(${LEON_PLATFORM} TOOLCHAIN_FILE _LeonToolchain)
if(_LeonToolchain AND NOT CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "${_LeonToolchain}")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Third-party CMake files that still declare cmake_minimum_required(< 3.5) (CMake 4 removed them).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Solution / build tree name: the project's name for game builds, LeonEngine otherwise (UE: UE4.sln / <Game>.sln).
if(LEON_PROJECT_FILE)
	get_filename_component(_LeonSolutionName "${LEON_PROJECT_FILE}" NAME_WE)
else()
	set(_LeonSolutionName LeonEngine)
endif()
project(${_LeonSolutionName} LANGUAGES C CXX)

include("${LEON_LBT_DIR}/Configuration/ModuleRules.cmake")
include("${LEON_LBT_DIR}/Configuration/TargetRules.cmake")
include("${LEON_LBT_DIR}/Configuration/CompileEnvironment.cmake")
include("${LEON_LBT_DIR}/Configuration/BuildTarget.cmake")
include("${LEON_LBT_DIR}/System/ModuleDiscovery.cmake")
include("${LEON_LBT_DIR}/System/ProjectDescriptor.cmake")
include("${LEON_LBT_DIR}/System/PluginDescriptor.cmake")
include("${LEON_LBT_DIR}/System/ThirdPartyDependencies.cmake")

# Platform folder names that do not apply here (UBT: a Windows/ folder never builds on PS2).
leon_all_platform_names(_LeonAllNames)
leon_platform_names(${LEON_PLATFORM} _LeonCurrentNames)
set(LEON_EXCLUDED_PLATFORM_FOLDERS ${_LeonAllNames})
list(REMOVE_ITEM LEON_EXCLUDED_PLATFORM_FOLDERS ${_LeonCurrentNames})

leon_init_compile_environment()

set(LEON_PROJECT_NAME "")
set(LEON_PROJECT_DIR "")
set(LEON_PROJECT_MODULES)
if(LEON_PROJECT_FILE)
	get_filename_component(LEON_PROJECT_FILE "${LEON_PROJECT_FILE}" ABSOLUTE)
	leon_read_project_descriptor("${LEON_PROJECT_FILE}")
endif()

# --- Discovery (plugins → modules → targets) ---
set(_LeonPluginRoots "${LEON_ENGINE_DIR}/Plugins")
if(LEON_PROJECT_DIR)
	list(APPEND _LeonPluginRoots "${LEON_PROJECT_DIR}/Plugins")
endif()
leon_discover_plugins(${_LeonPluginRoots})

leon_discover_modules("${LEON_ENGINE_DIR}/Source" Engine)
if(IS_DIRECTORY "${LEON_ENGINE_DIR}/Platforms/${LEON_PLATFORM}/Source")
	leon_discover_modules("${LEON_ENGINE_DIR}/Platforms/${LEON_PLATFORM}/Source" Platform)
endif()
get_property(_LeonPlugins GLOBAL PROPERTY LEON_PLUGINS)
foreach(_LeonPlugin IN LISTS _LeonPlugins)
	get_property(_LeonPluginDir GLOBAL PROPERTY LEON_PLUGIN_${_LeonPlugin}_DIR)
	if(IS_DIRECTORY "${_LeonPluginDir}/Source")
		leon_discover_modules("${_LeonPluginDir}/Source" "Plugin:${_LeonPlugin}")
	endif()
endforeach()
if(LEON_PROJECT_DIR AND IS_DIRECTORY "${LEON_PROJECT_DIR}/Source")
	leon_discover_modules("${LEON_PROJECT_DIR}/Source" Project)
endif()

# Platform extension folders merge into the engine module at the same relative path
# (Engine/Platforms/<P>/Source/Runtime/Core ↔ Engine/Source/Runtime/Core), with or without a
# <Module>_<P>.Build.cmake.
get_property(_LeonModules GLOBAL PROPERTY LEON_MODULES)
foreach(_LeonModule IN LISTS _LeonModules)
	leon_module_get(${_LeonModule} ORIGIN _LeonOrigin)
	leon_module_get(${_LeonModule} DIR _LeonDir)
	if(_LeonOrigin STREQUAL "Engine")
		file(RELATIVE_PATH _LeonRel "${LEON_ENGINE_DIR}/Source" "${_LeonDir}")
		set(_LeonExtDir "${LEON_ENGINE_DIR}/Platforms/${LEON_PLATFORM}/Source/${_LeonRel}")
		get_property(_LeonExtDirs GLOBAL PROPERTY LEON_MODULE_EXT_${_LeonModule}_DIRS)
		if(IS_DIRECTORY "${_LeonExtDir}" AND NOT _LeonExtDir IN_LIST _LeonExtDirs)
			set_property(GLOBAL APPEND PROPERTY LEON_MODULE_EXT_${_LeonModule}_DIRS "${_LeonExtDir}")
		endif()
	endif()
endforeach()

if(LEON_PROJECT_DIR)
	leon_discover_targets("${LEON_PROJECT_DIR}/Source/*.Target.cmake" ORIGIN Project)
else()
	leon_discover_targets(
		"${LEON_ENGINE_DIR}/Source/*.Target.cmake"
		"${LEON_ENGINE_DIR}/Source/Programs/*/*.Target.cmake"
		"${LEON_ENGINE_DIR}/Platforms/${LEON_PLATFORM}/Source/Programs/*/*.Target.cmake"
		ORIGIN Engine)
endif()

# --- Targets ---
enable_testing()
get_property(_LeonTargets GLOBAL PROPERTY LEON_TARGETS)
set(_LeonBuiltTargets)
foreach(_LeonTarget IN LISTS _LeonTargets)
	leon_target_allowed(${_LeonTarget} _LeonAllowed)
	if(_LeonAllowed)
		leon_build_target(${_LeonTarget})
		list(APPEND _LeonBuiltTargets ${_LeonTarget})
	endif()
endforeach()
if(NOT _LeonBuiltTargets)
	message(WARNING "LeonBuildTool: no target is available for ${LEON_PLATFORM}")
endif()
