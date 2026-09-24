# Launch: executable entry point and engine loop (Unreal: Runtime/Launch).
# GuardedMain / FEngineLoop drive every game target; platform entry points live in
# Private/<Platform>/Launch<Platform>.cpp (PS2: platform extension).
leon_module(Launch
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI
	# Desktop games tick the gameplay framework session (UGameEngine) from FEngineLoop.
	PRIVATE_DEPENDENCIES_Desktop Engine NetCore Projects
)
