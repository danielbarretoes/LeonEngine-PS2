# Catch2 3.5.4 — test framework for LeonAutomationTests.
leon_module(Catch2
	PLATFORMS Desktop
	DOWNLOAD_URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.4.tar.gz
	DOWNLOAD_SHA256 b7754b711242c167d8f60b890695347f90a1ebc95949a045385114165d606dbb
	DOWNLOAD_DIR Catch2-3.5.4
	EXTERNAL_TARGETS Catch2::Catch2
)

function(LeonExternal_Catch2)
	set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
	set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
	add_subdirectory("${LEON_THIRDPARTY_DIR}" "${CMAKE_BINARY_DIR}/ThirdParty/Catch2" EXCLUDE_FROM_ALL SYSTEM)
endfunction()
