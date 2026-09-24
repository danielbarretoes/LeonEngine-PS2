# LeonBuildTool module rules (UnrealBuildTool: ModuleRules / <Module>.Build.cs).
#
#   leon_module(<Name>
#     [TYPE Runtime|Developer|Program|External]     # default from folder (Source/Runtime → Runtime, …)
#     [PLATFORMS <platform-or-group>...]            # allow-list; empty = every platform
#     [CXX_STANDARD <n>]                            # default: lowest standard of the allowed platforms
#     [NO_MODULE_IMPLEMENTATION]                    # no IMPLEMENT_MODULE (Program/External imply it)
#     [PUBLIC_DEPENDENCIES ...] [PRIVATE_DEPENDENCIES ...] [CIRCULAR_DEPENDENCIES ...]
#     [PUBLIC_DEFINITIONS ...] [PRIVATE_DEFINITIONS ...]
#     [PUBLIC_INCLUDE_PATHS ...] [PRIVATE_INCLUDE_PATHS ...]
#     [PUBLIC_SYSTEM_LIBRARIES ...] [EXCLUDE_SOURCES <glob relative to the module>...]
#     [EXTERNAL_TARGETS <cmake-target>...]          # External modules: targets made by LeonExternal_<Name>()
#     [DOWNLOAD_URL <url> DOWNLOAD_SHA256 <hash> DOWNLOAD_DIR <dir>])
#
# Every list keyword also accepts a _<Platform|Group> suffix, applied only on matching platforms:
#   PUBLIC_DEPENDENCIES_Desktop GLFW   PUBLIC_SYSTEM_LIBRARIES_Win64 Psapi   EXCLUDE_SOURCES_PS2 Private/Foo.cpp
#
#   leon_module_extend(<Name> <list keywords>...)   # platform extension: <Module>_<Platform>.Build.cmake
#
# External modules define `function(LeonExternal_<Name>)`, called lazily with LEON_THIRDPARTY_DIR and
# LEON_MODULE_DIR set, only when the module is part of a target's closure.

set(LEON_MODULE_LIST_KEYS
	PLATFORMS
	PUBLIC_DEPENDENCIES
	PRIVATE_DEPENDENCIES
	CIRCULAR_DEPENDENCIES
	PUBLIC_DEFINITIONS
	PRIVATE_DEFINITIONS
	PUBLIC_INCLUDE_PATHS
	PRIVATE_INCLUDE_PATHS
	PUBLIC_SYSTEM_LIBRARIES
	EXCLUDE_SOURCES
	EXTERNAL_TARGETS
	COMPILE_OPTIONS
)
set(LEON_MODULE_ONE_VALUE_KEYS TYPE CXX_STANDARD DOWNLOAD_URL DOWNLOAD_SHA256 DOWNLOAD_DIR)

set_property(GLOBAL PROPERTY LEON_MODULES "")

# Base list keywords plus every _<Platform|Group> variant known to the registry.
function(_leon_module_list_keywords OutVar)
	leon_all_platform_names(Suffixes)
	set(Keywords ${LEON_MODULE_LIST_KEYS})
	foreach(Key IN LISTS LEON_MODULE_LIST_KEYS)
		foreach(Suffix IN LISTS Suffixes)
			list(APPEND Keywords ${Key}_${Suffix})
		endforeach()
	endforeach()
	set(${OutVar} "${Keywords}" PARENT_SCOPE)
endfunction()

