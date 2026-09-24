# Renderer: Scene renderer and GPU resources (Unreal: Runtime/Renderer).
# Debt: calls OpenGL directly instead of going through RHI command lists.
leon_module(Renderer
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RHI RenderCore SlateCore AnimationCore GLM NlohmannJson
	PRIVATE_DEPENDENCIES OpenGLDrv Glad STB
	# Debt: Renderer.h includes Level.h and Level.h includes GPU resources.
	CIRCULAR_DEPENDENCIES Engine
)
