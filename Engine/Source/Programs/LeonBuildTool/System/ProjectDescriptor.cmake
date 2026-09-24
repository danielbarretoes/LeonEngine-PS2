# .leonproject reader (Unreal: FProjectDescriptor / .uproject).
#
#   { "FileVersion": 1, "EngineAssociation": "", "Description": "...",
#     "Modules": [ { "Name": "ThirdPerson", "Type": "Runtime", "LoadingPhase": "Default" } ],
#     "Plugins": [ { "Name": "JoltPhysics", "Enabled": true } ],
#     "TargetPlatforms": [ "PS2" ] }
#
# Sets in the caller's scope:
#   LEON_PROJECT_NAME, LEON_PROJECT_DIR, LEON_PROJECT_MODULES,
#   LEON_PROJECT_ENABLED_PLUGINS, LEON_PROJECT_DISABLED_PLUGINS, LEON_PROJECT_TARGET_PLATFORMS

function(_leon_json_array_length Json OutVar)
	string(JSON Length ERROR_VARIABLE Error LENGTH "${Json}" ${ARGN})
	if(Error)
		set(Length 0)
	endif()
	set(${OutVar} ${Length} PARENT_SCOPE)
endfunction()

macro(leon_read_project_descriptor File)
	if(NOT EXISTS "${File}")
		message(FATAL_ERROR "Project file not found: ${File}")
	endif()
	file(READ "${File}" _LeonProjectJson)
	get_filename_component(LEON_PROJECT_NAME "${File}" NAME_WE)
	get_filename_component(LEON_PROJECT_DIR "${File}" DIRECTORY)
	set(LEON_PROJECT_MODULES)
	set(LEON_PROJECT_ENABLED_PLUGINS)
	set(LEON_PROJECT_DISABLED_PLUGINS)
	set(LEON_PROJECT_TARGET_PLATFORMS)

	_leon_json_array_length("${_LeonProjectJson}" _LeonCount Modules)
	if(_LeonCount GREATER 0)
		math(EXPR _LeonLast "${_LeonCount} - 1")
		foreach(_LeonIndex RANGE ${_LeonLast})
			string(JSON _LeonName GET "${_LeonProjectJson}" Modules ${_LeonIndex} Name)
			list(APPEND LEON_PROJECT_MODULES ${_LeonName})
		endforeach()
	endif()

	_leon_json_array_length("${_LeonProjectJson}" _LeonCount Plugins)
	if(_LeonCount GREATER 0)
		math(EXPR _LeonLast "${_LeonCount} - 1")
		foreach(_LeonIndex RANGE ${_LeonLast})
			string(JSON _LeonName GET "${_LeonProjectJson}" Plugins ${_LeonIndex} Name)
			string(JSON _LeonEnabled ERROR_VARIABLE _LeonError GET "${_LeonProjectJson}" Plugins ${_LeonIndex} Enabled)
			if(_LeonError OR _LeonEnabled)
				list(APPEND LEON_PROJECT_ENABLED_PLUGINS ${_LeonName})
			else()
				list(APPEND LEON_PROJECT_DISABLED_PLUGINS ${_LeonName})
			endif()
		endforeach()
	endif()

	_leon_json_array_length("${_LeonProjectJson}" _LeonCount TargetPlatforms)
	if(_LeonCount GREATER 0)
		math(EXPR _LeonLast "${_LeonCount} - 1")
		foreach(_LeonIndex RANGE ${_LeonLast})
			string(JSON _LeonName GET "${_LeonProjectJson}" TargetPlatforms ${_LeonIndex})
			list(APPEND LEON_PROJECT_TARGET_PLATFORMS ${_LeonName})
		endforeach()
	endif()
endmacro()
