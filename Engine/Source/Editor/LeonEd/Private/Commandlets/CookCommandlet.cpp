#include "Commandlets/CookCommandlet.h"

#include "AssetImportUtils.h"
#include "Commandlets/ResavePackagesCommandlet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GSDrawEnvironment.h"
#include "GSTextureLayout.h"
#include "HAL/FileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "LeonEdLog.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PalettedTexture.h"
#include "Sound/SoundWave.h"
#include "SpuAdpcm.h"
#include "StaticLightingSystem.h"
#include "Templates/UniquePtr.h"
#include "UObject/GarbageCollection.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectVersion.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogCook, Log, All);

namespace
{
	/** A texture's data as loaded, put back after its package is saved converted. */
	struct FTextureBackup
	{
		UTexture2D* Texture = nullptr;
		int32 SizeX = 0;
		int32 SizeY = 0;
		EPixelFormat Format = PF_Unknown;
		TArray<uint8> Data;
	};

	/** Whether the platform's textures are paletted (the PS2). */
	[[nodiscard]] bool WantsPalettedTextures(const ITargetPlatform& TargetPlatform)
	{
		TArray<FName> Formats;
		TargetPlatform.GetAllTextureFormats(Formats);
		return Formats.Contains(FName(TEXT("Paletted")));
	}

