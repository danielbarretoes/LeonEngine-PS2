# PlayStation 2 (Emotion Engine) platform extension for LeonBuildTool.
#
# Builds run inside the ps2dev Docker image (pinned by digest) unless PS2DEV is set on the host: the image LeonBuildTool
# derives from it once with DOCKER_FILE (CMake, Ninja and g++ preinstalled), so a build does not install them again.
# Development maps to Release: the EE build keeps -O2 without debug info (matches ps2sdk samples).
leon_register_platform(PS2
	IS_EXTENSION
	GROUPS PS2 Console
	HEADER_NAME PS2
	TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/PS2Toolchain.cmake"
	CXX_STANDARD 17
	EXECUTABLE_SUFFIX .elf
	RHI_MODULE PS2RHI
	SDK_ENV PS2DEV
	DOCKER_IMAGE "ghcr.io/ps2dev/ps2dev@sha256:79c24d3762f5cfeee0beabeacc94480fe580d379d3e48a0bfbac681a8895daf6"
	DOCKER_ENTRY Engine/Platforms/PS2/Build/BatchFiles/DockerEntry.sh
	DOCKER_FILE Engine/Platforms/PS2/Build/Docker/Dockerfile
	BUILD_TYPE_Debug Debug
	BUILD_TYPE_Development Release
	BUILD_TYPE_Shipping Release
	DEFINITIONS PLATFORM_PS2=1
)

# VU microcode (Docs/PLANS/ps2-shipping.md N14): a module's Private/VU1/*.vsm (dvp-as syntax, one upper and one lower
# instruction a line) is assembled by the ps2dev toolchain's dvp-as ($PS2DEV/dvp/bin) into an object archived with the
# module. The ELF carries its .vutext as data, and its global labels are the EE's symbols of the code (MPG uploads it).
# A .vsm may `.include` the code its programs share from a *.vsi of the same folder (ps2-polish P8b), which is not
# assembled on its own. LeonBuildTool calls this for every module of a PS2 build (leon_instantiate_module).
function(LeonPlatform_PS2_ModuleSources Name Target)
	leon_module_get(${Name} DIR Dir)
	_leon_module_ext_dirs(${Name} ExtDirs)
	set(Sources)
	foreach(Root IN ITEMS "${Dir}" ${ExtDirs})
		if(IS_DIRECTORY "${Root}/Private/VU1")
			file(GLOB Found CONFIGURE_DEPENDS "${Root}/Private/VU1/*.vsm")
			list(APPEND Sources ${Found})
		endif()
	endforeach()
	if(NOT Sources)
		return()
	endif()
	find_program(LEON_DVP_AS dvp-as HINTS "$ENV{PS2DEV}/dvp/bin" REQUIRED)
	foreach(Source IN LISTS Sources)
		get_filename_component(Base "${Source}" NAME_WE)
		get_filename_component(SourceDir "${Source}" DIRECTORY)
		file(GLOB Includes CONFIGURE_DEPENDS "${SourceDir}/*.vsi")
		set(ObjectDir "${CMAKE_CURRENT_BINARY_DIR}/VU1/${Name}")
		set(Object "${ObjectDir}/${Base}.vu.o")
		add_custom_command(OUTPUT "${Object}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${ObjectDir}"
			COMMAND "${LEON_DVP_AS}" -I "${SourceDir}" -o "${Object}" "${Source}"
			DEPENDS "${Source}" ${Includes}
			COMMENT "dvp-as ${Base}.vsm"
			VERBATIM)
		set_source_files_properties("${Object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
		target_sources(${Target} PRIVATE "${Object}")
	endforeach()
endfunction()
