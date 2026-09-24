# stb (stb_image, stb_easy_font) — vendored header-only image/font helpers.
leon_module(STB
	PLATFORMS Desktop
	EXTERNAL_TARGETS LeonThirdParty_STB
)

function(LeonExternal_STB)
	add_library(LeonThirdParty_STB INTERFACE)
	target_include_directories(LeonThirdParty_STB SYSTEM INTERFACE "${LEON_ROOT_DIR}/ThirdParty/stb")
endfunction()
