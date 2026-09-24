# ApplicationCore: Windowing and input devices (Unreal: Runtime/ApplicationCore).
leon_module(ApplicationCore
	PUBLIC_DEPENDENCIES Core InputCore RHI
	# Transitional: the window creates the platform RHI device (Launch will own this).
	PRIVATE_DEPENDENCIES_Desktop GLFW STB OpenGLDrv
)
