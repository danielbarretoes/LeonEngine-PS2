# AudioMixer: Audio device (Unreal: Runtime/AudioMixer). FAudioDevice runs the SPU2's model on every platform
# (Docs/PLANS/ps2-shipping.md N19): SPU2 ADPCM buffers resident in its RAM and 24 hardware voices. The platform's
# FAudioHardware carries it out: audsrv's voices in the PS2 extension; on the desktop (Private/Desktop) the buffers
# decoded and the voices mixed on the CPU into a device of miniaudio.
leon_module(AudioMixer
	PUBLIC_DEPENDENCIES Core
	PRIVATE_DEPENDENCIES_Desktop MiniAudio
)
