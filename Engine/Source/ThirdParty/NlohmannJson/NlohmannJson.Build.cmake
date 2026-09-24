# nlohmann/json 3.11.3 — JSON documents (levels, materials, recipes).
leon_module(NlohmannJson
	PLATFORMS Desktop
	DOWNLOAD_URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
	DOWNLOAD_SHA256 0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
	DOWNLOAD_DIR json-3.11.3
	EXTERNAL_TARGETS nlohmann_json::nlohmann_json
)

function(LeonExternal_NlohmannJson)
	set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
	set(JSON_Install OFF CACHE BOOL "" FORCE)
	add_subdirectory("${LEON_THIRDPARTY_DIR}" "${CMAKE_BINARY_DIR}/ThirdParty/NlohmannJson" EXCLUDE_FROM_ALL SYSTEM)
endfunction()
