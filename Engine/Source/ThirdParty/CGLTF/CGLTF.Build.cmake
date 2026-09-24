# cgltf — vendored header-only glTF 2.0 loader.
leon_module(CGLTF
	PLATFORMS Desktop
	EXTERNAL_TARGETS LeonThirdParty_CGLTF
)

function(LeonExternal_CGLTF)
	add_library(LeonThirdParty_CGLTF INTERFACE)
	target_include_directories(LeonThirdParty_CGLTF SYSTEM INTERFACE "${LEON_MODULE_DIR}/cgltf")
endfunction()
