# LeonEd: the editor module (Unreal: Editor/UnrealEd): the asset factories, reimport, and the commandlets LeonCook runs
# (-run=ImportAssets, ResavePackages, ValidateAssets, Cook). Edit-time code: an Editor module,
# desktop only, linked by programs (LeonCook, LeonAutomationTests) and never by a game.
leon_module(LeonEd
	PUBLIC_DEPENDENCIES Core CoreUObject Engine
	PRIVATE_DEPENDENCIES RenderCore AnimationCore MeshUtilities Json STB
)
