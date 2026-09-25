#include "Commandlets/ImportAssetsCommandlet.h"

#include "AssetImportUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "Factories/FbxFactory.h"
#include "Factories/GLTFImportFactory.h"
#include "Factories/LegacyMaterialFactory.h"
#include "Factories/SoundFactory.h"
#include "Factories/TextureFactory.h"
#include "LeonEdLog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

namespace
{

	/** The switches ImportAssets reads itself; every other `-Key=Value` is an import setting. */
	const TCHAR* const CommandletSwitches[] = {TEXT("source"), TEXT("dest"), TEXT("name"), TEXT("type"),
		TEXT("importlist"), TEXT("reimport"), TEXT("all"), TEXT("package"), TEXT("run"), TEXT("project")};

	bool IsOneOf(const FString& Key, TArrayView<const TCHAR* const> Keys)
	{
		for (const TCHAR* Candidate : Keys)
		{
			if (Key == Candidate)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The factory class of an import: the one Type names (adding the FBX mesh type to Settings), else the best one for
	 * the file's extension. Null (logged) when none.
	 */
	UClass* ResolveFactoryClass(const FString& Type, const FString& SourceFile, TMap<FString, FString>& Settings)
	{
		if (Type.IsEmpty())
		{
			UClass* FactoryClass = UFactory::FindFactoryClassForFile(SourceFile);
			if (FactoryClass == nullptr)
			{
				UE_LOG(LogLeonEd, Error, "ImportAssets: no factory imports '%s'", *SourceFile);
			}
			return FactoryClass;
		}
		const FString Extension = FPaths::GetExtension(SourceFile);
		if (Type == TEXT("Texture"))
		{
			return UTextureFactory::StaticClass();
		}
		if (Type == TEXT("StaticMesh"))
		{
			return Extension == TEXT("gltf") || Extension == TEXT("glb") ? UGLTFImportFactory::StaticClass()
																		 : UFbxFactory::StaticClass();
		}
		if (Type == TEXT("SkeletalMesh"))
		{
			Settings.Add(TEXT("MeshTypeToImport"), TEXT("FBXIT_SkeletalMesh"));
			return UFbxFactory::StaticClass();
		}
		if (Type == TEXT("Animation"))
		{
			Settings.Add(TEXT("MeshTypeToImport"), TEXT("FBXIT_Animation"));
			return UFbxFactory::StaticClass();
		}
		if (Type == TEXT("Sound"))
		{
			return USoundFactory::StaticClass();
		}
		if (Type == TEXT("Material"))
		{
			return ULegacyMaterialFactory::StaticClass();
		}
		UE_LOG(LogLeonEd, Error,
			"ImportAssets: unknown type '%s' (Texture, StaticMesh, SkeletalMesh, Animation, Sound, Material)", *Type);
		return nullptr;
	}

	/** Saves the packages of Assets, each once. */
	bool SaveAssetPackages(const TArray<UObject*>& Assets)
	{
		TArray<UPackage*> Saved;
		bool bAllSaved = true;
		for (UObject* Asset : Assets)
		{
			UPackage* Package = Asset != nullptr ? Asset->GetOutermost() : nullptr;
			if (Package == nullptr || Saved.Contains(Package))
			{
				continue;
			}
			Saved.Add(Package);
			bAllSaved &= FAssetImportUtils::SavePackage(Package, Asset);
		}
		return bAllSaved;
	}

	/** The package named PackageName: loaded when its file exists (an import over it), else a new one. */
	UPackage* FindOrLoadPackage(const FString& PackageName)
	{
		if (FPackageName::DoesPackageExist(PackageName))
		{
			if (UPackage* Loaded = LoadPackage(nullptr, *PackageName, LOAD_None))
			{
				return Loaded;
			}
		}
		return CreatePackage(*PackageName);
	}

} // namespace

UImportAssetsCommandlet::UImportAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription = TEXT("Imports source files as assets, or reimports assets from their sources");
	HelpUsage = TEXT("-run=ImportAssets -source=<File> -dest=<LongPackagePath> [-name=<Asset>] [-type=<Type>] "
					 "[-<Setting>=<Value>...] | -importlist=<ImportList.ini> | -reimport -all | -reimport "
					 "-package=<LongPackageName>[,...]");
	LogToConsole = 1;
}

UObject* UImportAssetsCommandlet::ImportAsset(const FString& SourceFile, const FString& DestPath,
	const FString& AssetName, const FString& Type, const TMap<FString, FString>& Settings)
{
	const FString Source = FPaths::ConvertRelativePathToFull(SourceFile);
	if (!FPaths::FileExists(Source))
	{
		UE_LOG(LogLeonEd, Error, "ImportAssets: the source '%s' does not exist", *Source);
		return nullptr;
	}
	TMap<FString, FString> FactorySettings = Settings;
	UClass* FactoryClass = ResolveFactoryClass(Type, Source, FactorySettings);
	if (FactoryClass == nullptr)
	{
		return nullptr;
	}
	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	TArray<FString> Unknown;
	if (!Factory->ApplyImportSettings(FactorySettings, &Unknown) || Unknown.Num() > 0)
	{
		UE_LOG(LogLeonEd, Error, "ImportAssets: bad import settings for '%s'", *Source);
		return nullptr;
	}
	UClass* AssetClass = Factory->ResolveSupportedClass();
	FString Path = DestPath;
	Path.RemoveFromEnd(TEXT("/"));
	const FString Name =
		AssetName.IsEmpty() ? FAssetImportUtils::MakeAssetName(AssetClass, FPaths::GetBaseFilename(Source)) : AssetName;
	const FString PackageName = Path + TEXT("/") + Name;
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		UE_LOG(LogLeonEd, Error, "ImportAssets: '%s' is not a package under a mount point (-dest=%s)", *PackageName,
			*DestPath);
		return nullptr;
	}
	UPackage* Package = FindOrLoadPackage(PackageName);
	UObject* Asset =
		UFactory::StaticImportObject(AssetClass, Package, FName(*Name), RF_Public | RF_Standalone, Source, Factory);
	if (Asset == nullptr)
	{
		return nullptr;
	}
	TArray<UObject*> Assets;
	Assets.Add(Asset);
	Assets.Append(Factory->AdditionalImportedObjects);
	if (!SaveAssetPackages(Assets))
	{
		return nullptr;
	}
	UE_LOG(LogLeonEd, Display, "Imported '%s' as %s", *Source, *Asset->GetPathName());
	return Asset;
}

