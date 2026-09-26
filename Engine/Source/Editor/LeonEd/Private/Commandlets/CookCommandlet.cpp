#include "Commandlets/CookCommandlet.h"

#include "AssetImportUtils.h"
#include "Commandlets/ResavePackagesCommandlet.h"
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
#include "PalettedTexture.h"
#include "Templates/UniquePtr.h"
#include "UObject/GarbageCollection.h"
#include "UObject/LinkerLoad.h"
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
		if (!FPalettedTextureBuilder::Build(Rgba.GetData(), Data.SizeX, Data.SizeY, Paletted))
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
		const EGSPixelFormat GSFormat = Paletted.Format == PF_P4 ? EGSPixelFormat::PSMT4 : EGSPixelFormat::PSMT8;
		OutInfo.Blocks = FGSTextureLayout::GetNumBlocks(GSFormat, uint32(Paletted.SizeX), uint32(Paletted.SizeY)) +
			FGSTextureLayout::GetClutBlocks(GSFormat);
		return Texture.SetPlatformData(Paletted.SizeX, Paletted.SizeY, Paletted.Format, Paletted.Data.GetData());
	}

	/** KB of a block count (a block is 256 bytes). */
	[[nodiscard]] FString BlocksToKB(uint32 Blocks)
	{
		return FString::Printf("%u KB", (Blocks * FGSTextureLayout::BytesPerBlock) / 1024);
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
			if (!StageFile(SourceDir / Name, DestDir / Name))
			{
				return -1;
			}
			++Staged;
		}
		return Staged;
	}
} // namespace

UCookCommandlet::UCookCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription = TEXT("Cooks the maps and what they use, with the config and the shaders, into Saved/Cooked");
	HelpUsage = TEXT("-run=Cook -TargetPlatform=Win64|PS2 [-map=<Map>+<Map>] [-package=<LongPackageName>[,...]] "
					 "[-packagefolder=<LongPackagePath>]");
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
	const FString& CookedDir, TArray<FCookedTexture>* OutTextures)
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

	// The platform's texture format, for the save only: the loaded textures get their data back after it, since
	// another platform's cook or a later package may use them.
	TArray<FTextureBackup> Backups;
	if (WantsPalettedTextures(TargetPlatform))
	{
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects);
		Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
		for (UObject* Object : Objects)
		{
			UTexture2D* Texture = Cast<UTexture2D>(Object);
			FTextureBackup Backup;
			FCookedTexture Info;
			if (Texture != nullptr && MakePaletted(*Texture, Backup, Info))
			{
				UE_LOG(LogCook, Display, "Cook: %s %dx%d -> %dx%d %s (%d colours), %s", *Info.Name, Info.SourceSizeX,
					Info.SourceSizeY, Info.SizeX, Info.SizeY, Info.Format == PF_P4 ? TEXT("PSMT4") : TEXT("PSMT8"),
					Info.SourceColors, *BlocksToKB(Info.Blocks));
				Backups.Add(MoveTemp(Backup));
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
			Out += FString::Printf("  %s %dx%d -> %dx%d %s, %d colours, %s\n", *Texture.Name, Texture.SourceSizeX,
				Texture.SourceSizeY, Texture.SizeX, Texture.SizeY,
				Texture.Format == PF_P4 ? TEXT("PSMT4") : TEXT("PSMT8"), Texture.SourceColors,
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

	// The shaders the desktop GS emulator compiles at run time (UE: the /Engine/Shaders virtual folder).
	const FString ShaderDir = FPaths::EngineDir() / TEXT("Shaders");
	TArray<FString> Shaders;
	IFileManager::Get().FindFilesRecursive(Shaders, *ShaderDir, TEXT("*"), true, false);
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

		// A fresh output: nothing left from an older cook (Leon has no -iterate yet).
		const FString CookedDir = GetCookedDir(TargetPlatform->PlatformName());
		IFileManager::Get().DeleteDirectory(*CookedDir, false, true);

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
		UE_LOG(LogCook, Display, "Cook (%s): %d seed(s), %d package(s) with their dependencies",
			*TargetPlatform->PlatformName(), Seeds.Num(), Packages.Num());

		int32 Cooked = 0;
		TMap<FString, TArray<FCookedTexture>> TexturesByPackage;
		for (const FString& PackageName : Packages)
		{
			TArray<FCookedTexture> Textures;
			if (CookPackage(PackageName, *TargetPlatform, CookedDir, &Textures))
			{
				++Cooked;
				if (Textures.Num() > 0)
				{
					TexturesByPackage.Add(PackageName, MoveTemp(Textures));
				}
			}
			else
			{
				++Failures;
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		const int32 Staged = StageNonPackageFiles(*TargetPlatform, CookedDir);
		if (Staged < 0)
		{
			++Failures;
		}
		if (WantsPalettedTextures(*TargetPlatform))
		{
			TArray<FString> Maps;
			TArray<FString> Common;
			for (const FString& Seed : Seeds)
			{
				FString Filename;
				const bool bMap = FPackageName::DoesPackageExist(Seed, nullptr, &Filename) &&
					FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension();
				(bMap ? Maps : Common).Add(Seed);
			}
			TArray<FString> OverBudget;
			const FString Report =
				MakeVramReport(Maps, Common, TexturesByPackage, FGSDrawEnvironment::TextureArenaBlocks, OverBudget);
			const FString ReportFile =
				FPaths::ProjectSavedDir() + TEXT("Cooked/") + TargetPlatform->PlatformName() + TEXT("-VramReport.txt");
			if (!FFileHelper::SaveStringToFile(Report, *ReportFile))
			{
				UE_LOG(LogCook, Error, "Cook: cannot write '%s'", *ReportFile);
				++Failures;
			}
			for (const FString& Map : OverBudget)
			{
				UE_LOG(LogCook, Warning, "Cook: the textures of %s do not fit the GS texture arena (%s)", *Map,
					*ReportFile);
			}
			UE_LOG(LogCook, Display, "Cook (%s): VRAM report of %d map(s) in %s", *TargetPlatform->PlatformName(),
				Maps.Num(), *ReportFile);
		}
		UE_LOG(LogCook, Display,
			"Cook (%s): %d of %d packages cooked, %d config / shader / project file(s) staged, in %s",
			*TargetPlatform->PlatformName(), Cooked, Packages.Num(), Staged < 0 ? 0 : Staged, *CookedDir);
	}
	return Failures == 0 ? 0 : 1;
}
