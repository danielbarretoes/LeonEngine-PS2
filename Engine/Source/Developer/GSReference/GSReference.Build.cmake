# GSReference: a software Graphics Synthesizer that executes an FGSCommandList by the GS User's Manual (Leon; the
# oracle of Docs/PLANS/ps2-gs-parity.md): the Win64 preview's GS emulator and the PS2 backend are checked against it.
# Host tools and tests only.
leon_module(GSReference
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core GSCore
)
