# ThirdPerson game target (Unreal: <Game>.Target.cs). Launch (GuardedMain / FEngineLoop) owns main();
# the game keeps its own F* types and compiles without the engine (WITH_ENGINE=0); ShooterGame is the engine game on PS2.
leon_target(ThirdPerson TYPE Game
	PLATFORMS PS2
	COMPILE_AGAINST_ENGINE OFF
)
