# TextureCompressor: builds a texture's platform data for a cook (Unreal: Developer/TextureCompressor with the
# TextureFormat modules): the PS2's paletted textures (PF_P8 / PF_P4, the GS's PSMT8 / PSMT4 with a CLUT), resized to
# the GS's power-of-two sides and quantized deterministically. Host tools and tests only.
leon_module(TextureCompressor
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RenderCore
)
