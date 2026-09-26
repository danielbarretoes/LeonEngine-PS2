# Global compile environment for Leon modules and targets (UnrealBuildTool: CppCompileEnvironment).
#
# Every Leon module (not External) and every executable gets:
#   PLATFORM_<P>=1 (+ platform definitions), LBT_COMPILED_PLATFORM=<HeaderName>, PLATFORM_IS_EXTENSION,
#   IS_MONOLITHIC=1, WITH_EDITOR=0, LEON_BUILD_<CONFIGURATION>=1, ENGINE_{MAJOR,MINOR,PATCH}_VERSION,
#   LEON_ENGINE_DIR="<abs>" (development fallback for FPaths).
# Target-level macros (WITH_ENGINE, IS_PROGRAM, …) only reach the launch module compiled into the
# executable — shared module libraries are identical for every target of a build tree.
#
# Leon code builds without RTTI and without C++ exceptions on every platform (plan decision D17; UE's defaults,
# bUseRTTI and bEnableExceptions false): casts go through UObject reflection (Cast<>) and failures through check /
# ensure. MSVC: /GR- and no /EH flag, with _HAS_EXCEPTIONS=0 so the STL does not throw either. GCC / Clang:
# -fno-rtti -fno-exceptions (the PS2 toolchain file already passes them to everything). Third-party code keeps its
# own flags: CMake's MSVC defaults (/EHsc, /GR) are stripped from CMAKE_CXX_FLAGS for the whole tree, and third-party
# C++ that needs them gets them back with leon_third_party_cxx_defaults (Jolt sets its own: no exceptions, no RTTI).

function(leon_read_build_version)
	set(File "${LEON_ENGINE_DIR}/Build/Build.version")
	set(Major 0)
	set(Minor 0)
	set(Patch 0)
	if(EXISTS "${File}")
		# A version bump re-runs the configure step, so existing build trees pick up the new ENGINE_*_VERSION.
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${File}")
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

	# CMake's MSVC defaults turn exceptions (/EHsc) and RTTI (/GR, before CMake 3.20) on for every target; Leon
	# targets add their own flags below, third-party ones call leon_third_party_cxx_defaults.
	if(MSVC)
		string(REGEX REPLACE "/EH[a-z]+" "" CxxFlags "${CMAKE_CXX_FLAGS}")
		string(REGEX REPLACE "/GR-?" "" CxxFlags "${CxxFlags}")
		string(REGEX REPLACE "  +" " " CxxFlags "${CxxFlags}")
		string(STRIP "${CxxFlags}" CxxFlags)
		set(CMAKE_CXX_FLAGS "${CxxFlags}" PARENT_SCOPE)
	endif()
endfunction()

# Third-party C++ built by its own CMake files keeps the compiler defaults Leon code turns off: C++ exceptions and
# RTTI (MSVC /EHsc /GR; GCC and Clang have both on by default).
function(leon_third_party_cxx_defaults Target)
	if(MSVC)
		target_compile_options(${Target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHsc /GR>)
	endif()
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
			# Shadowed locals / parameters / members are errors (UE: ShadowVariableWarningLevel = Error).
			/we4456 /we4457 /we4458 /we4459
			# Padding added for alignas (FMatrix, FVector4) is expected (UE disables C4324 the same way).
			/wd4324
			# No RTTI and no C++ exceptions (D17): no /EH flag, and 'noexcept' without an exception model is fine.
			/GR-
			/wd4577
			$<$<CONFIG:Debug,RelWithDebInfo>:/FS>)
		target_compile_definitions(${Target} PRIVATE _HAS_EXCEPTIONS=0)
	elseif(LEON_PLATFORM STREQUAL "PS2")
		# Shadowing is an error like on MSVC (UE: ShadowVariableWarningLevel = Error). The EE FPU is single precision:
		# an implicit float to double promotion goes through soft-float, so it is an error too.
		target_compile_options(${Target} PRIVATE -Wall -Wextra -Werror=shadow -Werror=double-promotion)
	else()
		# A GCC / Clang host (Linux, a development convenience): the PS2's warnings (shadowing is an error, as on MSVC),
		# no RTTI and no C++ exceptions (D17). No -Wpedantic: UE's checkf / UE_LOG style macros pass an empty
		# __VA_ARGS__, which C++17 pedantic mode reports on every use.
		target_compile_options(${Target} PRIVATE -Wall -Wextra -Werror=shadow
			$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions>)
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
