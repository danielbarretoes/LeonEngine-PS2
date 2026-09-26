# PS2 extension of AudioMixer (Docs/PLANS/ps2-engine.md, E5): FAudioDevice mixes on the EE (FSoftwareAudioMixer) and
# streams to the SPU2 through audsrv (libaudsrv, and libpatches for loading its IOP module from EE memory). The IOP
# module comes from the SDK and goes beside the executable, which loads it at start.
leon_module_extend(AudioMixer
	PUBLIC_SYSTEM_LIBRARIES audsrv patches
	RUNTIME_DEPENDENCIES "$ENV{PS2SDK}/iop/irx/audsrv.irx"
)
