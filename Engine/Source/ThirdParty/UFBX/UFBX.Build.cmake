# ufbx — vendored FBX loader (skeletal meshes / animation).
leon_module(UFBX
	PLATFORMS Desktop
	EXTERNAL_TARGETS LeonThirdParty_UFBX
)

function(LeonExternal_UFBX)
	set(Dir "${LEON_ROOT_DIR}/ThirdParty/ufbx")
	add_library(LeonThirdParty_UFBX STATIC "${Dir}/ufbx.c")
	target_include_directories(LeonThirdParty_UFBX SYSTEM PUBLIC "${Dir}")
	if(MSVC)
		target_compile_options(LeonThirdParty_UFBX PRIVATE /W0)
	else()
		target_compile_options(LeonThirdParty_UFBX PRIVATE -w)
	endif()
endfunction()
