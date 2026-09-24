# PS2 extension of ApplicationCore: window, pad input and the engine debug overlay draw with the GS.
leon_module_extend(ApplicationCore
	PRIVATE_DEPENDENCIES PS2RHI
	PUBLIC_SYSTEM_LIBRARIES pad
)
