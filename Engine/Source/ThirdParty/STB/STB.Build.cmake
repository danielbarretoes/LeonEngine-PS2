# stb (stb_image, stb_easy_font) — vendored header-only image/font helpers, portable C on every platform.
leon_module(STB
	EXTERNAL_TARGETS LeonThirdParty_STB
)

function(LeonExternal_STB)
	add_library(LeonThirdParty_STB INTERFACE)
	target_include_directories(LeonThirdParty_STB SYSTEM INTERFACE "${LEON_MODULE_DIR}/stb")
endfunction()