	/**
	 * Converts an RGBA8 texture to PF_P8 / PF_P4 in place, keeping what it was in OutBackup; false (and unchanged)
	 * for a texture without RGBA8 texels.
	 */
	bool MakePaletted(UTexture2D& Texture, FTextureBackup& OutBackup, FCookedTexture& OutInfo)
	{
		const FTexturePlatformData& Data = Texture.GetPlatformData();
		if (!Texture.HasValidPlatformData() || GetPixelFormatBytes(Data.PixelFormat) != 4)
		{
			return false;
		}
		const int64 NumBytes = GetPixelFormatDataSize(Data.PixelFormat, Data.SizeX, Data.SizeY);
		OutBackup.Texture = &Texture;
		OutBackup.SizeX = Data.SizeX;
		OutBackup.SizeY = Data.SizeY;
		OutBackup.Format = Data.PixelFormat;
		OutBackup.Data.SetNumUninitialized(int32(NumBytes));
		const FByteBulkData& BulkData = Data.Mips[0].BulkData;
		FMemory::Memcpy(OutBackup.Data.GetData(), BulkData.LockReadOnly(), SIZE_T(NumBytes));
		BulkData.Unlock();

		TArray<uint8> Rgba = OutBackup.Data;
		if (Data.PixelFormat == PF_B8G8R8A8)
		{
			for (int32 Index = 0; Index < Rgba.Num(); Index += 4)
			{
				Swap(Rgba[Index], Rgba[Index + 2]);
			}
		}
		FPalettedTexture Paletted;
		if (!FPalettedTextureBuilder::Build(Rgba.GetData(), Data.SizeX, Data.SizeY, Texture.SRGB != 0, Paletted))
		{
			return false;
		}
		OutInfo.Name = Texture.GetPathName();
		OutInfo.SourceSizeX = Data.SizeX;
		OutInfo.SourceSizeY = Data.SizeY;
		OutInfo.SizeX = Paletted.SizeX;
		OutInfo.SizeY = Paletted.SizeY;
		OutInfo.Format = Paletted.Format;
		OutInfo.SourceColors = Paletted.NumSourceColors;
		OutInfo.NumMips = 1 + Paletted.Mips.Num();
		// The blocks the texture cache takes for it: its levels at their alignment and its CLUT.
		const EGSPixelFormat GSFormat = Paletted.Format == PF_P4 ? EGSPixelFormat::PSMT4 : EGSPixelFormat::PSMT8;
		FGSTextureLayout::FFootprint Footprint;
		FGSTextureLayout::GetFootprint(
			GSFormat, uint32(Paletted.SizeX), uint32(Paletted.SizeY), OutInfo.NumMips, Footprint);
		OutInfo.Blocks = Footprint.NumBlocks;
		if (!Texture.SetPlatformData(Paletted.SizeX, Paletted.SizeY, Paletted.Format, Paletted.Data.GetData()))
		{
			return false;
		}
		for (const TArray<uint8>& Mip : Paletted.Mips)
		{
			if (!Texture.AddMip(Mip.GetData()))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Describes a texture that is paletted already (a map's overview, made PF_P8 by LeonEd: the cook keeps it as it
	 * is) for the reports and budgets; false for any other.
	 */
	bool DescribePaletted(const UTexture2D& Texture, FCookedTexture& OutInfo)
	{
		const FTexturePlatformData& Data = Texture.GetPlatformData();
		if (!Texture.HasValidPlatformData() || (Data.PixelFormat != PF_P8 && Data.PixelFormat != PF_P4))
		{
			return false;
		}
		OutInfo.Name = Texture.GetPathName();
		OutInfo.SourceSizeX = Data.SizeX;
		OutInfo.SourceSizeY = Data.SizeY;
		OutInfo.SizeX = Data.SizeX;
		OutInfo.SizeY = Data.SizeY;
		OutInfo.Format = Data.PixelFormat;
		OutInfo.NumMips = Texture.GetNumMips();
		// The colours its texels use: the palette's entries, counted from the indices of level 0 (after the CLUT).
		const int64 NumBytes = GetPixelFormatDataSize(Data.PixelFormat, Data.SizeX, Data.SizeY);
		const int64 NumTexels = int64(Data.SizeX) * Data.SizeY;
		const int64 IndexBytes = Data.PixelFormat == PF_P4 ? (NumTexels + 1) / 2 : NumTexels;
		bool bUsed[256] = {};
		const uint8* Bytes = static_cast<const uint8*>(Data.Mips[0].BulkData.LockReadOnly());
		for (int64 Index = NumBytes - IndexBytes; Index < NumBytes; ++Index)
		{
			if (Data.PixelFormat == PF_P4)
			{
				bUsed[Bytes[Index] & 0x0f] = true;
				bUsed[Bytes[Index] >> 4] = true;
			}
			else
			{
				bUsed[Bytes[Index]] = true;
			}
		}
		Data.Mips[0].BulkData.Unlock();
		OutInfo.SourceColors = 0;
		for (const bool bEntry : bUsed)
		{
			OutInfo.SourceColors += bEntry ? 1 : 0;
		}
		const EGSPixelFormat GSFormat = Data.PixelFormat == PF_P4 ? EGSPixelFormat::PSMT4 : EGSPixelFormat::PSMT8;
		FGSTextureLayout::FFootprint Footprint;
		FGSTextureLayout::GetFootprint(GSFormat, uint32(Data.SizeX), uint32(Data.SizeY), OutInfo.NumMips, Footprint);
		OutInfo.Blocks = Footprint.NumBlocks;
		return true;
	}

	/** Whether the platform plays the SPU2's ADPCM (both Leon platforms). */
	[[nodiscard]] bool WantsSpuAdpcmSounds(const ITargetPlatform& TargetPlatform)
	{
		TArray<FName> Formats;
		TargetPlatform.GetAllWaveFormats(Formats);
		return Formats.Contains(FName(FSpuAdpcm::FormatName));
	}

	/** KB of a byte count, rounded up. */
	[[nodiscard]] FString BytesToKB(int64 Bytes)
	{
		return FString::Printf("%lld KB", static_cast<long long>((Bytes + 1023) / 1024));
	}

	/** KB of a block count (a block is 256 bytes), rounded up: a texture of a few blocks is not 0 KB. */
	[[nodiscard]] FString BlocksToKB(uint32 Blocks)
	{
		return FString::Printf("%u KB", ((Blocks * FGSTextureLayout::BytesPerBlock) + 1023) / 1024);
	}

	/** The packaging settings' section (UE: UProjectPackagingSettings in UnrealEd, Config=Game). */
	const TCHAR* const PackagingSettingsSection = TEXT("/Script/UnrealEd.ProjectPackagingSettings");

	/** Sorted by name, ignoring case like the package names themselves. */
	void SortNames(TArray<FString>& Names)
	{
		Names.Sort([](const FString& A, const FString& B)
			{ return A.ToLower().Compare(B.ToLower(), ESearchCase::CaseSensitive) < 0; });
	}

	/**
	 * A packaging setting's value to its path: UE's struct text `(Path="/Game/Props")` / `(FilePath="/Game/Maps/A")`,
	 * or a bare path.
	 */
	FString ParseStructPath(const FString& Value)
	{
		FString Result = Value.TrimStartAndEnd();
		if (Result.StartsWith(TEXT("(")))
		{
			FString Inner;
			if (!FParse::Value(*Result, TEXT("FilePath="), Inner) && !FParse::Value(*Result, TEXT("Path="), Inner))
			{
				return FString();
			}
			Result = Inner;
		}
		while (Result.EndsWith(TEXT(")")) || Result.EndsWith(TEXT("\"")))
		{
			Result.LeftChopInline(1);
		}
		return Result.TrimStartAndEnd();
	}

	/**
	 * The package a config value names: an object path ("/Engine/EngineMaterials/M_Default.M_Default") or a long
	 * package name under a mount point; empty for anything else (a /Script class, a plain value).
	 */
	FString ConfigValueToPackage(const FString& Value)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		if (!Trimmed.StartsWith(TEXT("/")) || FPackageName::IsScriptPackage(Trimmed))
		{
			return FString();
		}
		const FString PackageName = FPackageName::ObjectPathToPackageName(Trimmed);
		return FPackageName::IsValidLongPackageName(PackageName) ? PackageName : FString();
	}

	/** The maps (.lmap packages) under a long package path. */
	void FindMaps(const FString& PackagePath, TArray<FString>& OutMaps)
	{
		TArray<FString> Packages;
		FAssetImportUtils::FindPackages(PackagePath, Packages);
		for (const FString& Package : Packages)
		{
			FString Filename;
			if (FPackageName::DoesPackageExist(Package, nullptr, &Filename) &&
				Filename.EndsWith(FPackageName::GetMapPackageExtension()))
			{
				OutMaps.AddUnique(Package);
			}
		}
	}

	/** Copies a file byte for byte; false (logged) when it cannot be read or written. */
	bool StageFile(const FString& Source, const FString& Dest)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Source))
		{
			UE_LOG(LogCook, Error, "Cook: '%s' cannot be read", *Source);
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest), true);
		if (!FFileHelper::SaveArrayToFile(Bytes, *Dest))
		{
			UE_LOG(LogCook, Error, "Cook: '%s' cannot be written", *Dest);
			return false;
		}
		return true;
	}

	/**
	 * The ini files of a folder (not its sub folders) whose names start with Prefix, except the Editor ones: the
	 * editor's config never ships (UE stages no Editor ini).
	 */
	int32 StageConfigFolder(const FString& SourceDir, const TCHAR* Prefix, const FString& DestDir)
	{
		if (!IFileManager::Get().DirectoryExists(*SourceDir))
		{
			return 0;
		}
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(SourceDir / TEXT("*.ini")), true, false);
		SortNames(Files);
		int32 Staged = 0;
		for (const FString& File : Files)
		{
			const FString Name = FPaths::GetCleanFilename(File);
			if (!Name.StartsWith(Prefix) || Name.Contains(TEXT("Editor")))
			{
				continue;
			}
			// As the target reads it: no comments, no editor or cook sections (N23).
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *(SourceDir / Name)))
			{
				UE_LOG(LogCook, Error, "Cook: '%s' cannot be read", *(SourceDir / Name));
				return -1;
			}
			IFileManager::Get().MakeDirectory(*DestDir, true);
			if (!FFileHelper::SaveStringToFile(UCookCommandlet::StripConfigForTarget(Text), *(DestDir / Name)))
			{
				UE_LOG(LogCook, Error, "Cook: '%s' cannot be written", *(DestDir / Name));
				return -1;
			}
			++Staged;
		}
		return Staged;
	}
	/** Where the cook cache keeps a package: its cooked file's path under CacheDir, and the info file beside it. */
	FString GetCachedFilename(const FString& PackageName, const FString& CacheDir, const FString& Extension)
	{
		return UCookCommandlet::GetCookedFilename(PackageName, CacheDir, Extension);
	}

	/** The info file of a cached package: its key, then one tab-separated line per texture, sound and mesh. */
	FString MakeCookInfo(const FString& Key, const FCookedPackageInfo& Info)
	{
		FString Text = FString::Printf("Key\t%s\n", *Key);
		for (const FCookedTexture& Texture : Info.Textures)
		{
			Text += FString::Printf("Texture\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%u\n", *Texture.Name, Texture.SourceSizeX,
				Texture.SourceSizeY, Texture.SizeX, Texture.SizeY, int32(Texture.Format), Texture.SourceColors,
				Texture.NumMips, Texture.Blocks);
		}
		for (const FCookedSound& Sound : Info.Sounds)
		{
			Text += FString::Printf("Sound\t%s\t%d\t%d\t%d\t%d\t%d\n", *Sound.Name, Sound.SourceSampleRate,
				Sound.SampleRate, Sound.NumFrames, Sound.Bytes, Sound.bLooping ? 1 : 0);
		}
		for (const FCookedMesh& Mesh : Info.Meshes)
		{
			Text += FString::Printf("Mesh\t%s\t%d\t%d\n", *Mesh.Name, Mesh.Triangles, Mesh.Bones);
		}
		if (Info.UnbakedLighting > 0)
		{
			Text += FString::Printf("UnbakedLighting\t%d\n", Info.UnbakedLighting);
		}
		return Text;
	}

	/** Reads an info file MakeCookInfo wrote; false when it is not one, or its key is not Key. */
	bool ParseCookInfo(const FString& Text, const FString& Key, FCookedPackageInfo& OutInfo)
	{
		OutInfo = FCookedPackageInfo();
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, true);
		if (Lines.Num() == 0 || Lines[0] != TEXT("Key\t") + Key)
		{
			return false;
		}
		for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
		{
			TArray<FString> Fields;
			Lines[LineIndex].ParseIntoArray(Fields, TEXT("\t"), false);
			const auto Int = [&Fields](int32 Index) { return FCString::Atoi(*Fields[Index]); };
			if (Fields.Num() == 10 && Fields[0] == TEXT("Texture"))
			{
				FCookedTexture& Texture = OutInfo.Textures.AddDefaulted_GetRef();
				Texture.Name = Fields[1];
				Texture.SourceSizeX = Int(2);
				Texture.SourceSizeY = Int(3);
				Texture.SizeX = Int(4);
				Texture.SizeY = Int(5);
				Texture.Format = EPixelFormat(Int(6));
				Texture.SourceColors = Int(7);
				Texture.NumMips = Int(8);
				Texture.Blocks = uint32(Int(9));
			}
			else if (Fields.Num() == 7 && Fields[0] == TEXT("Sound"))
			{
				FCookedSound& Sound = OutInfo.Sounds.AddDefaulted_GetRef();
				Sound.Name = Fields[1];
				Sound.SourceSampleRate = Int(2);
				Sound.SampleRate = Int(3);
				Sound.NumFrames = Int(4);
				Sound.Bytes = Int(5);
				Sound.bLooping = Int(6) != 0;
			}
			else if (Fields.Num() == 2 && Fields[0] == TEXT("UnbakedLighting"))
			{
				OutInfo.UnbakedLighting = Int(1);
			}
			else if (Fields.Num() == 4 && Fields[0] == TEXT("Mesh"))
			{
				FCookedMesh& Mesh = OutInfo.Meshes.AddDefaulted_GetRef();
				Mesh.Name = Fields[1];
				Mesh.Triangles = Int(2);
				Mesh.Bones = Int(3);
			}
			else
			{
				return false;
			}
		}
		return true;
	}
} // namespace

const TCHAR* const FCookBudgets::Section = TEXT("/Script/LeonEd.CookSettings");

