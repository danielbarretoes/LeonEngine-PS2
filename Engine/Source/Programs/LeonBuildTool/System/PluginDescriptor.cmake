# .lplugin reader (Unreal: FPluginDescriptor / .uplugin).
#
#   { "FileVersion": 1, "Version": 1, "VersionName": "1.0", "FriendlyName": "...", "Category": "...",
#     "EnabledByDefault": false,
#     "Modules": [ { "Name": "X", "Type": "Runtime", "LoadingPhase": "Default", "PlatformAllowList": [ "Win64" ] } ] }
#
# Registers global properties:
#   LEON_PLUGINS, LEON_PLUGIN_<Name>_{DIR,ENABLED_BY_DEFAULT,MODULES},
#   LEON_PLUGIN_MODULE_<Module>_{PLUGIN,ALLOW_LIST}

set_property(GLOBAL PROPERTY LEON_PLUGINS "")

function(leon_read_plugin_descriptor File)
	file(READ "${File}" Json)
	get_filename_component(Name "${File}" NAME_WE)
	get_filename_component(Dir "${File}" DIRECTORY)

	get_property(Existing GLOBAL PROPERTY LEON_PLUGIN_${Name}_DIR)
	if(Existing)
		message(FATAL_ERROR "Plugin '${Name}' found twice:\n  ${Existing}\n  ${Dir}")
	endif()

	string(JSON EnabledByDefault ERROR_VARIABLE Error GET "${Json}" EnabledByDefault)
	if(Error)
		set(EnabledByDefault OFF)
	endif()

	set(Modules)
	string(JSON Count ERROR_VARIABLE Error LENGTH "${Json}" Modules)
	if(NOT Error AND Count GREATER 0)
		math(EXPR Last "${Count} - 1")
		foreach(Index RANGE ${Last})
			string(JSON ModuleName GET "${Json}" Modules ${Index} Name)
			list(APPEND Modules ${ModuleName})
			set(AllowList)
			string(JSON AllowCount ERROR_VARIABLE AllowError LENGTH "${Json}" Modules ${Index} PlatformAllowList)
			if(NOT AllowError AND AllowCount GREATER 0)
				math(EXPR AllowLast "${AllowCount} - 1")
				foreach(AllowIndex RANGE ${AllowLast})
					string(JSON Platform GET "${Json}" Modules ${Index} PlatformAllowList ${AllowIndex})
					list(APPEND AllowList ${Platform})
				endforeach()
			endif()
			set_property(GLOBAL PROPERTY LEON_PLUGIN_MODULE_${ModuleName}_PLUGIN ${Name})
			set_property(GLOBAL PROPERTY LEON_PLUGIN_MODULE_${ModuleName}_ALLOW_LIST "${AllowList}")
		endforeach()
	endif()

	set_property(GLOBAL APPEND PROPERTY LEON_PLUGINS ${Name})
	set_property(GLOBAL PROPERTY LEON_PLUGIN_${Name}_DIR "${Dir}")
	set_property(GLOBAL PROPERTY LEON_PLUGIN_${Name}_ENABLED_BY_DEFAULT ${EnabledByDefault})
	set_property(GLOBAL PROPERTY LEON_PLUGIN_${Name}_MODULES "${Modules}")
endfunction()

# leon_discover_plugins(<Root>...) — every *.lplugin under the roots.
function(leon_discover_plugins)
	set(Files)
	foreach(Root IN LISTS ARGN)
		if(EXISTS "${Root}")
			file(GLOB_RECURSE Found CONFIGURE_DEPENDS "${Root}/*.lplugin")
			list(APPEND Files ${Found})
		endif()
	endforeach()
	list(SORT Files)
	foreach(File IN LISTS Files)
		leon_read_plugin_descriptor("${File}")
	endforeach()
endfunction()
