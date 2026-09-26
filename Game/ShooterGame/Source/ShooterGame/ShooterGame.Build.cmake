# ShooterGame game module (Unreal: ShooterGame.Build.cs): the game's classes (game mode, character, controllers, HUD,
# player state), found by reflection (plan decision D18).
leon_module(ShooterGame
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core CoreUObject InputCore Engine
	PRIVATE_DEPENDENCIES AIModule
)
