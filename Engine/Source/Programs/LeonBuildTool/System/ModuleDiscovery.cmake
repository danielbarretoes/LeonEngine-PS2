# LeonBuildTool module/target discovery (UnrealBuildTool: RulesCompiler).
#
# Finds every <Module>.Build.cmake / <Target>.Target.cmake under a root and includes it, so the
# leon_module()/leon_target() calls register themselves. Skips generated folders and downloaded
# third-party source trees (ThirdParty/<Lib>/<lib>-<version>/).

# file(GLOB ... CONFIGURE_DEPENDS) is only valid while configuring a build tree (not in cmake -P).
if(CMAKE_SCRIPT_MODE_FILE)
	set(_LEON_GLOB_DEPENDS "")
else()
	set(_LEON_GLOB_DEPENDS CONFIGURE_DEPENDS)
endif()

function(_leon_is_skipped_path Path OutVar)
	if(Path MATCHES "/(Intermediate|Binaries|Saved)/"
		OR Path MATCHES "/ThirdParty/[^/]+/[^/]+-[0-9][^/]*/")
		set(${OutVar} TRUE PARENT_SCOPE)
	else()
		set(${OutVar} FALSE PARENT_SCOPE)
	endif()
endfunction()

# Default module type from its folder (Source/Runtime → Runtime, Source/Editor → Editor, …); project modules are
# Runtime.
function(_leon_default_module_type File OutVar)
	if(File MATCHES "/Source/ThirdParty/")
		set(${OutVar} External PARENT_SCOPE)
	elseif(File MATCHES "/Source/Developer/")
		set(${OutVar} Developer PARENT_SCOPE)
	elseif(File MATCHES "/Source/Editor/")
		set(${OutVar} Editor PARENT_SCOPE)
	elseif(File MATCHES "/Source/Programs/")
		set(${OutVar} Program PARENT_SCOPE)
	else()
		set(${OutVar} Runtime PARENT_SCOPE)
	endif()
endfunction()

# leon_discover_modules(<Root> <Origin>)  Origin: Engine | Platform | Plugin:<Name> | Project
macro(leon_discover_modules Root Origin)
	file(GLOB_RECURSE _LeonBuildFiles ${_LEON_GLOB_DEPENDS} "${Root}/*.Build.cmake")
	list(SORT _LeonBuildFiles)
	foreach(_LeonBuildFile IN LISTS _LeonBuildFiles)
		_leon_is_skipped_path("${_LeonBuildFile}" _LeonSkip)
		if(NOT _LeonSkip)
			_leon_default_module_type("${_LeonBuildFile}" _LEON_DEFAULT_MODULE_TYPE)
			set(_LEON_MODULE_ORIGIN "${Origin}")
			include("${_LeonBuildFile}")
		endif()
	endforeach()
endmacro()

# leon_discover_targets(<Glob>... ORIGIN <Origin>)
macro(leon_discover_targets)
	cmake_parse_arguments(_LeonDT "" "ORIGIN" "" ${ARGN})
	file(GLOB _LeonTargetFiles ${_LEON_GLOB_DEPENDS} ${_LeonDT_UNPARSED_ARGUMENTS})
	list(SORT _LeonTargetFiles)
	foreach(_LeonTargetFile IN LISTS _LeonTargetFiles)
		set(_LEON_TARGET_ORIGIN "${_LeonDT_ORIGIN}")
		include("${_LeonTargetFile}")
	endforeach()
endmacro()
