# AudioMixer: Audio device (Unreal: Runtime/AudioMixer). Desktop plays through miniaudio (Private/AudioDevice.cpp); the
# PS2 extension has a silent device until the SPU2 backend (Docs/PLANS/ps2-engine.md, E5).
leon_module(AudioMixer
	PUBLIC_DEPENDENCIES Core
	PRIVATE_DEPENDENCIES_Desktop MiniAudio
	EXCLUDE_SOURCES_PS2 Private/AudioDevice.cpp
)
