# ShooterGame game target (Unreal: ShooterGame.Target.cs). Launch owns main (GuardedMain / FEngineLoop) with the engine
# (WITH_ENGINE=1): GEngine opens GameDefaultMap (/Game/Maps/de_leon) with GlobalDefaultGameMode
# (/Script/ShooterGame.ShooterGameMode), plan decision D18. The project's module comes from the .lproj.
leon_target(ShooterGame TYPE Game
	PLATFORMS Win64
)
