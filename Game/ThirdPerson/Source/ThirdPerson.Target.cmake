# ThirdPerson game target (Unreal: <Game>.Target.cs). Launch (GuardedMain / FEngineLoop) owns main();
# the gameplay framework does not run on PS2, so the target compiles without the engine (WITH_ENGINE=0).
leon_target(ThirdPerson TYPE Game
	PLATFORMS PS2
	COMPILE_AGAINST_ENGINE OFF
)
