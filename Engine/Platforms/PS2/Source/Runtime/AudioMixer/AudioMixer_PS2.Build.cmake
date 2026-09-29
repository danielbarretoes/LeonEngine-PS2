# PS2 extension of AudioMixer (Docs/PLANS/ps2-shipping.md N19): the hardware (FAudioHardware) is the SPU2's voices
# through audsrv's ADPCM calls (libaudsrv; Core readies the IOP to load its module from EE memory). The IOP module comes
# from the SDK and goes beside the executable, which loads it at start.
leon_module_extend(AudioMixer
	PUBLIC_SYSTEM_LIBRARIES audsrv
	RUNTIME_DEPENDENCIES "$ENV{PS2SDK}/iop/irx/audsrv.irx"
)