int32 UImportAssetsCommandlet::ImportList(const FString& ImportListFile)
{
	const FString ListFile = FPaths::ConvertRelativePathToFull(ImportListFile);
	if (!FPaths::FileExists(ListFile))
	{
		UE_LOG(LogLeonEd, Error, "ImportAssets: the import list '%s' does not exist", *ListFile);
		return 1;
	}
	FConfigFile List;
	List.Read(ListFile);
	const FString ListDir = FPaths::GetPath(ListFile);
	int32 Failures = 0;
	int32 Imported = 0;
	for (const TPair<FString, FConfigSection>& Section : List)
	{
		FString Source;
		FString Dest;
		FString Name;
		FString Type;
		TMap<FString, FString> Settings;
		for (const TPair<FName, FConfigValue>& Entry : Section.Value)
		{
			const FString Key = Entry.Key.ToString();
			const FString& Value = Entry.Value.GetValue();
			if (Key == TEXT("Source"))
			{
				Source = Value;
			}
			else if (Key == TEXT("Dest"))
			{
				Dest = Value;
			}
			else if (Key == TEXT("Name"))
			{
				Name = Value;
			}
			else if (Key == TEXT("Type"))
			{
				Type = Value;
			}
			else
			{
				Settings.Add(Key, Value);
			}
		}
		if (Source.IsEmpty() || Dest.IsEmpty())
		{
			UE_LOG(LogLeonEd, Error, "ImportAssets: [%s] of '%s' needs Source and Dest", *Section.Key, *ListFile);
			++Failures;
			continue;
		}
		const FString SourceFile = FPaths::IsRelative(Source) ? FPaths::Combine(ListDir, Source) : Source;
		if (ImportAsset(SourceFile, Dest, Name, Type, Settings) == nullptr)
		{
			++Failures;
			continue;
		}
		++Imported;
	}
	UE_LOG(LogLeonEd, Display, "ImportAssets: %d imported from '%s', %d failed", Imported, *ListFile, Failures);
	return Failures;
}

