# ThirdPerson game module (Unreal: <Game>.Build.cs).
leon_module(ThirdPerson
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore
	# Launch: GEngineLoop (main window + application); PS2RHI: GS drawing.
	PRIVATE_DEPENDENCIES Launch PS2RHI
)
