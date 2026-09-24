# PS2RHI: PlayStation 2 Graphics Synthesizer RHI (platform extension; Unreal: <Platform>RHI).
leon_module(PS2RHI
	PLATFORMS PS2
	PUBLIC_DEPENDENCIES Core RHI
	PUBLIC_SYSTEM_LIBRARIES draw math3d packet graph dma kernel
)
