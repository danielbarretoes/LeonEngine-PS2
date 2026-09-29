#include "AudioDevice.h"
#include "AudioHardware.h"
#include "Desktop/AudioOutput.h"
#include "Desktop/SoftwareAudioMixer.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogAudioHardware, Log, All);

namespace
{

	/** The SPU2's rate, which the desktop's output runs at too. */
	constexpr int32 OutputRate = FSpuAdpcm::OutputRate;
	/** The audio queued ahead of the output when it starts: two frames at 30 fps. */
	constexpr int32 LeadFrames = OutputRate * 2 / 30;
	/** The most a tick mixes: a hitch longer than this skips audio rather than queueing it (0.1 s). */
	constexpr uint64 MaxTickMicroseconds = 100000;

	FAudioOutputFactory GAudioOutputFactory = nullptr;

	/**
	 * The desktop's sound hardware (Docs/PLANS/ps2-shipping.md N19): each buffer decoded from its ADPCM once, as the
	 * SPU2 would decode it, and the voices mixed on the game thread (FSoftwareAudioMixer) at the SPU2's pitch and
	 * audsrv's levels into a device of miniaudio. Each tick queues what the output played since the last one.
	 */
	class FSoftwareAudioHardware final : public FAudioHardware
	{
	public:
		FSoftwareAudioHardware()
		{
			Buffers.SetNum(FAudioDevice::MaxSoundBuffers);
		}

		bool Start() override
		{
			Output = GAudioOutputFactory != nullptr ? GAudioOutputFactory() : CreatePlatformAudioOutput();
			if (!Output || !Output->Start(OutputRate))
			{
				UE_LOG(LogAudioHardware, Warning, "Audio: no output device");
				Output.Reset();
				return false;
			}
			Stream(LeadFrames);
			LastTickCycles = FPlatformTime::Cycles64();
			return true;
		}

		bool UploadSound(int32 Buffer, const FSpuAdpcmSound& Sound, int32 Pitch) override
		{
			FDecodedBuffer& Decoded = Buffers[Buffer];
			FSpuAdpcm::Decode(Sound, Decoded.Samples);
			Decoded.LoopStartFrame = Sound.LoopStartFrame;
			Decoded.Pitch = Pitch;
			return Decoded.Samples.Num() > 0;
		}

		void FreeSound(int32 Buffer) override
		{
			for (int32 Voice = 0; Voice < FSoftwareAudioMixer::MaxVoices; ++Voice)
			{
				if (VoiceBuffers[Voice] == Buffer)
				{
					Mixer.Stop(Voice);
					VoiceBuffers[Voice] = INDEX_NONE;
				}
			}
			Buffers[Buffer].Samples.Empty();
		}

		bool PlayVoice(int32 Voice, int32 Buffer, const FSpuVoiceVolume& Volume) override
		{
			const FDecodedBuffer& Decoded = Buffers[Buffer];
			Mixer.Play(Voice, Decoded.Samples.GetData(), Decoded.Samples.Num(), Decoded.LoopStartFrame, Decoded.Pitch,
				Volume.GetLeftGain(), Volume.GetRightGain());
			VoiceBuffers[Voice] = Buffer;
			return true;
		}

		void SetVoiceVolume(int32 Voice, const FSpuVoiceVolume& Volume) override
		{
			Mixer.SetGains(Voice, Volume.GetLeftGain(), Volume.GetRightGain());
		}

		void StopVoice(int32 Voice) override
		{
			Mixer.Stop(Voice);
		}

		void Tick() override
		{
			// The output plays at the output rate: a tick queues what it played since the last one.
			const uint64 Now = FPlatformTime::Cycles64();
			const uint64 Elapsed =
				FMath::Min(FPlatformTime::CyclesToMicroseconds(Now - LastTickCycles), MaxTickMicroseconds);
			LastTickCycles = Now;
			PendingFrameMicroseconds += Elapsed * uint64(OutputRate);
			const uint64 Frames = PendingFrameMicroseconds / 1000000ull;
			PendingFrameMicroseconds -= Frames * 1000000ull;
			Stream(int32(Frames));
		}

	private:
		struct FDecodedBuffer
		{
			TArray<int16> Samples;
			int32 LoopStartFrame = INDEX_NONE;
			int32 Pitch = FSpuAdpcm::PitchOne;
		};

		/** Mixes Frames stereo frames and queues them on the output. */
		void Stream(int32 Frames)
		{
			if (Frames <= 0 || !Output)
			{
				return;
			}
			Chunk.SetNumUninitialized(Frames * 2, false);
			Mixer.Mix(Chunk.GetData(), Frames);
			Output->Queue(Chunk.GetData(), Frames);
		}

		FSoftwareAudioMixer Mixer{OutputRate};
		TUniquePtr<FAudioOutput> Output;
		/** One per buffer slot, made once: the mixer's voices point into them. */
		TArray<FDecodedBuffer> Buffers;
		int32 VoiceBuffers[FSoftwareAudioMixer::MaxVoices] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			INDEX_NONE, INDEX_NONE};
		TArray<int16> Chunk;
		/**
		 * When the last tick ran (FPlatformTime::Cycles64), and the output time owed but not yet mixed, in frames times
		 * a million microseconds (the fraction of a frame a tick leaves).
		 */
		uint64 LastTickCycles = 0;
		uint64 PendingFrameMicroseconds = 0;
	};

	static_assert(FSoftwareAudioMixer::MaxVoices == FAudioDevice::NumVoices, "one mixer voice per hardware voice");

} // namespace

void SetAudioOutputFactory(FAudioOutputFactory Factory)
{
	GAudioOutputFactory = Factory;
}

TUniquePtr<FAudioHardware> CreatePlatformAudioHardware()
{
	return MakeUnique<FSoftwareAudioHardware>();
}
