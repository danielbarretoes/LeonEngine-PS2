#include "HAL/PlatformProperties.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "SpuAdpcm.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargetPlatformManager, Log, All);

namespace
{
	/**
	 * Win64 (UE: TGenericWindowsTargetPlatform, the WindowsNoEditor flavor): the Windows game, cooked with the PS2's
	 * formats (Docs/PLANS/ps2-preview.md D1): paletted textures, SPU2 ADPCM sounds (which the desktop decodes and
	 * mixes: Docs/PLANS/ps2-shipping.md N19), the Leon mesh and package formats.
	 */
	class FWin64TargetPlatform final : public ITargetPlatform
	{
	public:
		virtual FString PlatformName() const override
		{
			return TEXT("Win64");
		}
		virtual FString DisplayName() const override
		{
			return TEXT("Windows (64-bit)");
		}
		virtual FString IniPlatformName() const override
		{
			return TEXT("Windows");
		}
		virtual bool HasEditorOnlyData() const override
		{
			return false;
		}
		virtual bool IsLittleEndian() const override
		{
			return true;
		}
		virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
		{
			// The PS2's paletted textures: the Windows game shows and holds what the console does
			// (Docs/PLANS/ps2-preview.md V1).
			OutFormats.AddUnique(FName(TEXT("Paletted")));
		}
		virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
		{
			// The SPU2's: the Windows game plays what the console plays.
			OutFormats.AddUnique(FName(FSpuAdpcm::FormatName));
		}
		virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override
		{
			// The desktop GS emulator's GLSL (Engine/Shaders), compiled by the OpenGL driver.
			OutFormats.AddUnique(FName(TEXT("GLSL_330")));
		}
	};

	/**
	 * PS2 (Leon; UE's console platforms live in platform extensions; Docs/PLANS/ps2-engine.md E3): the textures cook
	 * to the GS's indexed formats (PSMT8 / PSMT4 with a CLUT, the "Paletted" format), with a VRAM report per map; the
	 * sounds to the SPU2's ADPCM (Docs/PLANS/ps2-shipping.md N19), with an SPU2 RAM report; the meshes are LPS2 v2 on
	 * every platform (ps2-shipping D1).
	 */
	class FPS2TargetPlatform final : public ITargetPlatform
	{
	public:
		virtual FString PlatformName() const override
		{
			return TEXT("PS2");
		}
		virtual FString DisplayName() const override
		{
			return TEXT("PlayStation 2");
		}
		virtual FString IniPlatformName() const override
		{
			return TEXT("PS2");
		}
		virtual bool HasEditorOnlyData() const override
		{
			return false;
		}
		virtual bool IsLittleEndian() const override
		{
			// The Emotion Engine (R5900) runs little-endian.
			return true;
		}
		virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
		{
			OutFormats.AddUnique(FName(TEXT("Paletted")));
		}
		virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
		{
			OutFormats.AddUnique(FName(FSpuAdpcm::FormatName));
		}
		virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override
		{
			// The GS draws the renderer's command lists as they are: no shaders.
			(void)OutFormats;
		}
		virtual FString GetCookNote() const override
		{
			return TEXT(
				"PS2: paletted textures (PSMT8 / PSMT4, at most 256 x 256) and SPU2 ADPCM sounds, as Win64's; the "
				"meshes are LPS2 v2 on every platform");
		}
	};

	/** The module: owns the platforms (UE: FTargetPlatformManagerModule). */
	class FTargetPlatformManagerModule final : public ITargetPlatformManagerModule
	{
	public:
		virtual void StartupModule() override
		{
			// Sorted by name.
			Owned.Add(MakeUnique<FPS2TargetPlatform>());
			Owned.Add(MakeUnique<FWin64TargetPlatform>());
			for (const TUniquePtr<ITargetPlatform>& Platform : Owned)
			{
				Platforms.Add(Platform.Get());
			}
		}

		virtual void ShutdownModule() override
		{
			ActivePlatforms.Reset();
			Platforms.Reset();
			Owned.Reset();
			bActiveInitialized = false;
		}

		virtual const TArray<ITargetPlatform*>& GetTargetPlatforms() override
		{
			return Platforms;
		}

		virtual ITargetPlatform* FindTargetPlatform(const FString& Name) override
		{
			for (ITargetPlatform* Platform : Platforms)
			{
				if (Platform->PlatformName().Equals(Name, ESearchCase::IgnoreCase))
				{
					return Platform;
				}
			}
			return nullptr;
		}

		virtual const TArray<ITargetPlatform*>& GetActiveTargetPlatforms() override
		{
			if (!bActiveInitialized)
			{
				bActiveInitialized = true;
				FString Names;
				if (FParse::Value(FCommandLine::Get(), TEXT("TargetPlatform="), Names))
				{
					TArray<FString> Split;
					Names.ParseIntoArray(Split, TEXT("+"), true);
					for (const FString& Name : Split)
					{
						if (ITargetPlatform* Platform = FindTargetPlatform(Name))
						{
							ActivePlatforms.AddUnique(Platform);
						}
						else
						{
							UE_LOG(LogTargetPlatformManager, Error, "Unknown target platform '%s'", *Name);
						}
					}
				}
				else if (ITargetPlatform* Running = GetRunningTargetPlatform())
				{
					ActivePlatforms.Add(Running);
				}
			}
			return ActivePlatforms;
		}

		virtual ITargetPlatform* GetRunningTargetPlatform() override
		{
			return FindTargetPlatform(FString(FPlatformProperties::PlatformName()));
		}

	private:
		TArray<TUniquePtr<ITargetPlatform>> Owned;
		TArray<ITargetPlatform*> Platforms;
		TArray<ITargetPlatform*> ActivePlatforms;
		bool bActiveInitialized = false;
	};
} // namespace

ITargetPlatformManagerModule* GetTargetPlatformManager()
{
	return FModuleManager::GetModulePtr<ITargetPlatformManagerModule>("TargetPlatform");
}

ITargetPlatformManagerModule& GetTargetPlatformManagerRef()
{
	return FModuleManager::LoadModuleChecked<ITargetPlatformManagerModule>("TargetPlatform");
}

IMPLEMENT_MODULE(FTargetPlatformManagerModule, TargetPlatform)
