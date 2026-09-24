# Engine: Gameplay framework, world, levels, physics scene, net driver (Unreal: Runtime/Engine).
leon_module(Engine
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI RenderCore UMG PhysicsCore AnimationCore AudioMixer
		NetCore GLM NlohmannJson
	PRIVATE_DEPENDENCIES Projects ENet GLFW STB
	CIRCULAR_DEPENDENCIES Renderer
)
