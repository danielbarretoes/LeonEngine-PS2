# Global compile environment for Leon modules and targets (UnrealBuildTool: CppCompileEnvironment).
#
# Every Leon module (not External) and every executable gets:
#   PLATFORM_<P>=1 (+ platform definitions), LBT_COMPILED_PLATFORM=<HeaderName>, PLATFORM_IS_EXTENSION,
#   IS_MONOLITHIC=1, WITH_EDITOR=0, LEON_BUILD_<CONFIGURATION>=1, ENGINE_{MAJOR,MINOR,PATCH}_VERSION,
#   LEON_ENGINE_DIR="<abs>" (development fallback for FPaths).
# Target-level macros (WITH_ENGINE, IS_PROGRAM, …) only reach the launch module compiled into the
# executable — shared module libraries are identical for every target of a build tree.

function(leon_read_build_version)
	set(File "${LEON_ENGINE_DIR}/Build/Build.version")
	set(Major 0)
	set(Minor 0)
	set(Patch 0)
	if(EXISTS "${File}")
		file(READ "${File}" Json)
		string(JSON Major ERROR_VARIABLE Error GET "${Json}" MajorVersion)
		string(JSON Minor ERROR_VARIABLE Error GET "${Json}" MinorVersion)
		string(JSON Patch ERROR_VARIABLE Error GET "${Json}" PatchVersion)
	endif()
	set(LEON_ENGINE_VERSION_MAJOR ${Major} PARENT_SCOPE)
	set(LEON_ENGINE_VERSION_MINOR ${Minor} PARENT_SCOPE)
	set(LEON_ENGINE_VERSION_PATCH ${Patch} PARENT_SCOPE)
endfunction()

function(leon_init_compile_environment)
	leon_platform_get(${LEON_PLATFORM} HEADER_NAME HeaderName)
	leon_platform_get(${LEON_PLATFORM} DEFINITIONS PlatformDefinitions)
	leon_platform_get(${LEON_PLATFORM} IS_EXTENSION IsExtension)
	if(IsExtension)
		set(IsExtension 1)
	else()
		set(IsExtension 0)
	endif()
	string(TOUPPER "${LEON_CONFIGURATION}" ConfigUpper)
	leon_read_build_version()

	set(Definitions
		${PlatformDefinitions}
		LBT_COMPILED_PLATFORM=${HeaderName}
		PLATFORM_IS_EXTENSION=${IsExtension}
		IS_MONOLITHIC=1
		WITH_EDITOR=0
		LEON_BUILD_${ConfigUpper}=1
		ENGINE_MAJOR_VERSION=${LEON_ENGINE_VERSION_MAJOR}
		ENGINE_MINOR_VERSION=${LEON_ENGINE_VERSION_MINOR}
		ENGINE_PATCH_VERSION=${LEON_ENGINE_VERSION_PATCH}
		LEON_ENGINE_DIR="${LEON_ENGINE_DIR}"
	)
	set_property(GLOBAL PROPERTY LEON_GLOBAL_DEFINITIONS "${Definitions}")
endfunction()

# Warnings / language settings for a Leon-owned target.
function(leon_apply_compile_environment Target CxxStandard)
	get_property(Definitions GLOBAL PROPERTY LEON_GLOBAL_DEFINITIONS)
	target_compile_definitions(${Target} PRIVATE ${Definitions})
	set_target_properties(${Target} PROPERTIES
		CXX_STANDARD ${CxxStandard}
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF)
	if(MSVC)
		target_compile_options(${Target} PRIVATE
			/W4
			/permissive-
			/Zc:__cplusplus
			/utf-8
			/MP
			$<$<CONFIG:Debug,RelWithDebInfo>:/FS>)
	elseif(LEON_PLATFORM STREQUAL "PS2")
		target_compile_options(${Target} PRIVATE -Wall -Wextra)
	else()
		target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic)
	endif()
endfunction()

# Lowest C++ standard among the platforms the module is allowed on (shared code must compile on all).
function(leon_module_cxx_standard Name OutVar)
	leon_module_get(${Name} CXX_STANDARD Explicit)
	if(Explicit)
		set(${OutVar} ${Explicit} PARENT_SCOPE)
		return()
	endif()
	leon_platform_get(${LEON_PLATFORM} CXX_STANDARD Standard)
	get_property(Platforms GLOBAL PROPERTY LEON_PLATFORMS)
	foreach(Platform IN LISTS Platforms)
		leon_module_allowed(${Name} Allowed ${Platform})
		if(Allowed)
			leon_platform_get(${Platform} CXX_STANDARD PlatformStandard)
			if(PlatformStandard LESS Standard)
				set(Standard ${PlatformStandard})
			endif()
		endif()
	endforeach()
	set(${OutVar} ${Standard} PARENT_SCOPE)
endfunction()
