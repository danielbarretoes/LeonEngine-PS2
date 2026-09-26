# PS2 extension of Core: the EE timer (GetTimerSystemTime) and the IOP reset are in libkernel, the LOADFILE patches in
# libpatches.
leon_module_extend(Core
	PUBLIC_SYSTEM_LIBRARIES patches kernel
)
