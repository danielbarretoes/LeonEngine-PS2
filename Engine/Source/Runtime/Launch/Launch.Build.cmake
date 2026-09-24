# Launch: executable entry point and engine loop (Unreal: Runtime/Launch).
# GuardedMain / FEngineLoop drive every game target; platform entry points live in
# Private/<Platform>/Launch<Platform>.cpp (PS2: platform extension).
leon_module(Launch
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI
	# Desktop games still run the pre-UE GameApplication loop inside FEngineLoop (Phase 4 folds it in).
	PRIVATE_DEPENDENCIES_Desktop Engine NetCore Projects
)
