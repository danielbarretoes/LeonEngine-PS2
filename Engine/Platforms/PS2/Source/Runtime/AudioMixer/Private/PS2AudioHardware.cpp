#include "AudioDevice.h"
#include "AudioHardware.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"

#include <audsrv.h>
#include <kernel.h>
#include <loadfile.h>

DEFINE_LOG_CATEGORY_STATIC(LogAudioHardware, Log, All);

namespace
{

	/** The header audsrv_load_adpcm reads before the blocks (ps2sdk's adpenc "APCM"): it skips 16 bytes. */
	constexpr int32 HeaderBytes = 16;

	/**
	 * The PS2's sound hardware (Docs/PLANS/ps2-shipping.md N19): the SPU2's core 1 voices through audsrv's ADPCM calls,
	 * the IOP module the build copies beside the ELF (RUNTIME_DEPENDENCIES: $PS2SDK/iop/irx/audsrv.irx), over the ROM's
	 * LIBSD. A buffer is uploaded once to SPU2 RAM (audsrv_load_adpcm places each after the last; audsrv_free_adpcm
	 * gives the room of the last ones back); a play sets the voice's volume, then keys it on at the buffer's pitch
	 * (audsrv_ch_play_adpcm, which refuses a voice whose end flag the SPU2 has not reached). Each call is a SIF RPC to
	 * the IOP: the device only calls when something changes. Nothing is mixed on the EE.
	 */
	class FPS2AudioHardware final : public FAudioHardware
	{
	public:
		~FPS2AudioHardware() override
		{
			if (bStarted)
			{
				(void)audsrv_adpcm_init();
				audsrv_quit();
			}
		}

		bool Start() override
		{
			// The SIF RPC and the patch that loads a module from EE memory (a no-op when the launcher did it).
			FPlatformMisc::InitializeIop(false);
			if (SifLoadModule("rom0:LIBSD", 0, nullptr) < 0)
			{
				UE_LOG(LogAudioHardware, Warning, "PS2 audio: rom0:LIBSD did not load");
				return false;
			}
			const FString IrxPath = FString(FPlatformProcess::BaseDir()) + "audsrv.irx";
			TArray<uint8> Irx;
			if (!FFileHelper::LoadFileToArray(Irx, *IrxPath))
			{
				UE_LOG(LogAudioHardware, Warning, "PS2 audio: no '%s' (the build copies it from $PS2SDK/iop/irx)",
					*IrxPath);
				return false;
			}
			// The SIF DMA reads the module from EE memory: 64-byte aligned.
			void* Aligned = FMemory::Malloc(SIZE_T(Irx.Num()), 64);
			FMemory::Memcpy(Aligned, Irx.GetData(), SIZE_T(Irx.Num()));
			int ModuleResult = 0;
			const int Module = SifExecModuleBuffer(Aligned, u32(Irx.Num()), 0, nullptr, &ModuleResult);
			FMemory::Free(Aligned);
			if (Module < 0 || ModuleResult < 0)
			{
				UE_LOG(LogAudioHardware, Warning, "PS2 audio: audsrv.irx did not start (%d, %d)", Module, ModuleResult);
				return false;
			}
			if (audsrv_init() != 0)
			{
				UE_LOG(LogAudioHardware, Warning, "PS2 audio: audsrv_init failed (%d)", audsrv_get_error());
				return false;
			}
			bStarted = true;
			// Every voice off and no sample loaded (audsrv's stream stays silent: nothing is queued on it).
			if (audsrv_adpcm_init() != 0)
			{
				UE_LOG(LogAudioHardware, Warning, "PS2 audio: audsrv_adpcm_init failed (%d)", audsrv_get_error());
				return false;
			}
			UE_LOG(LogAudioHardware, Log, "PS2 audio: the SPU2's voices through audsrv");
			return true;
		}

		bool UploadSound(int32 Buffer, const FSpuAdpcmSound& Sound, int32 Pitch) override
		{
			// audsrv's header, then the blocks, 64-byte aligned for the SIF DMA and written back from the data cache.
			const int32 Size = HeaderBytes + Sound.GetNumBytes();
			uint8* Data = static_cast<uint8*>(FMemory::Malloc(SIZE_T(Size), 64));
			FMemory::Memzero(Data, HeaderBytes);
			FMemory::Memcpy(Data, "APCM", 4);
			Data[4] = 1;
			Data[5] = 1;
			Data[6] = Sound.IsLooping() ? 1 : 0;
			const uint32 HeaderWords[2] = {uint32(Pitch), uint32(Sound.GetNumFrames())};
			FMemory::Memcpy(Data + 8, HeaderWords, sizeof(HeaderWords));
			FMemory::Memcpy(Data + HeaderBytes, Sound.Blocks, SIZE_T(Sound.GetNumBytes()));
			SyncDCache(Data, Data + Size);
			audsrv_adpcm_t& Sample = Samples[Buffer];
			FMemory::Memzero(&Sample, sizeof(Sample));
			const int Result = audsrv_load_adpcm(&Sample, Data, Size);
			FMemory::Free(Data);
			if (Result != 0)
			{
				UE_LOG(LogAudioHardware, Error, "PS2 audio: audsrv_load_adpcm of %d bytes failed (%d)", Size, Result);
				return false;
			}
			return true;
		}

		void FreeSound(int32 Buffer) override
		{
			(void)audsrv_free_adpcm(&Samples[Buffer]);
		}

		bool PlayVoice(int32 Voice, int32 Buffer, const FSpuVoiceVolume& Volume) override
		{
			// The volume first: a voice keys on at the level it has.
			SetVoiceVolume(Voice, Volume);
			const int Channel = audsrv_ch_play_adpcm(Voice, &Samples[Buffer]);
			if (Channel != Voice)
			{
				UE_LOG(LogAudioHardware, Verbose, "PS2 audio: voice %d is still playing (%d)", Voice, Channel);
				return false;
			}
			return true;
		}

		void SetVoiceVolume(int32 Voice, const FSpuVoiceVolume& Volume) override
		{
			(void)audsrv_adpcm_set_volume_and_pan(Voice, Volume.Volume, Volume.Pan);
		}

		void StopVoice(int32 Voice) override
		{
			// audsrv has no key off: the voice plays on silently until its end flag.
			(void)audsrv_adpcm_set_volume_and_pan(Voice, 0, 0);
		}

	private:
		/** audsrv names a sample by its descriptor's address: one per buffer slot, never moved. */
		audsrv_adpcm_t Samples[FAudioDevice::MaxSoundBuffers] = {};
		bool bStarted = false;
	};

} // namespace

TUniquePtr<FAudioHardware> CreatePlatformAudioHardware()
{
	return MakeUnique<FPS2AudioHardware>();
}
