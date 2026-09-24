# ENet — vendored reliable UDP (LAN multiplayer).
leon_module(ENet
	PLATFORMS Desktop
	EXTERNAL_TARGETS LeonThirdParty_ENet
	PUBLIC_SYSTEM_LIBRARIES_Windows ws2_32 winmm
)

function(LeonExternal_ENet)
	set(Dir "${LEON_MODULE_DIR}/enet")
	add_library(LeonThirdParty_ENet STATIC
		"${Dir}/callbacks.c" "${Dir}/compress.c" "${Dir}/host.c" "${Dir}/list.c" "${Dir}/packet.c"
		"${Dir}/peer.c" "${Dir}/protocol.c" "${Dir}/unix.c" "${Dir}/win32.c")
	target_include_directories(LeonThirdParty_ENet SYSTEM PUBLIC "${Dir}/include")
	if(MSVC)
		target_compile_options(LeonThirdParty_ENet PRIVATE /W0)
		target_compile_definitions(LeonThirdParty_ENet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
	else()
		target_compile_options(LeonThirdParty_ENet PRIVATE -w)
	endif()
endfunction()
