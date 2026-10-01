# Renderer: Scene renderer (Unreal: Runtime/Renderer). It implements Engine's IRendererModule and FSceneInterface, so
# it depends on Engine (as in UE); Engine never includes a Renderer header.
# Every platform draws the same way (Docs/PLANS/ps2-gs-parity.md): the scene (FScene) and the GS scene renderer
# (Private/GS) record the frame as an FGSCommandList. The desktop module (Private/RendererModule.cpp) executes it on the
# OpenGL emulation of the GS (Private/GSEmulator, Engine/Shaders/gs_*); the PS2 extension sends it through FPS2RHI.
leon_module(Renderer
	PUBLIC_DEPENDENCIES Core RHI RenderCore Engine
	PRIVATE_DEPENDENCIES GSCore
	# TextureCompressor: the desktop draws uncooked textures as the PS2 cook makes them (Docs/PLANS/ps2-preview.md V1).
	PRIVATE_DEPENDENCIES_Desktop OpenGLDrv Glad TextureCompressor
	EXCLUDE_SOURCES_PS2
		Private/RendererModule.cpp
		Private/Shader.cpp
		Private/GSEmulator/*.cpp
		# FGSSceneCapture: the desktop tools' capture (LeonEd's map overview), with the desktop's texture converter.
		Private/Capture/*.cpp
)
