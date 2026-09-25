# Engine: Gameplay framework, world, levels, physics scene (Unreal: Runtime/Engine).
leon_module(Engine
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI RenderCore UMG PhysicsCore AnimationCore AudioMixer
	PRIVATE_DEPENDENCIES STB
	CIRCULAR_DEPENDENCIES Renderer
)
