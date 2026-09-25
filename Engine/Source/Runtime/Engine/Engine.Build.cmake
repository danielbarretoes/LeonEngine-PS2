# Engine: Gameplay framework, world, levels, physics scene (Unreal: Runtime/Engine).
# The Renderer depends on Engine (IRendererModule, FSceneInterface, the scene proxies), never the other way round.
leon_module(Engine
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core CoreUObject EngineSettings InputCore ApplicationCore RHI RenderCore SlateCore UMG
		PhysicsCore AnimationCore AudioMixer
	PRIVATE_DEPENDENCIES STB
)
