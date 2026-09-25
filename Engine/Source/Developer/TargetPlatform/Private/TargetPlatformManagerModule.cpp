#include "HAL/PlatformProperties.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargetPlatformManager, Log, All);

namespace
{
	/**
	 * Win64 (UE: TGenericWindowsTargetPlatform, the WindowsNoEditor flavor): the identity target. Its content has
	 * the formats the desktop runtime loads today (BGRA8 textures, PCM16 sounds, the Leon mesh and package formats).
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
			OutFormats.AddUnique(FName(TEXT("BGRA8")));
		}
		virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
		{
			OutFormats.AddUnique(FName(TEXT("PCM")));
		}
	};

	/**
	 * PS2 (Leon; UE's console platforms live in platform extensions): a stub until the Engine port. It cooks the same
	 * formats as Win64, and the cook says so: the PS2 conversions (PSMT8 / PSMT4 textures with a CLUT, LPS2 v2
	 * meshes, ADPCM sounds, a pak aligned for cdrom0:) are a later milestone.
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
			OutFormats.AddUnique(FName(TEXT("BGRA8")));
		}
		virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
		{
			OutFormats.AddUnique(FName(TEXT("PCM")));
		}
		virtual FString GetCookNote() const override
		{
			return TEXT("PS2 is a stub target: its content keeps the Win64 formats; the PS2 conversion (PSMT8/PSMT4 "
						"textures, LPS2 v2 meshes, ADPCM audio) is a later milestone");
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
