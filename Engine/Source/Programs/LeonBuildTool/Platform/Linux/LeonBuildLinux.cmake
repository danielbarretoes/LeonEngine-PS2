# Linux platform (UnrealBuildTool: UEBuildLinux). Registered and folder-filtered; not a verification gate.
leon_register_platform(Linux
	GROUPS Unix Linux Desktop
	HEADER_NAME Linux
	CXX_STANDARD 20
	EXECUTABLE_SUFFIX ""
	RHI_MODULE OpenGLDrv
	HOST_SYSTEM Linux
	BUILD_TYPE_Debug Debug
	BUILD_TYPE_Development RelWithDebInfo
	BUILD_TYPE_Shipping Release
	DEFINITIONS PLATFORM_LINUX=1
		LEON_PLATFORM_HOST=1 # transitional (removed in Phase 3)
)