function(leon_module Name)
	_leon_module_list_keywords(ListKeywords)
	cmake_parse_arguments(M "NO_MODULE_IMPLEMENTATION" "${LEON_MODULE_ONE_VALUE_KEYS}" "${ListKeywords}" ${ARGN})
	if(M_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "leon_module(${Name}): unknown arguments: ${M_UNPARSED_ARGUMENTS} (${CMAKE_CURRENT_LIST_FILE})")
	endif()

	get_property(Existing GLOBAL PROPERTY LEON_MODULE_${Name}_FILE)
	if(Existing)
		message(FATAL_ERROR "Module '${Name}' is defined twice:\n  ${Existing}\n  ${CMAKE_CURRENT_LIST_FILE}")
	endif()

	if(NOT M_TYPE)
		set(M_TYPE "${_LEON_DEFAULT_MODULE_TYPE}")
	endif()
	if(NOT M_TYPE MATCHES "^(Runtime|Developer|Program|External)$")
		message(FATAL_ERROR "leon_module(${Name}): TYPE must be Runtime, Developer, Program or External (got '${M_TYPE}')")
	endif()

	set_property(GLOBAL APPEND PROPERTY LEON_MODULES ${Name})
	set_property(GLOBAL PROPERTY LEON_MODULE_${Name}_FILE "${CMAKE_CURRENT_LIST_FILE}")
	set_property(GLOBAL PROPERTY LEON_MODULE_${Name}_DIR "${CMAKE_CURRENT_LIST_DIR}")
	set_property(GLOBAL PROPERTY LEON_MODULE_${Name}_ORIGIN "${_LEON_MODULE_ORIGIN}")
	set_property(GLOBAL PROPERTY LEON_MODULE_${Name}_NO_MODULE_IMPLEMENTATION ${M_NO_MODULE_IMPLEMENTATION})
	foreach(Key IN LISTS LEON_MODULE_ONE_VALUE_KEYS)
		set_property(GLOBAL PROPERTY LEON_MODULE_${Name}_${Key} "${M_${Key}}")
	endforeach()
	foreach(Key IN LISTS ListKeywords)
		if(DEFINED M_${Key})
			set_property(GLOBAL PROPERTY LEON_MODULE_${Name}_${Key} "${M_${Key}}")
		endif()
	endforeach()
endfunction()

function(leon_module_extend Name)
	_leon_module_list_keywords(ListKeywords)
	cmake_parse_arguments(M "" "" "${ListKeywords}" ${ARGN})
	if(M_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "leon_module_extend(${Name}): unknown arguments: ${M_UNPARSED_ARGUMENTS}")
	endif()
	set_property(GLOBAL APPEND PROPERTY LEON_MODULE_EXT_${Name}_DIRS "${CMAKE_CURRENT_LIST_DIR}")
	foreach(Key IN LISTS ListKeywords)
		if(DEFINED M_${Key})
			set_property(GLOBAL APPEND PROPERTY LEON_MODULE_EXT_${Name}_${Key} ${M_${Key}})
		endif()
	endforeach()
endfunction()

function(leon_module_get Name Key OutVar)
	get_property(Value GLOBAL PROPERTY LEON_MODULE_${Name}_${Key})
	set(${OutVar} "${Value}" PARENT_SCOPE)
endfunction()

function(leon_module_exists Name OutVar)
	get_property(File GLOBAL PROPERTY LEON_MODULE_${Name}_FILE)
	if(File)
		set(${OutVar} TRUE PARENT_SCOPE)
	else()
		set(${OutVar} FALSE PARENT_SCOPE)
	endif()
endfunction()

# Base + extension + suffixed values that apply to <Platform> (defaults to LEON_PLATFORM).
function(leon_module_effective Name Key OutVar)
	set(Platform "${LEON_PLATFORM}")
	if(ARGC GREATER 3)
		set(Platform "${ARGV3}")
	endif()
	leon_platform_names(${Platform} Suffixes)
	get_property(Base GLOBAL PROPERTY LEON_MODULE_${Name}_${Key})
	get_property(Ext GLOBAL PROPERTY LEON_MODULE_EXT_${Name}_${Key})
	set(Result ${Base} ${Ext})
	foreach(Suffix IN LISTS Suffixes)
		get_property(Base GLOBAL PROPERTY LEON_MODULE_${Name}_${Key}_${Suffix})
		get_property(Ext GLOBAL PROPERTY LEON_MODULE_EXT_${Name}_${Key}_${Suffix})
		list(APPEND Result ${Base} ${Ext})
	endforeach()
	set(${OutVar} "${Result}" PARENT_SCOPE)
endfunction()

# TRUE when the module's PLATFORMS allow-list accepts <Platform> (defaults to LEON_PLATFORM).
function(leon_module_allowed Name OutVar)
	set(Platform "${LEON_PLATFORM}")
	if(ARGC GREATER 2)
		set(Platform "${ARGV2}")
	endif()
	get_property(Allowed GLOBAL PROPERTY LEON_MODULE_${Name}_PLATFORMS)
	if(NOT Allowed)
		set(${OutVar} TRUE PARENT_SCOPE)
		return()
	endif()
	leon_platform_names(${Platform} Names)
	foreach(Entry IN LISTS Allowed)
		if(Entry IN_LIST Names)
			set(${OutVar} TRUE PARENT_SCOPE)
			return()
		endif()
	endforeach()
	set(${OutVar} FALSE PARENT_SCOPE)
endfunction()