int32 UImportAssetsCommandlet::ReimportPackages(const TArray<FString>& PackageNames, int32* OutReimported)
{
	TArray<FString> Packages = PackageNames;
	if (Packages.Num() == 0)
	{
		TArray<FString> Roots;
		FAssetImportUtils::GetContentMountPoints(Roots);
		for (const FString& Root : Roots)
		{
			FAssetImportUtils::FindPackages(Root, Packages);
		}
	}
	int32 Failures = 0;
	int32 Reimported = 0;
	int32 Skipped = 0;
	for (const FString& PackageName : Packages)
	{
		UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		if (Package == nullptr)
		{
			++Failures;
			continue;
		}
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, false);
		for (UObject* Object : Objects)
		{
			const UAssetImportData* ImportData = UFactory::GetAssetImportData(Object);
			const FString Source = ImportData != nullptr ? ImportData->GetFirstFilename() : FString();
			if (Source.IsEmpty())
			{
				continue;
			}
			if (!FPaths::FileExists(Source))
			{
				UE_LOG(LogLeonEd, Display, "ImportAssets: skipping %s, its source '%s' is missing",
					*Object->GetPathName(), *Source);
				++Skipped;
				continue;
			}
			TArray<UObject*> Assets;
			Assets.Add(Object);
			if (!FReimportManager::Instance()->Reimport(Object, &Assets) || !SaveAssetPackages(Assets))
			{
				++Failures;
				continue;
			}
			++Reimported;
		}
		// The next packages load into a clean object system; nothing here is referenced any more.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	UE_LOG(LogLeonEd, Display, "ImportAssets: %d reimported, %d skipped (no source), %d failed", Reimported, Skipped,
		Failures);
	if (OutReimported != nullptr)
	{
		*OutReimported = Reimported;
	}
	return Failures;
}

int32 UImportAssetsCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

	if (Switches.Contains(TEXT("reimport")))
	{
		TArray<FString> PackageNames;
		if (const FString* Packages = ParamsMap.Find(TEXT("package")))
		{
			Packages->ParseIntoArray(PackageNames, TEXT(","), true);
		}
		else if (!Switches.Contains(TEXT("all")))
		{
			UE_LOG(LogLeonEd, Error, "ImportAssets: -reimport needs -all or -package=<LongPackageName>");
			return 1;
		}
		return ReimportPackages(PackageNames) == 0 ? 0 : 1;
	}
	if (const FString* ListFile = ParamsMap.Find(TEXT("importlist")))
	{
		return ImportList(*ListFile) == 0 ? 0 : 1;
	}
	const FString* Source = ParamsMap.Find(TEXT("source"));
	const FString* Dest = ParamsMap.Find(TEXT("dest"));
	if (Source == nullptr || Dest == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "ImportAssets: usage: %s", *HelpUsage);
		return 1;
	}
	TMap<FString, FString> Settings;
	for (const TPair<FString, FString>& Param : ParamsMap)
	{
		if (!IsOneOf(Param.Key, CommandletSwitches))
		{
			Settings.Add(Param.Key, Param.Value);
		}
	}
	const FString* Name = ParamsMap.Find(TEXT("name"));
	const FString* Type = ParamsMap.Find(TEXT("type"));
	return ImportAsset(*Source, *Dest, Name != nullptr ? *Name : FString(), Type != nullptr ? *Type : FString(),
			   Settings) != nullptr
		? 0
		: 1;
}
