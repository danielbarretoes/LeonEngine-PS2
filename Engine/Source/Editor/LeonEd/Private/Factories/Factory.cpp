#include "Factories/Factory.h"

#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "LeonEdLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

FString UFactory::CurrentFilename;

namespace
{

	/** The extension of a Formats entry ("png;PNG image" -> "png"). */
	FString FormatExtension(const FString& Format)
	{
		int32 Semicolon = INDEX_NONE;
		return Format.FindChar(';', Semicolon) ? Format.Left(Semicolon) : Format;
	}

} // namespace

UFactory::UFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = 0;
	bEditorImport = 0;
}

bool UFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);
	for (const FString& Format : Formats)
	{
		if (FormatExtension(Format) == Extension)
		{
			return true;
		}
	}
	return false;
}

bool UFactory::DoesSupportClass(UClass* Class)
{
	UClass* Supported = ResolveSupportedClass();
	return Class != nullptr && Supported != nullptr && Class->IsChildOf(Supported);
}

UClass* UFactory::ResolveSupportedClass()
{
	return SupportedClass.Get();
}

UObject* UFactory::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context)
{
	(void)InClass;
	(void)InParent;
	(void)InName;
	(void)Flags;
	(void)Context;
	return nullptr;
}

UObject* UFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)InParent;
	(void)InName;
	(void)Flags;
	(void)Context;
	(void)Type;
	(void)Buffer;
	(void)BufferEnd;
	bOutOperationCanceled = false;
	return nullptr;
}

UObject* UFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled)
{
	(void)Parms;
	bOutOperationCanceled = false;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
	{
		UE_LOG(LogLeonEd, Error, "%s: cannot read '%s'", *GetClass()->GetName(), *Filename);
		return nullptr;
	}
	CurrentFilename = Filename;
	const FString Extension = FPaths::GetExtension(Filename);
	const uint8* Buffer = Bytes.GetData();
	UObject* Result = FactoryCreateBinary(InClass, InParent, InName, Flags, nullptr, *Extension, Buffer,
		Bytes.GetData() + Bytes.Num(), bOutOperationCanceled);
	CurrentFilename.Empty();
	return Result;
}

UObject* UFactory::StaticImportObject(
	UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, UFactory* InFactory)
{
	UFactory* Factory = InFactory;
	if (Factory == nullptr)
	{
		UClass* FactoryClass = FindFactoryClassForFile(Filename, Class);
		if (FactoryClass == nullptr)
		{
			UE_LOG(LogLeonEd, Error, "No factory imports '%s'", *Filename);
			return nullptr;
		}
		Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	}
	UClass* ImportClass = Class != nullptr ? Class : Factory->ResolveSupportedClass();
	bool bCanceled = false;
	Factory->AdditionalImportedObjects.Reset();
	UObject* Result = Factory->FactoryCreateFile(ImportClass, InParent, Name, Flags, Filename, nullptr, bCanceled);
	if (Result == nullptr && !bCanceled)
	{
		UE_LOG(LogLeonEd, Error, "%s failed to import '%s'", *Factory->GetClass()->GetName(), *Filename);
	}
	return Result;
}

UClass* UFactory::FindFactoryClassForFile(const FString& Filename, UClass* PreferredClass)
{
	UClass* Best = nullptr;
	int32 BestPriority = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UFactory::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		UFactory* Default = Class->GetDefaultObject<UFactory>();
		if (Default == nullptr || !Default->bEditorImport || !Default->FactoryCanImport(Filename))
		{
			continue;
		}
		UClass* Made = Default->ResolveSupportedClass();
		if (PreferredClass != nullptr &&
			(Made == nullptr || !(Made->IsChildOf(PreferredClass) || PreferredClass->IsChildOf(Made))))
		{
			continue;
		}
		// Highest priority first; ties go to the name, so the choice never depends on registration order.
		if (Best == nullptr || Default->ImportPriority > BestPriority ||
			(Default->ImportPriority == BestPriority && Class->GetName() < Best->GetName()))
		{
			Best = Class;
			BestPriority = Default->ImportPriority;
		}
	}
	return Best;
}

bool UFactory::ApplyImportSettings(const TMap<FString, FString>& Settings, TArray<FString>* OutUnknown)
{
	bool bAllParsed = true;
	for (const TPair<FString, FString>& Setting : Settings)
	{
		FProperty* Property = GetClass()->FindPropertyByName(FName(*Setting.Key));
		if (Property == nullptr || Property->HasAnyPropertyFlags(CPF_Transient))
		{
			if (OutUnknown != nullptr)
			{
				OutUnknown->Add(Setting.Key);
			}
			UE_LOG(LogLeonEd, Warning, "%s has no import setting '%s'", *GetClass()->GetName(), *Setting.Key);
			continue;
		}
		// An object path names an asset that may not be loaded yet (ImportText only finds loaded objects).
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (Setting.Value.StartsWith(TEXT("/")) && ObjectProperty->PropertyClass != nullptr)
			{
				(void)StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *Setting.Value);
			}
		}
		void* Value = Property->ContainerPtrToValuePtr<void>(this);
		if (Property->ImportText(*Setting.Value, Value, PPF_None, this) == nullptr)
		{
			UE_LOG(LogLeonEd, Error, "%s: cannot set %s to '%s'", *GetClass()->GetName(), *Setting.Key, *Setting.Value);
			bAllParsed = false;
			continue;
		}
		AppliedImportSettings.Add(Setting.Key, Setting.Value);
	}
	return bAllParsed;
}

