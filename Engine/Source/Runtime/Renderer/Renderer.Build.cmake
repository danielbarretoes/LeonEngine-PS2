# Renderer: Scene renderer and GPU resources (Unreal: Runtime/Renderer). It implements Engine's IRendererModule and
# FSceneInterface, so it depends on Engine (as in UE); Engine never includes a Renderer header.
# Every platform shares the scene (FScene) and the GS scene renderer (Private/GS: the scene as an FGSCommandList,
# Docs/PLANS/ps2-gs-parity.md P5). The desktop draws with OpenGL directly (the files excluded on PS2) until the GL
# emulation of the GS (P4); the PS2 extension implements the module on the GS scene renderer and FPS2RHI.
leon_module(Renderer
	PUBLIC_DEPENDENCIES Core RHI RenderCore Engine
	PRIVATE_DEPENDENCIES GSCore
	PRIVATE_DEPENDENCIES_Desktop OpenGLDrv Glad
	EXCLUDE_SOURCES_PS2
		Private/CanvasRenderer.cpp
		Private/GPUPassTimer.cpp
		Private/LineBatchRenderer.cpp
		Private/PlanarReflection.cpp
		Private/RenderResourceCache.cpp
		Private/RendererModule.cpp
		Private/SceneRenderer.cpp
		Private/Shader.cpp
		Private/ShadowMap.cpp
		Private/SkeletalMeshRenderData.cpp
		Private/StaticMeshRenderData.cpp
		Private/Texture2DResource.cpp
		Private/UniformBuffer.cpp
		Private/WorldEffectsRenderer.cpp
)