FCookBudgets FCookBudgets::Load(const ITargetPlatform& TargetPlatform, const TMap<FString, FString>& ParamsMap)
{
	FCookBudgets Budgets;
	const FString IniPlatform = TargetPlatform.IniPlatformName();
	FConfigFile GameConfig;
	FConfigCacheIni::LoadLocalIniFile(GameConfig, TEXT("Game"), true, *IniPlatform);
	const struct
	{
		const TCHAR* Key;
		int32* Value;
	} Settings[] = {{TEXT("MapVramKB"), &Budgets.MapVramKB}, {TEXT("MapRamKB"), &Budgets.MapRamKB},
		{TEXT("RuntimeBaseKB"), &Budgets.RuntimeBaseKB},
		{TEXT("RuntimeExpansionPercent"), &Budgets.RuntimeExpansionPercent},
		{TEXT("MapSoundRamKB"), &Budgets.MapSoundRamKB}, {TEXT("MaxMeshTriangles"), &Budgets.MaxMeshTriangles},
		{TEXT("MaxMeshBones"), &Budgets.MaxMeshBones}, {TEXT("MaxTextureSize"), &Budgets.MaxTextureSize},
		{TEXT("MaxTextureBitsPerPixel"), &Budgets.MaxTextureBitsPerPixel}};
	for (const auto& Setting : Settings)
	{
		FString Value;
		if (GameConfig.GetString(Section, Setting.Key, Value) && !Value.IsEmpty())
		{
			*Setting.Value = FCString::Atoi(*Value);
		}
		if (const FString* Param = ParamsMap.Find(Setting.Key))
		{
			*Setting.Value = FCString::Atoi(**Param);
		}
	}
	// 0: the platform's own limits.
	if (Budgets.MapVramKB <= 0)
	{
		Budgets.MapVramKB = int32((FGSDrawEnvironment::TextureArenaBlocks * FGSTextureLayout::BytesPerBlock) / 1024);
	}
	if (Budgets.MapSoundRamKB <= 0)
	{
		Budgets.MapSoundRamKB = FSpuAdpcm::SoundRamBytes / 1024;
	}
	if (Budgets.MapRamKB <= 0)
	{
		FConfigFile EngineConfig;
		FConfigCacheIni::LoadLocalIniFile(EngineConfig, TEXT("Engine"), true, *IniPlatform);
		FString Total;
		Budgets.MapRamKB =
			EngineConfig.GetString(TEXT("Core.MemoryBudgets"), TEXT("Total"), Total) ? FCString::Atoi(*Total) : 0;
	}
	Budgets.MaxTextureSize =
		FMath::Clamp(Budgets.MaxTextureSize, FPalettedTextureBuilder::MinSize, FPalettedTextureBuilder::MaxSize);
	Budgets.MaxTextureBitsPerPixel = Budgets.MaxTextureBitsPerPixel >= 8 ? 8 : 4;
	return Budgets;
}

void FCookBudgets::CheckAssets(const FCookedPackageInfo& Info, TArray<FString>& OutErrors) const
{
	for (const FCookedTexture& Texture : Info.Textures)
	{
		const int32 Bits = Texture.Format == PF_P4 ? 4 : 8;
		const int32 Size = FMath::Max(Texture.SizeX, Texture.SizeY);
		if (Size > MaxTextureSize)
		{
			OutErrors.Add(FString::Printf("%s is %dx%d, over [%s] MaxTextureSize=%d", *Texture.Name, Texture.SizeX,
				Texture.SizeY, Section, MaxTextureSize));
		}
		if (Bits > MaxTextureBitsPerPixel)
		{
			OutErrors.Add(FString::Printf("%s is %d bits a texel (PSMT%d, %d colours), over [%s] "
										  "MaxTextureBitsPerPixel=%d",
				*Texture.Name, Bits, Bits, Texture.SourceColors, Section, MaxTextureBitsPerPixel));
		}
	}
	for (const FCookedMesh& Mesh : Info.Meshes)
	{
		if (Mesh.Triangles > MaxMeshTriangles)
		{
			OutErrors.Add(FString::Printf("%s has %d triangles, over [%s] MaxMeshTriangles=%d", *Mesh.Name,
				Mesh.Triangles, Section, MaxMeshTriangles));
		}
		if (Mesh.Bones > MaxMeshBones)
		{
			OutErrors.Add(FString::Printf(
				"%s has %d bones, over [%s] MaxMeshBones=%d", *Mesh.Name, Mesh.Bones, Section, MaxMeshBones));
		}
	}
}

UCookCommandlet::UCookCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription = TEXT("Cooks the maps and what they use, with the config and the shaders, into Saved/Cooked");
	HelpUsage = TEXT("-run=Cook -TargetPlatform=Win64|PS2 [-map=<Map>+<Map>] [-package=<LongPackageName>[,...]] "
					 "[-packagefolder=<LongPackagePath>] [-iterate | -full] [-<CookSettings budget>=<value>]");
	LogToConsole = 1;
}

FString UCookCommandlet::GetProjectFolderName()
{
	return FApp::HasProjectName() ? FString(FApp::GetProjectName()) : FString(TEXT("Game"));
}

FString UCookCommandlet::GetCookedDir(const FString& PlatformName)
{
	return FPaths::ProjectSavedDir() + TEXT("Cooked/") + PlatformName + TEXT("/");
}

FString UCookCommandlet::GetCookedFilename(
	const FString& PackageName, const FString& CookedDir, const FString& Extension)
{
	FString Root;
	FString Path;
	FString Name;
	if (!FPackageName::SplitLongPackageName(PackageName, Root, Path, Name))
	{
		return FString();
	}
	// "/Engine/" -> Engine, "/Game/" -> the project, any other mount point -> its name (UE's cooked layout).
	FString Folder = Root.Mid(1, Root.Len() - 2);
	if (Folder == TEXT("Game"))
	{
		Folder = GetProjectFolderName();
	}
	FString Dir = CookedDir;
	if (!Dir.EndsWith(TEXT("/")))
	{
		Dir += TEXT("/");
	}
	return Dir + Folder + TEXT("/Content/") + Path + Name + Extension;
}

void UCookCommandlet::GatherCookSeeds(
	const ITargetPlatform& TargetPlatform, const TMap<FString, FString>& ParamsMap, TArray<FString>& OutPackages)
{
	OutPackages.Reset();
	// -package= / -packagefolder= name the packages themselves.
	if (ParamsMap.Contains(TEXT("package")) || ParamsMap.Contains(TEXT("packagefolder")))
	{
		UResavePackagesCommandlet::GatherPackages(ParamsMap, OutPackages);
		SortNames(OutPackages);
		return;
	}

	// The target's config: its layers, not the host's (UE: FConfigCacheIni::ForPlatform).
	const FString IniPlatform = TargetPlatform.IniPlatformName();
	FConfigFile EngineConfig;
	FConfigCacheIni::LoadLocalIniFile(EngineConfig, TEXT("Engine"), true, *IniPlatform);
	FConfigFile GameConfig;
	FConfigCacheIni::LoadLocalIniFile(GameConfig, TEXT("Game"), true, *IniPlatform);

	// The maps: -map=A+B, else +MapsToCook, else every map under /Game/Maps (UE cooks them all when none is listed).
	TArray<FString> Maps;
	if (const FString* MapParam = ParamsMap.Find(TEXT("map")))
	{
		TArray<FString> Names;
		MapParam->ParseIntoArray(Names, TEXT("+"), true);
		for (const FString& MapName : Names)
		{
			Maps.AddUnique(MapName.StartsWith(TEXT("/")) ? MapName : TEXT("/Game/Maps/") + MapName);
		}
	}
	else
	{
		TArray<FString> MapsToCook;
		GameConfig.GetArray(PackagingSettingsSection, TEXT("MapsToCook"), MapsToCook);
		for (const FString& Entry : MapsToCook)
		{
			const FString MapName = ParseStructPath(Entry);
			if (!MapName.IsEmpty())
			{
				Maps.AddUnique(FPackageName::ObjectPathToPackageName(MapName));
			}
		}
		if (Maps.Num() == 0)
		{
			FindMaps(TEXT("/Game/Maps"), Maps);
		}
	}
	for (const FString& Map : Maps)
	{
		OutPackages.AddUnique(Map);
	}

	// The folders cooked whole (UE: DirectoriesToAlwaysCook), the engine's included (BaseGame.ini: BasicShapes, which
	// code loads by path).
	TArray<FString> Directories;
	GameConfig.GetArray(PackagingSettingsSection, TEXT("DirectoriesToAlwaysCook"), Directories);
	for (const FString& Entry : Directories)
	{
		const FString Directory = ParseStructPath(Entry);
		TArray<FString> Packages;
		if (!Directory.IsEmpty())
		{
			FAssetImportUtils::FindPackages(Directory, Packages);
		}
		if (Packages.Num() == 0)
		{
			UE_LOG(LogCook, Warning, "Cook: DirectoriesToAlwaysCook '%s' holds no package", *Entry);
		}
		for (const FString& Package : Packages)
		{
			OutPackages.AddUnique(Package);
		}
	}

	// What the config names by path: GameDefaultMap, ServerDefaultMap, the engine's default material and textures,
	// the UI sounds... (UE cooks the soft references of the config).
	for (const FConfigFile* Config : {&EngineConfig, &GameConfig})
	{
		for (const TPair<FString, FConfigSection>& Section : *Config)
		{
			if (Section.Key == PackagingSettingsSection)
			{
				continue;
			}
			for (const TPair<FName, FConfigValue>& Value : Section.Value)
			{
				const FString Package = ConfigValueToPackage(Value.Value.GetValue());
				if (Package.IsEmpty())
				{
					continue;
				}
				if (!FPackageName::DoesPackageExist(Package))
				{
					UE_LOG(LogCook, Warning, "Cook: [%s] %s names %s, which has no file", *Section.Key,
						*Value.Key.ToString(), *Package);
					continue;
				}
				OutPackages.AddUnique(Package);
			}
		}
	}
	SortNames(OutPackages);
}