UObject* UFactory::CreateOrOverwriteAsset(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags) const
{
	if (UObject* Existing = StaticFindObjectFast(nullptr, InParent, InName))
	{
		if (Existing->GetClass() == InClass && !Existing->IsPendingKill())
		{
			Existing->SetFlags(InFlags);
			return Existing;
		}
		UE_LOG(LogLeonEd, Error, "%s: '%s' already exists as a %s, not a %s", *GetClass()->GetName(),
			*Existing->GetPathName(), *Existing->GetClass()->GetName(), *InClass->GetName());
		return nullptr;
	}
	return NewObject<UObject>(InParent, InClass, InName, InFlags);
}

UAssetImportData* UFactory::GetAssetImportData(UObject* Asset, bool bCreate)
{
	if (Asset == nullptr)
	{
		return nullptr;
	}
	const FObjectProperty* Property =
		CastField<FObjectProperty>(Asset->GetClass()->FindPropertyByName(FName(TEXT("AssetImportData"))));
	if (Property == nullptr || Property->PropertyClass == nullptr ||
		!Property->PropertyClass->IsChildOf(UAssetImportData::StaticClass()))
	{
		return nullptr;
	}
	UObject** Value = Property->ContainerPtrToValuePtr<UObject*>(Asset);
	if (*Value == nullptr && bCreate)
	{
		// A fixed name keeps saves deterministic (D13); transient with a transient asset.
		*Value = NewObject<UAssetImportData>(
			Asset, TEXT("AssetImportData"), Asset->HasAnyFlags(RF_Transient) ? RF_Transient : RF_NoFlags);
	}
	return Cast<UAssetImportData>(*Value);
}

UAssetImportData* UFactory::UpdateAssetImportData(UObject* Asset, const FString& SourceFile) const
{
	UAssetImportData* ImportData = GetAssetImportData(Asset, true);
	if (ImportData != nullptr)
	{
		ImportData->Update(SourceFile);
		ImportData->SetImportSettings(AppliedImportSettings);
	}
	return ImportData;
}

bool UFactory::FactoryCanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UAssetImportData* ImportData = GetAssetImportData(Obj);
	if (Obj == nullptr || ImportData == nullptr || !DoesSupportClass(Obj->GetClass()))
	{
		return false;
	}
	const FString Source = ImportData->GetFirstFilename();
	if (Source.IsEmpty() || !FactoryCanImport(Source))
	{
		return false;
	}
	ImportData->ExtractFilenames(OutFilenames);
	return true;
}

void UFactory::FactorySetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (Obj != nullptr && NewReimportPaths.Num() > 0)
	{
		UAssetImportData* ImportData = GetAssetImportData(Obj, true);
		if (ImportData != nullptr)
		{
			ImportData->Update(NewReimportPaths[0]);
		}
	}
}

EReimportResult::Type UFactory::FactoryReimport(UObject* Obj)
{
	LastReimportAdditionalObjects.Reset();
	UAssetImportData* ImportData = GetAssetImportData(Obj);
	const FString Source = ImportData != nullptr ? ImportData->GetFirstFilename() : FString();
	if (Source.IsEmpty() || !FPaths::FileExists(Source))
	{
		UE_LOG(LogLeonEd, Error, "Cannot reimport %s: its source '%s' is missing", *Obj->GetPathName(), *Source);
		return EReimportResult::Failed;
	}
	// A fresh factory: the class default object answers CanReimport, but its settings must stay the defaults.
	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), GetClass());
	if (!Factory->ApplyImportSettings(ImportData->ImportSettings))
	{
		return EReimportResult::Failed;
	}
	bool bCanceled = false;
	UObject* Result = Factory->FactoryCreateFile(
		Obj->GetClass(), Obj->GetOuter(), Obj->GetFName(), Obj->GetFlags() & RF_Load, Source, nullptr, bCanceled);
	if (bCanceled)
	{
		return EReimportResult::Cancelled;
	}
	if (Result != Obj)
	{
		UE_LOG(LogLeonEd, Error, "Reimporting %s from '%s' failed", *Obj->GetPathName(), *Source);
		return EReimportResult::Failed;
	}
	LastReimportAdditionalObjects = Factory->AdditionalImportedObjects;
	UE_LOG(LogLeonEd, Log, "Reimported %s from '%s'", *Obj->GetPathName(), *Source);
	return EReimportResult::Succeeded;
}

void UFactory::FactoryGetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	OutObjects.Append(LastReimportAdditionalObjects);
}
