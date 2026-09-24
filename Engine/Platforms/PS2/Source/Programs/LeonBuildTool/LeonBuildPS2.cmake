# PlayStation 2 (Emotion Engine) platform extension for LeonBuildTool.
#
# Builds run inside the ps2dev Docker image (pinned by digest) unless PS2DEV is set on the host.
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
	BUILD_TYPE_Debug Debug
	BUILD_TYPE_Development Release
	BUILD_TYPE_Shipping Release
	DEFINITIONS PLATFORM_PS2=1
)
