# Win64 platform (UnrealBuildTool: UEBuildWindows). Host compiler: MSVC via Engine/Build/BatchFiles/GetVSEnv.bat.
leon_register_platform(Win64
	GROUPS Windows Microsoft Desktop
	HEADER_NAME Windows
	CXX_STANDARD 17
	EXECUTABLE_SUFFIX .exe
	RHI_MODULE OpenGLDrv
	HOST_SYSTEM Windows
	BUILD_TYPE_Debug Debug
	BUILD_TYPE_Development RelWithDebInfo
	BUILD_TYPE_Shipping Release
	DEFINITIONS PLATFORM_WINDOWS=1 NOMINMAX WIN32_LEAN_AND_MEAN
)
