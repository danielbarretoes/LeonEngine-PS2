# LeonEd: the editor module (Unreal: Editor/UnrealEd): the asset factories, reimport, and the commandlets LeonCook runs
# (-run=ImportAssets, ResavePackages, ValidateAssets, Cook, EmbedFont). Edit-time code: an Editor module,
# desktop only, linked by programs (LeonCook, LeonAutomationTests) and never by a game.
leon_module(LeonEd
	# TargetPlatform: the cook's platforms (UCookCommandlet takes an ITargetPlatform); RenderCore: its cooked textures'
	# pixel formats.
	PUBLIC_DEPENDENCIES Core CoreUObject Engine TargetPlatform RenderCore
	# TextureCompressor and GSCore: the PS2 cook's paletted textures and their VRAM report, and the GS debug font's layout.
	PRIVATE_DEPENDENCIES AnimationCore MeshUtilities Json STB TextureCompressor GSCore
)
