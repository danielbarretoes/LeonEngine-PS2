# Launch: executable entry point and engine loop (Unreal: Runtime/Launch).
# GuardedMain / FEngineLoop drive every game target; platform entry points live in
# Private/<Platform>/Launch<Platform>.cpp (PS2: platform extension).
leon_module(Launch
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI
	# The .lproj descriptor is loaded in PreInit (IProjectManager).
	PRIVATE_DEPENDENCIES Projects
	# Games compiled against the engine tick GEngine (UGameEngine) from FEngineLoop and mount their paks: the target
	# adds Engine and PakFile (LeonBuildTool, COMPILE_AGAINST_ENGINE). Desktop links them for every target, with the
	# Renderer (Engine reaches it only by name, IRendererModule; UE: Launch's Renderer dependency) and the platform RHI
	# PreInit's RHIInit needs (OpenGLDrv; the PS2 extension links PS2RHI). The PS2 has no Renderer yet
	# (Docs/PLANS/ps2-engine.md, E2): its engine games run headless.
	PRIVATE_DEPENDENCIES_Desktop Engine Renderer OpenGLDrv PakFile
)
