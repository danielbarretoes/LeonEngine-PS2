# Engine: Gameplay framework, world, levels, physics scene (Unreal: Runtime/Engine).
# The Renderer depends on Engine (IRendererModule, FSceneInterface, the scene proxies), never the other way round.
leon_module(Engine
	PUBLIC_DEPENDENCIES Core CoreUObject EngineSettings InputCore ApplicationCore RHI RenderCore SlateCore UMG
		PhysicsCore AnimationCore AudioMixer
	PRIVATE_DEPENDENCIES STB
	# AudioCompressor: an editor build makes an uncooked sound's SPU2 ADPCM as the cook would, so the desktop plays what
	# the PS2 plays (USoundWave::CacheCompressedData; Docs/PLANS/ps2-shipping.md N19).
	PRIVATE_DEPENDENCIES_Desktop AudioCompressor
)
