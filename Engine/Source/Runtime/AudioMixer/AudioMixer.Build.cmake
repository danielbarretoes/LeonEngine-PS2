# AudioMixer: Audio device (Unreal: Runtime/AudioMixer). FAudioDevice mixes on the game thread with
# FSoftwareAudioMixer on every platform and queues the mix on the platform's FAudioOutput: a device of miniaudio on the
# desktop (Private/Desktop), the SPU2 through audsrv in the PS2 extension (Docs/PLANS/ps2-preview.md V1).
leon_module(AudioMixer
	PUBLIC_DEPENDENCIES Core
	PRIVATE_DEPENDENCIES_Desktop MiniAudio
)
