# GSCore: the Graphics Synthesizer's contract (Leon; UE's counterpart is the RHI command list): the GS registers and
# formats of Docs/PS2OFFICIAL/GS_Users_Manual.pdf, and FGSCommandList, the register stream the scene renderer fills and
# every backend consumes (the PS2's GIF, the Win64 GS emulator, the reference rasterizer). Every platform.
leon_module(GSCore
	PUBLIC_DEPENDENCIES Core
)