bool UCookCommandlet::CollectDependencies(const TArray<FString>& Seeds, TArray<FString>& OutPackages)
{
	OutPackages.Reset();
	struct FQueued
	{
		FString Name;
		/** Who asked for it: a seed, a hard import or a soft reference (the last is only a warning when missing). */
		FString Referencer;
		bool bSoft = false;
	};
	TArray<FQueued> Queue;
	TSet<FString> Seen;
	for (const FString& Seed : Seeds)
	{
		if (!Seen.Contains(Seed))
		{
			Seen.Add(Seed);
			Queue.Add({Seed, FString(), false});
		}
	}

	bool bSuccess = true;
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		const FQueued Current = Queue[Index];
		FString Filename;
		if (!FPackageName::DoesPackageExist(Current.Name, nullptr, &Filename) || Filename.IsEmpty())
		{
			if (Current.bSoft)
			{
				UE_LOG(LogCook, Warning, "Cook: %s refers softly to %s, which does not exist", *Current.Referencer,
					*Current.Name);
			}
			else
			{
				UE_LOG(LogCook, Error, "Cook: %s does not exist%s", *Current.Name,
					Current.Referencer.IsEmpty() ? TEXT("")
												 : *FString::Printf(TEXT(" (imported by %s)"), *Current.Referencer));
				bSuccess = false;
			}
			continue;
		}
		// The tables only (UE: the cooker's FPackageReader): the imports and soft references, nothing loaded.
		const TUniquePtr<FLinkerLoad> Tables(FLinkerLoad::CreateLinker(nullptr, *Filename, LOAD_None));
		if (!Tables)
		{
			UE_LOG(LogCook, Error, "Cook: %s is not a readable package", *Filename);
			bSuccess = false;
			continue;
		}
		OutPackages.Add(Current.Name);

		TArray<FQueued> Found;
		for (int32 ImportIndex = 0; ImportIndex < Tables->ImportMap.Num(); ++ImportIndex)
		{
			if (Tables->ImportMap[ImportIndex].OuterIndex.IsNull())
			{
				const FString Imported = Tables->GetImportPathName(ImportIndex);
				if (!FPackageName::IsScriptPackage(Imported))
				{
					Found.Add({Imported, Current.Name, false});
				}
			}
		}
		for (const FName& Soft : Tables->SoftPackageReferenceList)
		{
			const FString SoftPackage = Soft.ToString();
			if (!FPackageName::IsScriptPackage(SoftPackage))
			{
				Found.Add({SoftPackage, Current.Name, true});
			}
		}
		for (const FQueued& Next : Found)
		{
			if (!Seen.Contains(Next.Name))
			{
				Seen.Add(Next.Name);
				Queue.Add(Next);
			}
		}
	}
	SortNames(OutPackages);
	return bSuccess;
}

bool UCookCommandlet::CookPackage(const FString& PackageName, const ITargetPlatform& TargetPlatform,
	const FString& CookedDir, TArray<FCookedTexture>* OutTextures, TArray<FCookedSound>* OutSounds,
	TArray<FCookedMesh>* OutMeshes, int32* OutUnbakedLighting)
{
	FString SourceFile;
	UPackage* Package = FPackageName::DoesPackageExist(PackageName, nullptr, &SourceFile)
		? LoadPackage(nullptr, *PackageName, LOAD_None)
		: nullptr;
	const FString CookedFile = GetCookedFilename(PackageName, CookedDir, FPaths::GetExtension(SourceFile, true));
	if (Package == nullptr || CookedFile.IsEmpty())
	{
		UE_LOG(LogCook, Error, "Cook: %s cannot be loaded", *PackageName);
		return false;
	}
	// Without editor-only data: the editor-only properties are filtered, and the editor-only objects (the assets' and
	// the worlds' import data) are left out with the references to them (SavePackage, UObject::IsEditorOnly).
	Package->SetPackageFlags(PKG_FilterEditorOnly | PKG_Cooked);

	TArray<UObject*> Objects;
	GetObjectsWithOuter(Package, Objects);
	Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });

	// The meshes, for the budgets (their triangles and bones).
	for (UObject* Object : Objects)
	{
		FCookedMesh Mesh;
		if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
		{
			Mesh.Name = StaticMesh->GetPathName();
			Mesh.Triangles = StaticMesh->GetNumTriangles();
		}
		else if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
		{
			Mesh.Name = SkeletalMesh->GetPathName();
			Mesh.Triangles = SkeletalMesh->GetNumTriangles();
			Mesh.Bones = SkeletalMesh->GetRefSkeleton().GetNum();
		}
		if (!Mesh.Name.IsEmpty() && OutMeshes != nullptr)
		{
			OutMeshes->Add(Mesh);
		}
	}

	// The sounds' SPU2 ADPCM, which the cooked package keeps instead of the PCM (USoundWave::Serialize).
	for (UObject* Object : Objects)
	{
		USoundWave* Sound = Cast<USoundWave>(Object);
		if (Sound == nullptr)
		{
			continue;
		}
		if (!WantsSpuAdpcmSounds(TargetPlatform))
		{
			UE_LOG(LogCook, Error, "Cook: %s plays no sound format Leon cooks (%s)", *TargetPlatform.PlatformName(),
				FSpuAdpcm::FormatName);
			return false;
		}
		if (!Sound->CacheCompressedData())
		{
			UE_LOG(LogCook, Error, "Cook: %s has no ADPCM", *Sound->GetPathName());
			return false;
		}
		const FSpuAdpcmSound Compressed = Sound->LockCompressedData();
		FCookedSound Info;
		Info.Name = Sound->GetPathName();
		Info.SourceSampleRate = Sound->SampleRate;
		Info.SampleRate = Compressed.SampleRate;
		Info.NumFrames = Compressed.GetNumFrames();
		Info.Bytes = Compressed.GetNumBytes();
		Info.bLooping = Compressed.IsLooping();
		Sound->UnlockCompressedData();
		UE_LOG(LogCook, Display, "Cook: %s %d Hz -> SPU2 ADPCM %d Hz%s, %d frames, %d bytes", *Info.Name,
			Info.SourceSampleRate, Info.SampleRate, Info.bLooping ? TEXT(" looping") : TEXT(""), Info.NumFrames,
			Info.Bytes);
		if (OutSounds != nullptr)
		{
			OutSounds->Add(Info);
		}
	}

	// The baked lighting (N22): a Static mesh with none, or baked for an older build of its mesh, draws unlit.
	int32 NumUnbaked = 0;
	for (const UObject* Object : Objects)
	{
		const UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object);
		NumUnbaked += Component != nullptr && FStaticLightingSystem::ReceivesStaticLighting(*Component) &&
				!Component->HasValidBakedVertexColors()
			? 1
			: 0;
	}
	if (OutUnbakedLighting != nullptr)
	{
		*OutUnbakedLighting = NumUnbaked;
	}
	if (NumUnbaked > 0)
	{
		UE_LOG(LogCook, Warning,
			"Cook: %s: the lighting needs to be rebuilt for %d static mesh(es) (LeonCook -run=ResavePackages "
			"-buildlighting -package=%s)",
			*PackageName, NumUnbaked, *PackageName);
	}

	// The platform's texture format, for the save only: the loaded textures get their data back after it, since
	// another platform's cook or a later package may use them.
	TArray<FTextureBackup> Backups;
	if (WantsPalettedTextures(TargetPlatform))
	{
		for (UObject* Object : Objects)
		{
			UTexture2D* Texture = Cast<UTexture2D>(Object);
			FTextureBackup Backup;
			FCookedTexture Info;
			if (Texture != nullptr && MakePaletted(*Texture, Backup, Info))
			{
				UE_LOG(LogCook, Display, "Cook: %s %dx%d -> %dx%d %s (%d colours), %d level(s), %s", *Info.Name,
					Info.SourceSizeX, Info.SourceSizeY, Info.SizeX, Info.SizeY,
					Info.Format == PF_P4 ? TEXT("PSMT4") : TEXT("PSMT8"), Info.SourceColors, Info.NumMips,
					*BlocksToKB(Info.Blocks));
				Backups.Add(MoveTemp(Backup));
				if (OutTextures != nullptr)
				{
					OutTextures->Add(Info);
				}
			}
			else if (Texture != nullptr && DescribePaletted(*Texture, Info))
			{
				UE_LOG(LogCook, Display, "Cook: %s %dx%d %s (%d colours), %d level(s), %s, paletted already",
					*Info.Name, Info.SizeX, Info.SizeY, Info.Format == PF_P4 ? TEXT("PSMT4") : TEXT("PSMT8"),
					Info.SourceColors, Info.NumMips, *BlocksToKB(Info.Blocks));
				if (OutTextures != nullptr)
				{
					OutTextures->Add(Info);
				}
			}
		}
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CookedFile), true);
	const bool bSaved = UPackage::SavePackage(Package, nullptr, RF_Public | RF_Standalone, *CookedFile, nullptr,
		SAVE_None, *TargetPlatform.CookedPlatformName());
	for (FTextureBackup& Backup : Backups)
	{
		(void)Backup.Texture->SetPlatformData(Backup.SizeX, Backup.SizeY, Backup.Format, Backup.Data.GetData());
	}
	if (!bSaved)
	{
		UE_LOG(LogCook, Error, "Cook: saving %s to '%s' failed", *PackageName, *CookedFile);
		return false;
	}
	UE_LOG(LogCook, Display, "Cooked %s -> %s", *PackageName, *CookedFile);
	return true;
}

