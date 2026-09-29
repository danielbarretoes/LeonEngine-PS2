#include "Desktop/AudioOutput.h"

#include <miniaudio.h>

DEFINE_LOG_CATEGORY_STATIC(LogAudioOutput, Log, All);

namespace
{

	/**
	 * The desktop's output: a playback device of miniaudio that plays the frames queued in its ring buffer
	 * (ma_pcm_rb: one writer, the game thread; one reader, miniaudio's audio thread; no lock). A device that runs dry
	 * plays silence until the next tick queues more, as the SPU2 does.
	 */
	class FMiniAudioOutput final : public FAudioOutput
	{
	public:
		/** Half a second of frames: room for the lead and a few slow ticks. */
		static constexpr ma_uint32 RingFrames = 24000;

		~FMiniAudioOutput() override
		{
			if (bDeviceOk)
			{
				ma_device_uninit(&Device);
			}
			if (bRingOk)
			{
				ma_pcm_rb_uninit(&Ring);
			}
		}

		bool Start(int32 SampleRate) override
		{
			if (ma_pcm_rb_init(ma_format_s16, 2, RingFrames, nullptr, nullptr, &Ring) != MA_SUCCESS)
			{
				UE_LOG(LogAudioOutput, Warning, "miniaudio: the ring buffer failed");
				return false;
			}
			bRingOk = true;
			ma_device_config Config = ma_device_config_init(ma_device_type_playback);
			Config.playback.format = ma_format_s16;
			Config.playback.channels = 2;
			Config.sampleRate = ma_uint32(SampleRate);
			Config.dataCallback = &FMiniAudioOutput::OnData;
			Config.pUserData = this;
			const ma_result Result = ma_device_init(nullptr, &Config, &Device);
			if (Result != MA_SUCCESS)
			{
				UE_LOG(LogAudioOutput, Warning, "miniaudio: no playback device (%d)", int32(Result));
				return false;
			}
			bDeviceOk = true;
			if (ma_device_start(&Device) != MA_SUCCESS)
			{
				UE_LOG(LogAudioOutput, Warning, "miniaudio: the device did not start");
				return false;
			}
			return true;
		}

		void Queue(const int16* Frames, int32 NumFrames) override
		{
			ma_uint32 Remaining = ma_uint32(NumFrames);
			while (Remaining > 0)
			{
				ma_uint32 Size = Remaining;
				void* Buffer = nullptr;
				if (ma_pcm_rb_acquire_write(&Ring, &Size, &Buffer) != MA_SUCCESS || Size == 0)
				{
					return;
				}
				FMemory::Memcpy(Buffer, Frames, SIZE_T(Size) * 2 * sizeof(int16));
				(void)ma_pcm_rb_commit_write(&Ring, Size);
				Frames += Size * 2;
				Remaining -= Size;
			}
		}

	private:
		static void OnData(ma_device* InDevice, void* Output, const void* /*Input*/, ma_uint32 FrameCount)
		{
			FMiniAudioOutput& Self = *static_cast<FMiniAudioOutput*>(InDevice->pUserData);
			int16* Out = static_cast<int16*>(Output);
			ma_uint32 Remaining = FrameCount;
			while (Remaining > 0)
			{
				ma_uint32 Size = Remaining;
				void* Buffer = nullptr;
				if (ma_pcm_rb_acquire_read(&Self.Ring, &Size, &Buffer) != MA_SUCCESS || Size == 0)
				{
					break;
				}
				FMemory::Memcpy(Out, Buffer, SIZE_T(Size) * 2 * sizeof(int16));
				(void)ma_pcm_rb_commit_read(&Self.Ring, Size);
				Out += Size * 2;
				Remaining -= Size;
			}
			FMemory::Memzero(Out, SIZE_T(Remaining) * 2 * sizeof(int16));
		}

		ma_pcm_rb Ring{};
		ma_device Device{};
		bool bRingOk = false;
		bool bDeviceOk = false;
	};

} // namespace

TUniquePtr<FAudioOutput> CreatePlatformAudioOutput()
{
	return MakeUnique<FMiniAudioOutput>();
}
