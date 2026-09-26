#include "AudioOutput.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"

#include <audsrv.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>

DEFINE_LOG_CATEGORY_STATIC(LogAudioOutput, Log, All);

namespace
{

	/**
	 * The PS2's output (Docs/PLANS/ps2-engine.md E5): the SPU2 through audsrv, the IOP module the build copies beside
	 * the ELF (RUNTIME_DEPENDENCIES: $PS2SDK/iop/irx/audsrv.irx), over the ROM's LIBSD. audsrv queues what its ring
	 * holds and drops the rest.
	 */
	class FPS2AudioOutput final : public FAudioOutput
	{
	public:
		~FPS2AudioOutput() override
		{
			if (bStarted)
			{
				audsrv_stop_audio();
				audsrv_quit();
			}
		}

		bool Start(int32 SampleRate) override
		{
			SifInitRpc(0);
			// The ROM's LOADFILE cannot load a module from EE memory without this patch (older consoles).
			sbv_patch_enable_lmb();
			if (SifLoadModule("rom0:LIBSD", 0, nullptr) < 0)
			{
				UE_LOG(LogAudioOutput, Warning, "PS2 audio: rom0:LIBSD did not load");
				return false;
			}
			const FString IrxPath = FString(FPlatformProcess::BaseDir()) + "audsrv.irx";
			TArray<uint8> Irx;
			if (!FFileHelper::LoadFileToArray(Irx, *IrxPath))
			{
				UE_LOG(
					LogAudioOutput, Warning, "PS2 audio: no '%s' (the build copies it from $PS2SDK/iop/irx)", *IrxPath);
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
				UE_LOG(LogAudioOutput, Warning, "PS2 audio: audsrv.irx did not start (%d, %d)", Module, ModuleResult);
				return false;
			}
			if (audsrv_init() != 0)
			{
				UE_LOG(LogAudioOutput, Warning, "PS2 audio: audsrv_init failed (%d)", audsrv_get_error());
				return false;
			}
			bStarted = true;
			audsrv_fmt_t Format;
			Format.freq = SampleRate;
			Format.bits = 16;
			Format.channels = 2;
			if (audsrv_set_format(&Format) != 0)
			{
				UE_LOG(LogAudioOutput, Warning, "PS2 audio: audsrv_set_format failed (%d)", audsrv_get_error());
				return false;
			}
			audsrv_set_volume(MAX_VOLUME);
			UE_LOG(LogAudioOutput, Log, "PS2 audio: audsrv on the SPU2");
			return true;
		}

		void Queue(const int16* Frames, int32 NumFrames) override
		{
			(void)audsrv_play_audio(reinterpret_cast<const char*>(Frames), NumFrames * 2 * int32(sizeof(int16)));
		}

	private:
		bool bStarted = false;
	};

} // namespace

TUniquePtr<FAudioOutput> CreatePlatformAudioOutput()
{
	return MakeUnique<FPS2AudioOutput>();
}