FString UCookCommandlet::GetCookCacheDir(const FString& PlatformName)
{
	return FPaths::ProjectIntermediateDir() + TEXT("CookCache/") + PlatformName + TEXT("/");
}

FString UCookCommandlet::GetCookKey(
	const FString& PackageName, const ITargetPlatform& TargetPlatform, FCookKeyCache& Cache)
{
	// The package and every package its hard imports reach, sorted.
	TArray<FString> Closure;
	TSet<FString> Seen;
	Closure.Add(PackageName);
	Seen.Add(PackageName);
	for (int32 Index = 0; Index < Closure.Num(); ++Index)
	{
		const FString Current = Closure[Index];
		FString Filename;
		if (!FPackageName::DoesPackageExist(Current, nullptr, &Filename) || Filename.IsEmpty())
		{
			return FString();
		}
		if (!Cache.SourceHashes.Contains(Current))
		{
			TArray<uint8> Bytes;
			Cache.SourceHashes.Add(Current,
				FFileHelper::LoadFileToArray(Bytes, *Filename)
					? FSHA1::HashBuffer(Bytes.GetData(), uint64(Bytes.Num())).ToString()
					: FString());
		}
		if (Cache.SourceHashes[Current].IsEmpty())
		{
			return FString();
		}
		if (!Cache.Imports.Contains(Current))
		{
			TArray<FString> Imports;
			const TUniquePtr<FLinkerLoad> Tables(FLinkerLoad::CreateLinker(nullptr, *Filename, LOAD_None));
			if (!Tables)
			{
				return FString();
			}
			for (int32 ImportIndex = 0; ImportIndex < Tables->ImportMap.Num(); ++ImportIndex)
			{
				if (Tables->ImportMap[ImportIndex].OuterIndex.IsNull())
				{
					const FString Imported = Tables->GetImportPathName(ImportIndex);
					if (!FPackageName::IsScriptPackage(Imported))
					{
						Imports.AddUnique(Imported);
					}
				}
			}
			Cache.Imports.Add(Current, MoveTemp(Imports));
		}
		for (const FString& Imported : Cache.Imports[Current])
		{
			if (!Seen.Contains(Imported))
			{
				Seen.Add(Imported);
				Closure.Add(Imported);
			}
		}
	}
	SortNames(Closure);

	// The cooker, the package format and the platform's settings, then each package of the closure.
	TArray<FName> TextureFormats;
	TArray<FName> WaveFormats;
	TargetPlatform.GetAllTextureFormats(TextureFormats);
	TargetPlatform.GetAllWaveFormats(WaveFormats);
	FString Text = FString::Printf("LeonCook %d, package version %d\nPlatform %s (%s), editor-only data %d, "
								   "little-endian %d\n",
		CookerVersion, int32(VER_LEON_LATEST), *TargetPlatform.PlatformName(), *TargetPlatform.CookedPlatformName(),
		TargetPlatform.HasEditorOnlyData() ? 1 : 0, TargetPlatform.IsLittleEndian() ? 1 : 0);
	for (const FName& Format : TextureFormats)
	{
		Text += TEXT("Texture format ") + Format.ToString() + TEXT("\n");
	}
	for (const FName& Format : WaveFormats)
	{
		Text += TEXT("Wave format ") + Format.ToString() + TEXT("\n");
	}
	Text += TEXT("Package ") + PackageName + TEXT("\n");
	for (const FString& Package : Closure)
	{
		Text += Package + TEXT(" ") + Cache.SourceHashes[Package] + TEXT("\n");
	}
	return FSHA1::HashBuffer(*Text, uint64(Text.Len())).ToString();
}

bool UCookCommandlet::CookPackageCached(const FString& PackageName, const ITargetPlatform& TargetPlatform,
	const FString& CookedDir, const FString& CacheDir, const FString& Key, bool bUseCache, FCookedPackageInfo& OutInfo,
	bool& bOutFromCache)
{
	bOutFromCache = false;
	OutInfo = FCookedPackageInfo();
	FString SourceFile;
	if (!FPackageName::DoesPackageExist(PackageName, nullptr, &SourceFile))
	{
		UE_LOG(LogCook, Error, "Cook: %s cannot be loaded", *PackageName);
		return false;
	}
	const FString Extension = FPaths::GetExtension(SourceFile, true);
	const FString CookedFile = GetCookedFilename(PackageName, CookedDir, Extension);
	const FString CachedFile = GetCachedFilename(PackageName, CacheDir, Extension);
	const FString InfoFile = CachedFile + TEXT(".cookinfo");
	if (bUseCache && !Key.IsEmpty() && !CookedFile.IsEmpty())
	{
		FString InfoText;
		FCookedPackageInfo Cached;
		if (FFileHelper::LoadFileToString(InfoText, *InfoFile) && ParseCookInfo(InfoText, Key, Cached) &&
			IFileManager::Get().FileExists(*CachedFile))
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(CookedFile), true);
			if (IFileManager::Get().Copy(*CookedFile, *CachedFile))
			{
				OutInfo = MoveTemp(Cached);
				bOutFromCache = true;
				UE_LOG(LogCook, Display, "Cooked %s -> %s (from the cook cache)", *PackageName, *CookedFile);
				if (OutInfo.UnbakedLighting > 0)
				{
					UE_LOG(LogCook, Warning,
						"Cook: %s: the lighting needs to be rebuilt for %d static mesh(es) (LeonCook "
						"-run=ResavePackages "
						"-buildlighting -package=%s)",
						*PackageName, OutInfo.UnbakedLighting, *PackageName);
				}
				return true;
			}
		}
	}
	if (!CookPackage(PackageName, TargetPlatform, CookedDir, &OutInfo.Textures, &OutInfo.Sounds, &OutInfo.Meshes,
			&OutInfo.UnbakedLighting))
	{
		return false;
	}
	// Into the cache for the next cook; a cache that cannot be written only costs that cook's time.
	if (!Key.IsEmpty() && !CachedFile.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(CachedFile), true);
		if (!IFileManager::Get().Copy(*CachedFile, *CookedFile) ||
			!FFileHelper::SaveStringToFile(MakeCookInfo(Key, OutInfo), *InfoFile))
		{
			UE_LOG(LogCook, Warning, "Cook: %s cannot be kept in the cook cache ('%s')", *PackageName, *CachedFile);
		}
	}
	return true;
}

