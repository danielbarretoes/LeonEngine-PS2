# LeonBuildTool host tools (UnrealBuildTool builds UnrealHeaderTool before the targets that need it).
#
#   leon_build_host_tools(<OutHeaderTool> <OutError>)
#
# Configures Engine/Source/Programs/LeonHeaderTool (its own CMakeLists.txt, std only) with the host compiler and
# Ninja, Release, into Engine/Intermediate/Build/HostTools/<Host>/ the first time, then runs `cmake --build`, which is
# a ninja no-op when nothing changed. Returns the LeonHeaderTool executable, which LeonBuildTool.cmake passes to the
# target configure as -DLEON_HEADER_TOOL=<path>, or an error message (the caller restores its console first).
# Script-mode only (called from LeonBuildTool.cmake).
#
# <Host>: Win64 (MSVC from the environment Build.bat sets up with GetVSEnv.bat), Linux (the system c++) or LinuxMusl
# (the ps2dev Docker image: Alpine, g++ and musl-dev installed by DockerEntry.sh). A Linux host that runs PS2 builds in
# Docker shares Engine/Intermediate with the container, hence the separate musl tree.

function(leon_host_tools_platform OutVar)
	if(CMAKE_HOST_WIN32)
		set(${OutVar} Win64 PARENT_SCOPE)
	elseif(EXISTS "/etc/alpine-release")
		set(${OutVar} LinuxMusl PARENT_SCOPE)
	else()
		set(${OutVar} Linux PARENT_SCOPE)
	endif()
endfunction()

function(leon_build_host_tools OutVar OutError)
	set(${OutVar} "" PARENT_SCOPE)
	set(${OutError} "" PARENT_SCOPE)
	leon_host_tools_platform(Host)
	set(SourceDir "${LEON_ENGINE_DIR}/Source/Programs/LeonHeaderTool")
	set(BinaryDir "${LEON_ENGINE_DIR}/Intermediate/Build/HostTools/${Host}")
	set(Tool "${BinaryDir}/LeonHeaderTool")
	if(CMAKE_HOST_WIN32)
		string(APPEND Tool ".exe")
	endif()

	if(NOT EXISTS "${BinaryDir}/build.ninja")
		set(ConfigureArgs -S "${SourceDir}" -B "${BinaryDir}" -G Ninja -DCMAKE_BUILD_TYPE=Release)
		if(CMAKE_HOST_WIN32)
			# Never pick up another compiler from PATH (clang, MinGW): the host tools use the same MSVC as Win64 builds.
			list(APPEND ConfigureArgs -DCMAKE_CXX_COMPILER=cl)
		endif()
		message(STATUS "LeonBuildTool: configuring host tools (${Host}) in ${BinaryDir}")
		execute_process(COMMAND "${CMAKE_COMMAND}" ${ConfigureArgs}
			RESULT_VARIABLE Result OUTPUT_VARIABLE Output ERROR_VARIABLE Output)
		if(NOT Result EQUAL 0)
			# Do not leave a half-configured tree behind: the next run would skip the configure step.
			file(REMOVE "${BinaryDir}/build.ninja")
			set(${OutError} "LeonBuildTool: configuring LeonHeaderTool failed (is a host C++ compiler on PATH?):\n${Output}"
				PARENT_SCOPE)
			return()
		endif()
	endif()

	execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BinaryDir}"
		RESULT_VARIABLE Result OUTPUT_VARIABLE Output ERROR_VARIABLE Output)
	if(NOT Result EQUAL 0)
		set(${OutError} "LeonBuildTool: building LeonHeaderTool failed:\n${Output}" PARENT_SCOPE)
		return()
	endif()
	# Ninja 1.11 prints "no work to do", newer releases "nothing to do".
	if(NOT Output MATCHES "no work to do|nothing to do")
		message(STATUS "LeonBuildTool: built LeonHeaderTool (${Host})")
	endif()
	set(${OutVar} "${Tool}" PARENT_SCOPE)
endfunction()
