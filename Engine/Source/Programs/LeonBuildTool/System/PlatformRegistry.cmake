# LeonBuildTool platform registry (UnrealBuildTool: UEBuildPlatform / UnrealTargetPlatform).
#
# Each platform registers itself from Platform/<Name>/LeonBuild<Name>.cmake or, for platform
# extensions, from Engine/Platforms/<Name>/Source/Programs/LeonBuildTool/LeonBuild<Name>.cmake.
# Script-mode safe (no project() needed).

set_property(GLOBAL PROPERTY LEON_PLATFORMS "")

set(_LEON_PLATFORM_ONE_VALUE_KEYS
	HEADER_NAME
	TOOLCHAIN_FILE
	CXX_STANDARD
	EXECUTABLE_SUFFIX
	RHI_MODULE
	HOST_SYSTEM
	DOCKER_IMAGE
	DOCKER_ENTRY
	SDK_ENV
	BUILD_TYPE_Debug
	BUILD_TYPE_Development
	BUILD_TYPE_Shipping
)
set(_LEON_PLATFORM_MULTI_VALUE_KEYS GROUPS DEFINITIONS)

# leon_register_platform(<Name> GROUPS <group>... HEADER_NAME <dir> [IS_EXTENSION] ...)
function(leon_register_platform Name)
	cmake_parse_arguments(P "IS_EXTENSION" "${_LEON_PLATFORM_ONE_VALUE_KEYS}" "${_LEON_PLATFORM_MULTI_VALUE_KEYS}" ${ARGN})
	set_property(GLOBAL APPEND PROPERTY LEON_PLATFORMS ${Name})
	foreach(Key IN LISTS _LEON_PLATFORM_ONE_VALUE_KEYS _LEON_PLATFORM_MULTI_VALUE_KEYS)
		set_property(GLOBAL PROPERTY LEON_PLATFORM_${Name}_${Key} "${P_${Key}}")
	endforeach()
	set_property(GLOBAL PROPERTY LEON_PLATFORM_${Name}_IS_EXTENSION ${P_IS_EXTENSION})
endfunction()

function(leon_platform_get Name Key OutVar)
	get_property(Value GLOBAL PROPERTY LEON_PLATFORM_${Name}_${Key})
	set(${OutVar} "${Value}" PARENT_SCOPE)
endfunction()

# Platform name + its groups (+ header folder name): the folder/suffix names that apply to it.
function(leon_platform_names Name OutVar)
	leon_platform_get(${Name} GROUPS Groups)
	leon_platform_get(${Name} HEADER_NAME HeaderName)
	set(Names ${Name} ${Groups} ${HeaderName})
	list(REMOVE_DUPLICATES Names)
	set(${OutVar} "${Names}" PARENT_SCOPE)
endfunction()

# Every platform and group name known to the registry (candidate platform folder names).
function(leon_all_platform_names OutVar)
	get_property(Platforms GLOBAL PROPERTY LEON_PLATFORMS)
	set(All)
	foreach(Platform IN LISTS Platforms)
		leon_platform_names(${Platform} Names)
		list(APPEND All ${Names})
	endforeach()
	list(REMOVE_DUPLICATES All)
	set(${OutVar} "${All}" PARENT_SCOPE)
endfunction()

# Include Platform/*/LeonBuild*.cmake and every platform extension's LeonBuild*.cmake.
macro(leon_load_platforms EngineDir)
	file(GLOB _LeonPlatformFiles
		"${EngineDir}/Source/Programs/LeonBuildTool/Platform/*/LeonBuild*.cmake"
		"${EngineDir}/Platforms/*/Source/Programs/LeonBuildTool/LeonBuild*.cmake")
	foreach(_LeonPlatformFile IN LISTS _LeonPlatformFiles)
		include("${_LeonPlatformFile}")
	endforeach()
endmacro()