FString UCookCommandlet::StripConfigForTarget(const FString& Text)
{
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, true);
	FString Result;
	bool bSkipSection = false;
	for (const FString& Line : Lines)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT(";")))
		{
			continue;
		}
		if (Trimmed.StartsWith(TEXT("[")) && Trimmed.EndsWith(TEXT("]")))
		{
			// The packaging and cook settings: only the editor and the cook read them.
			bSkipSection =
				Trimmed.StartsWith(TEXT("[/Script/UnrealEd.")) || Trimmed.StartsWith(TEXT("[/Script/LeonEd."));
		}
		if (!bSkipSection)
		{
			Result += Trimmed + TEXT("\n");
		}
	}
	return Result;
}

FString UCookCommandlet::MakeVramReport(const TArray<FString>& Maps, const TArray<FString>& Common,
	const TMap<FString, TArray<FCookedTexture>>& TexturesByPackage, uint32 ArenaBlocks, TArray<FString>& OutOverBudget)
{
	// The textures of Seeds' closure that are not in Exclude, sorted by name, and their blocks.
	const auto GatherTextures = [&TexturesByPackage](const TArray<FString>& Seeds, const TSet<FString>& Exclude,
									TArray<FCookedTexture>& OutTextures, TSet<FString>& OutNames)
	{
		TArray<FString> Closure;
		if (Seeds.Num() > 0)
		{
			(void)CollectDependencies(Seeds, Closure);
		}
		uint32 Blocks = 0;
		for (const FString& Package : Closure)
		{
			if (const TArray<FCookedTexture>* Found = TexturesByPackage.Find(Package))
			{
				for (const FCookedTexture& Texture : *Found)
				{
					if (!Exclude.Contains(Texture.Name) && !OutNames.Contains(Texture.Name))
					{
						OutNames.Add(Texture.Name);
						OutTextures.Add(Texture);
						Blocks += Texture.Blocks;
					}
				}
			}
		}
		OutTextures.Sort([](const FCookedTexture& A, const FCookedTexture& B) { return A.Name < B.Name; });
		return Blocks;
	};
	const auto ListTextures = [](const TArray<FCookedTexture>& Textures, FString& Out)
	{
		for (const FCookedTexture& Texture : Textures)
		{
			Out += FString::Printf("  %s %dx%d -> %dx%d %s, %d colours, %d level(s), %s\n", *Texture.Name,
				Texture.SourceSizeX, Texture.SourceSizeY, Texture.SizeX, Texture.SizeY,
				Texture.Format == PF_P4 ? TEXT("PSMT4") : TEXT("PSMT8"), Texture.SourceColors, Texture.NumMips,
				*BlocksToKB(Texture.Blocks));
		}
	};

	FString Report = FString::Printf(
		"VRAM report: the textures of each map against the GS texture arena (%s)\n", *BlocksToKB(ArenaBlocks));
	TArray<FCookedTexture> CommonTextures;
	TSet<FString> CommonNames;
	const uint32 CommonBlocks = GatherTextures(Common, TSet<FString>(), CommonTextures, CommonNames);
	Report += FString::Printf("\nCommon (the config's default assets, the directories always cooked): %d texture(s), "
							  "%s\n",
		CommonTextures.Num(), *BlocksToKB(CommonBlocks));
	ListTextures(CommonTextures, Report);
	for (const FString& Map : Maps)
	{
		TArray<FCookedTexture> Textures;
		TSet<FString> Names;
		const uint32 Blocks = GatherTextures({Map}, CommonNames, Textures, Names);
		const uint32 Total = Blocks + CommonBlocks;
		const bool bFits = Total <= ArenaBlocks;
		if (!bFits)
		{
			OutOverBudget.Add(Map);
		}
		Report += FString::Printf("\n%s: %d texture(s) of its own, %s; with the common ones %s of %s%s\n", *Map,
			Textures.Num(), *BlocksToKB(Blocks), *BlocksToKB(Total), *BlocksToKB(ArenaBlocks),
			bFits ? TEXT("") : TEXT(" (over: the cache uploads them again as it fills)"));
		ListTextures(Textures, Report);
	}
	return Report;
}

FString UCookCommandlet::MakeRamReport(const TArray<FString>& Maps, const TArray<FString>& Common,
	const TMap<FString, int64>& BytesByPackage, const FCookBudgets& Budgets, TArray<FString>& OutOverBudget)
{
	struct FPackageBytes
	{
		FString Name;
		int64 Bytes;
	};
	// The packages of Seeds' closure that are not in Exclude, largest first (then by name), and their bytes.
	const auto GatherPackages = [&BytesByPackage](const TArray<FString>& Seeds, const TSet<FString>& Exclude,
									TArray<FPackageBytes>& OutPackages, TSet<FString>& OutNames)
	{
		TArray<FString> Closure;
		if (Seeds.Num() > 0)
		{
			(void)CollectDependencies(Seeds, Closure);
		}
		int64 Total = 0;
		for (const FString& Package : Closure)
		{
			const int64* Found = BytesByPackage.Find(Package);
			if (Found != nullptr && !Exclude.Contains(Package) && !OutNames.Contains(Package))
			{
				OutNames.Add(Package);
				OutPackages.Add({Package, *Found});
				Total += *Found;
			}
		}
		OutPackages.Sort([](const FPackageBytes& A, const FPackageBytes& B)
			{ return A.Bytes != B.Bytes ? A.Bytes > B.Bytes : A.Name < B.Name; });
		return Total;
	};
	const auto ToKB = [](int64 Bytes)
	{ return FString::Printf("%lld KB", static_cast<long long>((Bytes + 1023) / 1024)); };
	const auto ListPackages = [&ToKB](const TArray<FPackageBytes>& Packages, FString& Out)
	{
		for (const FPackageBytes& Package : Packages)
		{
			Out += FString::Printf("  %s %s\n", *Package.Name, *ToKB(Package.Bytes));
		}
	};

	FString Report = TEXT("RAM report: the cooked packages each map loads (their serialized size; the heap they take "
						  "once loaded is GMalloc's, measured on the EE)\n");
	Report += FString::Printf("Estimate: %d KB + the cooked bytes x %d%%, against %s\n", Budgets.RuntimeBaseKB,
		Budgets.RuntimeExpansionPercent,
		Budgets.MapRamKB > 0 ? *FString::Printf("%d KB", Budgets.MapRamKB) : TEXT("no budget"));
	TArray<FPackageBytes> CommonPackages;
	TSet<FString> CommonNames;
	const int64 CommonBytes = GatherPackages(Common, TSet<FString>(), CommonPackages, CommonNames);
	Report += FString::Printf("\nCommon (the config's default assets, the directories always cooked): %d package(s), "
							  "%s\n",
		CommonPackages.Num(), *ToKB(CommonBytes));
	ListPackages(CommonPackages, Report);
	for (const FString& Map : Maps)
	{
		TArray<FPackageBytes> Packages;
		TSet<FString> Names;
		const int64 Bytes = GatherPackages({Map}, CommonNames, Packages, Names);
		const int64 Estimate = Budgets.EstimateRamBytes(Bytes + CommonBytes);
		const bool bFits = Budgets.MapRamKB <= 0 || Estimate <= int64(Budgets.MapRamKB) * 1024;
		if (!bFits)
		{
			OutOverBudget.Add(Map);
		}
		Report += FString::Printf("\n%s: %d package(s) of its own, %s; with the common ones %s; estimated at run time "
								  "%s%s\n",
			*Map, Packages.Num(), *ToKB(Bytes), *ToKB(Bytes + CommonBytes), *ToKB(Estimate),
			bFits ? TEXT("") : TEXT(" (over the budget)"));
		ListPackages(Packages, Report);
	}
	return Report;
}

