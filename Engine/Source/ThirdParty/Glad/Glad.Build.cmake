# GLAD — vendored OpenGL 3.3 core loader.
leon_module(Glad
	PLATFORMS Desktop
	EXTERNAL_TARGETS LeonThirdParty_Glad
)

function(LeonExternal_Glad)
	set(Dir "${LEON_ROOT_DIR}/ThirdParty/glad")
	add_library(LeonThirdParty_Glad STATIC "${Dir}/src/glad.c")
	target_include_directories(LeonThirdParty_Glad SYSTEM PUBLIC "${Dir}/include")
endfunction()
