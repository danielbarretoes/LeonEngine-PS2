# AudioCompressor: builds a sound's platform data for a cook (Unreal: Developer/AudioFormatADPCM and the other
# AudioFormat modules): the SPU2's ADPCM (FSpuAdpcmEncoder), mono, resampled to the sound's rate, the loop on a block,
# the same bytes every time. The decoder is AudioMixer's (FSpuAdpcm), which plays the data on every platform. Host
# tools, the tests, and the desktop's uncooked sounds (Engine) only.
leon_module(AudioCompressor
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core AudioMixer
)