FString UCookCommandlet::MakeSoundReport(const TArray<FString>& Maps, const TArray<FString>& Common,
	const TMap<FString, TArray<FCookedSound>>& SoundsByPackage, int32 BudgetBytes, TArray<FString>& OutOverBudget)
{
	// The sounds of Seeds' closure that are not in Exclude, sorted by name, and their bytes.
	const auto GatherSounds = [&SoundsByPackage](const TArray<FString>& Seeds, const TSet<FString>& Exclude,
								  TArray<FCookedSound>& OutSounds, TSet<FString>& OutNames)
	{
		TArray<FString> Closure;
		if (Seeds.Num() > 0)
		{
			(void)CollectDependencies(Seeds, Closure);
		}
		int64 Bytes = 0;
		for (const FString& Package : Closure)
		{
			if (const TArray<FCookedSound>* Found = SoundsByPackage.Find(Package))
			{
				for (const FCookedSound& Sound : *Found)
				{
					if (!Exclude.Contains(Sound.Name) && !OutNames.Contains(Sound.Name))
					{
						OutNames.Add(Sound.Name);
						OutSounds.Add(Sound);
						Bytes += Sound.Bytes;
					}
				}
			}
		}
		OutSounds.Sort([](const FCookedSound& A, const FCookedSound& B) { return A.Name < B.Name; });
		return Bytes;
	};
	const auto ListSounds = [](const TArray<FCookedSound>& Sounds, FString& Out)
	{
		for (const FCookedSound& Sound : Sounds)
		{
			Out += FString::Printf("  %s %d Hz -> %d Hz%s, %d frames, %s\n", *Sound.Name, Sound.SourceSampleRate,
				Sound.SampleRate, Sound.bLooping ? TEXT(" looping") : TEXT(""), Sound.NumFrames,
				*BytesToKB(Sound.Bytes));
		}
	};

	FString Report = FString::Printf(
		"SPU2 RAM report: the SPU2 ADPCM sounds of each map against the SPU2 RAM the audio device may fill (%s)\n",
		*BytesToKB(BudgetBytes));
	TArray<FCookedSound> CommonSounds;
	TSet<FString> CommonNames;
	const int64 CommonBytes = GatherSounds(Common, TSet<FString>(), CommonSounds, CommonNames);
	Report += FString::Printf("\nCommon (the config's default assets, the directories always cooked): %d sound(s), "
							  "%s\n",
		CommonSounds.Num(), *BytesToKB(CommonBytes));
	ListSounds(CommonSounds, Report);
	for (const FString& Map : Maps)
	{
		TArray<FCookedSound> Sounds;
		TSet<FString> Names;
		const int64 Bytes = GatherSounds({Map}, CommonNames, Sounds, Names);
		const int64 Total = Bytes + CommonBytes;
		const bool bFits = Total <= BudgetBytes;
		if (!bFits)
		{
			OutOverBudget.Add(Map);
		}
		Report += FString::Printf("\n%s: %d sound(s) of its own, %s; with the common ones %s of %s%s\n", *Map,
			Sounds.Num(), *BytesToKB(Bytes), *BytesToKB(Total), *BytesToKB(BudgetBytes),
			bFits ? TEXT("") : TEXT(" (over: the SPU2 cannot keep them all)"));
		ListSounds(Sounds, Report);
	}
	return Report;
}

int32 UCookCommandlet::StageNonPackageFiles(const ITargetPlatform& TargetPlatform, const FString& CookedDir)
{
	const FString IniPlatform = TargetPlatform.IniPlatformName();
	const FString EngineOut = CookedDir / TEXT("Engine");
	const FString ProjectOut = CookedDir / GetProjectFolderName();
	int32 Staged = 0;
	auto Add = [&Staged](int32 Count) { Staged = (Staged < 0 || Count < 0) ? -1 : Staged + Count; };

	// The config layers a game reads (D8), without the Editor files: Base*.ini, the platform's, the project's Default*.
	Add(StageConfigFolder(FPaths::EngineConfigDir(), TEXT("Base"), EngineOut / TEXT("Config")));
	Add(StageConfigFolder(
		FPaths::EngineConfigDir() / IniPlatform, *IniPlatform, EngineOut / TEXT("Config") / IniPlatform));
	Add(StageConfigFolder(FPaths::EnginePlatformExtensionsDir() / IniPlatform / TEXT("Config"), *IniPlatform,
		EngineOut / TEXT("Platforms") / IniPlatform / TEXT("Config")));
	if (FPaths::IsProjectFilePathSet())
	{
		Add(StageConfigFolder(FPaths::ProjectConfigDir(), TEXT("Default"), ProjectOut / TEXT("Config")));
		Add(StageConfigFolder(
			FPaths::ProjectConfigDir() / IniPlatform, *IniPlatform, ProjectOut / TEXT("Config") / IniPlatform));
		Add(StageConfigFolder(FPaths::ProjectPlatformExtensionsDir() / IniPlatform / TEXT("Config"), *IniPlatform,
			ProjectOut / TEXT("Platforms") / IniPlatform / TEXT("Config")));
		// The descriptor the game starts from.
		const FString ProjectFile = FPaths::GetProjectFilePath();
		Add(StageFile(ProjectFile, ProjectOut / FPaths::GetCleanFilename(ProjectFile)) ? 1 : -1);
	}

	// The shaders the desktop GS emulator compiles at run time (UE: the /Engine/Shaders virtual folder), for a platform
	// that compiles shaders (Win64); the PS2 draws without them.
	TArray<FName> ShaderFormats;
	TargetPlatform.GetAllTargetedShaderFormats(ShaderFormats);
	const FString ShaderDir = FPaths::EngineDir() / TEXT("Shaders");
	TArray<FString> Shaders;
	if (ShaderFormats.Num() > 0)
	{
		IFileManager::Get().FindFilesRecursive(Shaders, *ShaderDir, TEXT("*"), true, false);
	}
	SortNames(Shaders);
	for (const FString& Shader : Shaders)
	{
		FString Relative = Shader;
		FPaths::MakePathRelativeTo(Relative, *(ShaderDir + TEXT("/")));
		Add(StageFile(Shader, EngineOut / TEXT("Shaders") / Relative) ? 1 : -1);
	}
	return Staged;
}

