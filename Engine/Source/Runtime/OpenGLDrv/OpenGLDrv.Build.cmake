# OpenGLDrv: OpenGL 3.3 RHI device (Unreal: Runtime/OpenGLDrv).
leon_module(OpenGLDrv
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RHI
	PRIVATE_DEPENDENCIES Glad
	PUBLIC_SYSTEM_LIBRARIES_Windows dxgi
)
