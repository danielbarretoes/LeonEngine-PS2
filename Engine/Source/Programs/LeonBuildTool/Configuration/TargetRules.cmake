# LeonBuildTool target rules (UnrealBuildTool: TargetRules / <Target>.Target.cs).
#
#   leon_target(<Name> TYPE Game|Program
#     [PLATFORMS <platform-or-group>...]        # allow-list; empty = every platform
#     [LAUNCH_MODULE <Module>]                   # module compiled into the executable (owns main)
#                                                #   Game default: Launch; Program default: <Name>
#     [EXTRA_MODULE_NAMES <Module>...]           # Game default: the project's modules
#     [ENABLE_PLUGINS <Plugin>...] [DISABLE_PLUGINS <Plugin>...]
#     [COMPILE_AGAINST_ENGINE ON|OFF]            # WITH_ENGINE for the launch module (Game ON, Program OFF)
#     [COLLECT_AUTOMATION_TESTS]                 # compile <Module>/Private/Tests/** of the closure in
#     [OUTPUT_NAME <name>])

set_property(GLOBAL PROPERTY LEON_TARGETS "")

function(leon_target Name)
	cmake_parse_arguments(T "COLLECT_AUTOMATION_TESTS"
		"TYPE;LAUNCH_MODULE;COMPILE_AGAINST_ENGINE;OUTPUT_NAME"
		"PLATFORMS;EXTRA_MODULE_NAMES;ENABLE_PLUGINS;DISABLE_PLUGINS" ${ARGN})
	if(T_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "leon_target(${Name}): unknown arguments: ${T_UNPARSED_ARGUMENTS}")
	endif()
	if(NOT T_TYPE MATCHES "^(Game|Program)$")
		message(FATAL_ERROR "leon_target(${Name}): TYPE must be Game or Program")
	endif()
	get_property(Existing GLOBAL PROPERTY LEON_TARGET_${Name}_FILE)
	if(Existing)
		message(FATAL_ERROR "Target '${Name}' is defined twice:\n  ${Existing}\n  ${CMAKE_CURRENT_LIST_FILE}")
	endif()

	if(NOT T_LAUNCH_MODULE)
		if(T_TYPE STREQUAL "Game")
			set(T_LAUNCH_MODULE Launch)
		else()
			set(T_LAUNCH_MODULE ${Name})
		endif()
	endif()
	if(T_COMPILE_AGAINST_ENGINE STREQUAL "")
		if(T_TYPE STREQUAL "Game")
			set(T_COMPILE_AGAINST_ENGINE ON)
		else()
			set(T_COMPILE_AGAINST_ENGINE OFF)
		endif()
	endif()
	if(NOT T_OUTPUT_NAME)
		set(T_OUTPUT_NAME ${Name})
	endif()

	set_property(GLOBAL APPEND PROPERTY LEON_TARGETS ${Name})
	set_property(GLOBAL PROPERTY LEON_TARGET_${Name}_FILE "${CMAKE_CURRENT_LIST_FILE}")
	set_property(GLOBAL PROPERTY LEON_TARGET_${Name}_ORIGIN "${_LEON_TARGET_ORIGIN}")
	foreach(Key TYPE LAUNCH_MODULE COMPILE_AGAINST_ENGINE OUTPUT_NAME PLATFORMS EXTRA_MODULE_NAMES
			ENABLE_PLUGINS DISABLE_PLUGINS COLLECT_AUTOMATION_TESTS)
		set_property(GLOBAL PROPERTY LEON_TARGET_${Name}_${Key} "${T_${Key}}")
	endforeach()
endfunction()

function(leon_target_get Name Key OutVar)
	get_property(Value GLOBAL PROPERTY LEON_TARGET_${Name}_${Key})
	set(${OutVar} "${Value}" PARENT_SCOPE)
endfunction()

function(leon_target_allowed Name OutVar)
	get_property(Allowed GLOBAL PROPERTY LEON_TARGET_${Name}_PLATFORMS)
	if(NOT Allowed)
		set(${OutVar} TRUE PARENT_SCOPE)
		return()
	endif()
	leon_platform_names(${LEON_PLATFORM} Names)
	foreach(Entry IN LISTS Allowed)
		if(Entry IN_LIST Names)
			set(${OutVar} TRUE PARENT_SCOPE)
			return()
		endif()
	endforeach()
	set(${OutVar} FALSE PARENT_SCOPE)
endfunction()
