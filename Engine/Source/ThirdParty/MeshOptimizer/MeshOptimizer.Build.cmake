# meshoptimizer 1.2 — vertex cache optimization, stripification and simplification of the cooked meshes
# (Developer/MeshUtilities: the LPS2 v2 build, Docs/ASSET_FORMATS.md). Edit time only: no game target links it.
leon_module(MeshOptimizer
	PLATFORMS Desktop
	DOWNLOAD_URL https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.2.tar.gz
	DOWNLOAD_SHA256 e40f71b809cdf3361b9a4def85fd44534e8733ce29d4b943c145b76859e4c2b4
	DOWNLOAD_DIR meshoptimizer-1.2
	EXTERNAL_TARGETS meshoptimizer
)

function(LeonExternal_MeshOptimizer)
	set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
	set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
	set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
	add_subdirectory("${LEON_THIRDPARTY_DIR}" "${CMAKE_BINARY_DIR}/ThirdParty/MeshOptimizer" EXCLUDE_FROM_ALL SYSTEM)
	# A C++ library built with its own CMake files: keep the compiler's exceptions and RTTI (Leon code turns them off).
	leon_third_party_cxx_defaults(meshoptimizer)
endfunction()
