# Launch: executable entry point and engine loop (Unreal: Runtime/Launch).
# GuardedMain / FEngineLoop drive every game target; platform entry points live in
# Private/<Platform>/Launch<Platform>.cpp (PS2: platform extension).
leon_module(Launch
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI
	# The .lproj descriptor is loaded in PreInit (IProjectManager).
	PRIVATE_DEPENDENCIES Projects
	# Desktop games tick GEngine (UGameEngine) from FEngineLoop; Engine reaches the Renderer module only by name
	# (IRendererModule), so the launch module links it (UE: Launch's Renderer dependency), and PreInit's RHIInit needs
	# the platform RHI (OpenGLDrv; the PS2 extension links PS2RHI).
	PRIVATE_DEPENDENCIES_Desktop Engine Renderer OpenGLDrv
)
