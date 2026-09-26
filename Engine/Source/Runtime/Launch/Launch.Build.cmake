# Launch: executable entry point and engine loop (Unreal: Runtime/Launch).
# GuardedMain / FEngineLoop drive every game target; platform entry points live in
# Private/<Platform>/Launch<Platform>.cpp (PS2: platform extension).
leon_module(Launch
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI
	# The .lproj descriptor is loaded in PreInit (IProjectManager).
	PRIVATE_DEPENDENCIES Projects
	# Games compiled against the engine tick GEngine (UGameEngine) from FEngineLoop and mount their paks: the target
	# adds Engine, the Renderer and PakFile on every platform (LeonBuildTool, COMPILE_AGAINST_ENGINE). Desktop links
	# them for every target, with the Renderer (Engine reaches it only by name, IRendererModule; UE: Launch's Renderer
	# dependency) and the platform RHI PreInit's RHIInit needs (OpenGLDrv; the PS2 extension links PS2RHI).
	PRIVATE_DEPENDENCIES_Desktop Engine Renderer OpenGLDrv PakFile
)
