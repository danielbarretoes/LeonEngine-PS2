#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectVersion.h"
#include "UObject/Package.h"

// UGameplayStatics' saves (UE: GameplayStatics.cpp's SaveGame functions; Docs/PLANS/ps2-shipping.md N24).

DEFINE_LOG_CATEGORY_STATIC(LogSaveGame, Log, All);

namespace
{
	/** The save's header (UE: FSaveGameHeader): "GVAS", the save format's version, the package version, the class. */
	constexpr uint32 SaveGameFileTypeTag = 0x53415647;
	/** Leon's first save format. */
	constexpr int32 SaveGameFileVersion = 1;

	ESaveGameResult GLastSaveGameResult = ESaveGameResult::Succeeded;

	/** The platform's save game system (none: every call fails). */
	ISaveGameSystem* GetSaveSystem()
	{
		return IPlatformFeaturesModule::Get().GetSaveGameSystem();
	}
} // namespace

USaveGame* UGameplayStatics::CreateSaveGameObject(TSubclassOf<USaveGame> SaveGameClass)
{
	if (SaveGameClass == nullptr || SaveGameClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	return NewObject<USaveGame>(GetTransientPackage(), SaveGameClass.Get());
}

bool UGameplayStatics::SaveGameToMemory(USaveGame* SaveGameObject, TArray<uint8>& OutSaveData)
{
	OutSaveData.Reset();
	if (SaveGameObject == nullptr)
	{
		return false;
	}
	FMemoryWriter Writer(OutSaveData, true);
	uint32 Tag = SaveGameFileTypeTag;
	int32 SaveVersion = SaveGameFileVersion;
	int32 PackageVersion = VER_LEON_LATEST;
	FString ClassName = SaveGameObject->GetClass()->GetPathName();
	Writer << Tag << SaveVersion << PackageVersion << ClassName;
	Writer.SetUEVer(PackageVersion);
	SaveGameObject->Serialize(Writer);
	return !Writer.IsError();
}

USaveGame* UGameplayStatics::LoadGameFromMemory(const TArray<uint8>& InSaveData)
{
	if (InSaveData.Num() == 0)
	{
		return nullptr;
	}
	FMemoryReader Reader(InSaveData, true);
	uint32 Tag = 0;
	int32 SaveVersion = 0;
	int32 PackageVersion = 0;
	Reader << Tag << SaveVersion << PackageVersion;
	if (Reader.IsError() || Tag != SaveGameFileTypeTag || SaveVersion < 1 || SaveVersion > SaveGameFileVersion ||
		PackageVersion > VER_LEON_LATEST)
	{
		UE_LOG(LogSaveGame, Warning, "LoadGameFromMemory: the bytes are not a save of this engine");
		return nullptr;
	}
	FString ClassName;
	Reader << ClassName;
	UClass* SaveGameClass = Reader.IsError()
		? nullptr
		: StaticLoadClass(USaveGame::StaticClass(), nullptr, *ClassName, nullptr, LOAD_Quiet);
	if (SaveGameClass == nullptr)
	{
		UE_LOG(LogSaveGame, Warning, "LoadGameFromMemory: the save's class %s is gone", *ClassName);
		return nullptr;
	}
	USaveGame* SaveGame = CreateSaveGameObject(SaveGameClass);
	if (SaveGame == nullptr)
	{
		return nullptr;
	}
	Reader.SetUEVer(PackageVersion);
	SaveGame->Serialize(Reader);
	if (Reader.IsError())
	{
		UE_LOG(LogSaveGame, Warning, "LoadGameFromMemory: the save of %s is cut short", *ClassName);
		return nullptr;
	}
	return SaveGame;
}

bool UGameplayStatics::SaveGameToSlot(USaveGame* SaveGameObject, const FString& SlotName, const int32 UserIndex)
{
	ISaveGameSystem* SaveSystem = GetSaveSystem();
	TArray<uint8> Data;
	if (SaveSystem == nullptr || SlotName.IsEmpty() || !SaveGameToMemory(SaveGameObject, Data))
	{
		GLastSaveGameResult = ESaveGameResult::Failed;
		return false;
	}
	const bool bSaved = SaveSystem->SaveGame(false, *SlotName, UserIndex, Data);
	GLastSaveGameResult = SaveSystem->GetLastResult();
	UE_LOG(LogSaveGame, Log, "SaveGameToSlot %s: %s", *SlotName, LexToString(GLastSaveGameResult));
	return bSaved;
}

USaveGame* UGameplayStatics::LoadGameFromSlot(const FString& SlotName, const int32 UserIndex)
{
	ISaveGameSystem* SaveSystem = GetSaveSystem();
	TArray<uint8> Data;
	if (SaveSystem == nullptr || SlotName.IsEmpty())
	{
		GLastSaveGameResult = ESaveGameResult::Failed;
		return nullptr;
	}
	if (!SaveSystem->LoadGame(false, *SlotName, UserIndex, Data))
	{
		GLastSaveGameResult = SaveSystem->GetLastResult();
		return nullptr;
	}
	USaveGame* SaveGame = LoadGameFromMemory(Data);
	GLastSaveGameResult = SaveGame != nullptr ? ESaveGameResult::Succeeded : ESaveGameResult::Corrupt;
	return SaveGame;
}

bool UGameplayStatics::DoesSaveGameExist(const FString& SlotName, const int32 UserIndex)
{
	ISaveGameSystem* SaveSystem = GetSaveSystem();
	const bool bExists =
		SaveSystem != nullptr && !SlotName.IsEmpty() && SaveSystem->DoesSaveGameExist(*SlotName, UserIndex);
	GLastSaveGameResult = SaveSystem != nullptr ? SaveSystem->GetLastResult() : ESaveGameResult::Failed;
	return bExists;
}

bool UGameplayStatics::DeleteGameInSlot(const FString& SlotName, const int32 UserIndex)
{
	ISaveGameSystem* SaveSystem = GetSaveSystem();
	const bool bDeleted =
		SaveSystem != nullptr && !SlotName.IsEmpty() && SaveSystem->DeleteGame(false, *SlotName, UserIndex);
	GLastSaveGameResult = SaveSystem != nullptr ? SaveSystem->GetLastResult() : ESaveGameResult::Failed;
	return bDeleted;
}

ESaveGameResult UGameplayStatics::GetLastSaveGameResult()
{
	return GLastSaveGameResult;
}
