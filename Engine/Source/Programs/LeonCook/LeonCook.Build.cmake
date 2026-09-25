# LeonCook: the engine as a command-line editor (Unreal: UE4Editor-Cmd -run=<Commandlet>). Links the editor module
# (LeonEd: factories and commandlets) and Engine, but no renderer or RHI.
leon_module(LeonCook
	PLATFORMS Desktop
	PRIVATE_DEPENDENCIES Core CoreUObject Projects Engine LeonEd
)