int32 UCookCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

	ITargetPlatformManagerModule& PlatformManager = GetTargetPlatformManagerRef();
	TArray<ITargetPlatform*> TargetPlatforms;
	const FString* PlatformParam = ParamsMap.Find(TEXT("TargetPlatform"));
	if (PlatformParam == nullptr || PlatformParam->IsEmpty())
	{
		TargetPlatforms.Add(PlatformManager.GetRunningTargetPlatform());
	}
	else
	{
		TArray<FString> Names;
		PlatformParam->ParseIntoArray(Names, TEXT("+"), true);
		for (const FString& Name : Names)
		{
			ITargetPlatform* Platform = PlatformManager.FindTargetPlatform(Name);
			if (Platform == nullptr)
			{
				FString Known;
				for (const ITargetPlatform* Candidate : PlatformManager.GetTargetPlatforms())
				{
					Known += (Known.IsEmpty() ? TEXT("") : TEXT(", ")) + Candidate->PlatformName();
				}
				UE_LOG(LogCook, Error, "Cook: unknown -TargetPlatform=%s (the platforms: %s)", *Name, *Known);
				return 1;
			}
			TargetPlatforms.AddUnique(Platform);
		}
	}

	int32 Failures = 0;
	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		if (TargetPlatform->HasEditorOnlyData() || !TargetPlatform->IsLittleEndian())
		{
			UE_LOG(LogCook, Error, "Cook: %s needs editor-only data or big-endian packages, which Leon cannot cook",
				*TargetPlatform->PlatformName());
			return 1;
		}
		const FString Note = TargetPlatform->GetCookNote();
		if (!Note.IsEmpty())
		{
			UE_LOG(LogCook, Display, "Cook: %s", *Note);
		}

		// A fresh output: nothing left from an older cook. -iterate (the default) takes the packages whose key has not
		// changed from the cook cache; -full cooks them all.
		const FString CookedDir = GetCookedDir(TargetPlatform->PlatformName());
		IFileManager::Get().DeleteDirectory(*CookedDir, false, true);
		const bool bIterate = !Switches.ContainsByPredicate(
			[](const FString& Switch) { return Switch.Equals(TEXT("full"), ESearchCase::IgnoreCase); });
		const FString CacheDir = GetCookCacheDir(TargetPlatform->PlatformName());
		const FCookBudgets Budgets = FCookBudgets::Load(*TargetPlatform, ParamsMap);

		TArray<FString> Seeds;
		GatherCookSeeds(*TargetPlatform, ParamsMap, Seeds);
		if (Seeds.Num() == 0)
		{
			UE_LOG(LogCook, Error, "Cook: nothing to cook (no map, no DirectoriesToAlwaysCook, no default asset)");
			return 1;
		}
		TArray<FString> Packages;
		if (!CollectDependencies(Seeds, Packages))
		{
			++Failures;
		}
		UE_LOG(LogCook, Display, "Cook (%s): %d seed(s), %d package(s) with their dependencies, %s",
			*TargetPlatform->PlatformName(), Seeds.Num(), Packages.Num(),
			bIterate ? TEXT("iterative (the cook cache)") : TEXT("full (-full)"));

		int32 Cooked = 0;
		int32 FromCache = 0;
		FCookKeyCache KeyCache;
		TMap<FString, TArray<FCookedTexture>> TexturesByPackage;
		TMap<FString, TArray<FCookedSound>> SoundsByPackage;
		TMap<FString, int64> BytesByPackage;
		for (const FString& PackageName : Packages)
		{
			FCookedPackageInfo Info;
			bool bFromCache = false;
			const FString Key = GetCookKey(PackageName, *TargetPlatform, KeyCache);
			if (CookPackageCached(PackageName, *TargetPlatform, CookedDir, CacheDir, Key, bIterate, Info, bFromCache))
			{
				++Cooked;
				FromCache += bFromCache ? 1 : 0;
				FString SourceFile;
				if (FPackageName::DoesPackageExist(PackageName, nullptr, &SourceFile))
				{
					BytesByPackage.Add(PackageName,
						IFileManager::Get().FileSize(
							*GetCookedFilename(PackageName, CookedDir, FPaths::GetExtension(SourceFile, true))));
				}
				// The assets' hard budgets: over one fails the cook.
				TArray<FString> Errors;
				Budgets.CheckAssets(Info, Errors);
				for (const FString& Error : Errors)
				{
					UE_LOG(LogCook, Error, "Cook: over budget: %s", *Error);
					++Failures;
				}
				if (Info.Textures.Num() > 0)
				{
					TexturesByPackage.Add(PackageName, MoveTemp(Info.Textures));
				}
				if (Info.Sounds.Num() > 0)
				{
					SoundsByPackage.Add(PackageName, MoveTemp(Info.Sounds));
				}
			}
			else
			{
				++Failures;
			}
			if (!bFromCache)
			{
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			}
		}
		const int32 Staged = StageNonPackageFiles(*TargetPlatform, CookedDir);
		if (Staged < 0)
		{
			++Failures;
		}
		TArray<FString> Maps;
		TArray<FString> Common;
		for (const FString& Seed : Seeds)
		{
			FString Filename;
			const bool bMap = FPackageName::DoesPackageExist(Seed, nullptr, &Filename) &&
				FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension();
			(bMap ? Maps : Common).Add(Seed);
		}
		const auto SaveReport = [&Failures](const FString& Report, const FString& ReportFile)
		{
			if (!FFileHelper::SaveStringToFile(Report, *ReportFile))
			{
				UE_LOG(LogCook, Error, "Cook: cannot write '%s'", *ReportFile);
				++Failures;
			}
		};
		const FString ReportPrefix = FPaths::ProjectSavedDir() + TEXT("Cooked/") + TargetPlatform->PlatformName();
		{
			// The packages against the EE RAM (their runtime estimate): a map over its budget fails the cook.
			TArray<FString> OverBudget;
			const FString ReportFile = ReportPrefix + TEXT("-RamReport.txt");
			SaveReport(MakeRamReport(Maps, Common, BytesByPackage, Budgets, OverBudget), ReportFile);
			for (const FString& Map : OverBudget)
			{
				UE_LOG(LogCook, Error, "Cook: over budget: the packages of %s do not fit [%s] MapRamKB=%d (%s)", *Map,
					FCookBudgets::Section, Budgets.MapRamKB, *ReportFile);
				++Failures;
			}
			UE_LOG(LogCook, Display, "Cook (%s): RAM report of %d map(s) in %s", *TargetPlatform->PlatformName(),
				Maps.Num(), *ReportFile);
		}
		if (WantsSpuAdpcmSounds(*TargetPlatform))
		{
			// The sounds against the SPU2 RAM: a map that cannot keep its sounds resident fails the cook.
			TArray<FString> OverBudget;
			const FString ReportFile = ReportPrefix + TEXT("-SoundReport.txt");
			SaveReport(
				MakeSoundReport(Maps, Common, SoundsByPackage, Budgets.MapSoundRamKB * 1024, OverBudget), ReportFile);
			for (const FString& Map : OverBudget)
			{
				UE_LOG(LogCook, Error, "Cook: over budget: the sounds of %s do not fit [%s] MapSoundRamKB=%d (%s)",
					*Map, FCookBudgets::Section, Budgets.MapSoundRamKB, *ReportFile);
				++Failures;
			}
			UE_LOG(LogCook, Display, "Cook (%s): SPU2 RAM report of %d map(s) in %s", *TargetPlatform->PlatformName(),
				Maps.Num(), *ReportFile);
		}
		if (WantsPalettedTextures(*TargetPlatform))
		{
			// The textures against the GS texture VRAM: a map over its budget fails the cook.
			TArray<FString> OverBudget;
			const FString ReportFile = ReportPrefix + TEXT("-VramReport.txt");
			const uint32 BudgetBlocks = uint32(Budgets.MapVramKB) * 1024 / FGSTextureLayout::BytesPerBlock;
			SaveReport(MakeVramReport(Maps, Common, TexturesByPackage, BudgetBlocks, OverBudget), ReportFile);
			for (const FString& Map : OverBudget)
			{
				UE_LOG(LogCook, Error, "Cook: over budget: the textures of %s do not fit [%s] MapVramKB=%d (%s)", *Map,
					FCookBudgets::Section, Budgets.MapVramKB, *ReportFile);
				++Failures;
			}
			UE_LOG(LogCook, Display, "Cook (%s): VRAM report of %d map(s) in %s", *TargetPlatform->PlatformName(),
				Maps.Num(), *ReportFile);
		}
		UE_LOG(LogCook, Display, "Cook (%s): %d package(s) from the cook cache, %d cooked",
			*TargetPlatform->PlatformName(), FromCache, Cooked - FromCache);
		UE_LOG(LogCook, Display,
			"Cook (%s): %d of %d packages cooked, %d config / shader / project file(s) staged, in %s",
			*TargetPlatform->PlatformName(), Cooked, Packages.Num(), Staged < 0 ? 0 : Staged, *CookedDir);
	}
	return Failures == 0 ? 0 : 1;
}
