# Renderer: Scene renderer and GPU resources (Unreal: Runtime/Renderer). It implements Engine's IRendererModule and
# FSceneInterface, so it depends on Engine (as in UE); Engine never includes a Renderer header.
# Debt: calls OpenGL directly instead of going through RHI command lists.
leon_module(Renderer
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RHI RenderCore Engine
	PRIVATE_DEPENDENCIES OpenGLDrv Glad
)
