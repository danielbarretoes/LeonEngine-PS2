# tinyobjloader 2.0.0rc13 — Wavefront OBJ import (Developer/MeshUtilities).
leon_module(TinyObjLoader
	PLATFORMS Desktop
	DOWNLOAD_URL https://github.com/tinyobjloader/tinyobjloader/archive/refs/tags/v2.0.0rc13.tar.gz
	DOWNLOAD_SHA256 0feb92b838f8ce4aa6eb0ccc32dff30cb64a891e0ec3bde837fca49c78d44334
	DOWNLOAD_DIR tinyobjloader-2.0.0rc13
	EXTERNAL_TARGETS tinyobjloader
)

function(LeonExternal_TinyObjLoader)
	set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
	set(TINYOBJLOADER_INSTALL OFF CACHE BOOL "" FORCE)
	add_subdirectory("${LEON_THIRDPARTY_DIR}" "${CMAKE_BINARY_DIR}/ThirdParty/TinyObjLoader" EXCLUDE_FROM_ALL SYSTEM)
endfunction()
