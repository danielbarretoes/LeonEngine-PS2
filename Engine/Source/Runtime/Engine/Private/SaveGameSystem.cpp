#include "SaveGameSystem.h"

#include "GameFramework/SaveGame.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Saves (Docs/PLANS/ps2-shipping.md N24): the generic system of the desktop and the platform features that choose one.

USaveGame::USaveGame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const TCHAR* LexToString(ESaveGameResult Result)
{
	switch (Result)
	{
		case ESaveGameResult::Succeeded:
			return TEXT("succeeded");
		case ESaveGameResult::NotFound:
			return TEXT("no such save");
		case ESaveGameResult::NoStorage:
			return TEXT("no memory card");
		case ESaveGameResult::Unformatted:
			return TEXT("the memory card is not formatted");
		case ESaveGameResult::NoSpace:
			return TEXT("not enough free space");
		case ESaveGameResult::Removed:
			return TEXT("the memory card was removed");
		case ESaveGameResult::Corrupt:
			return TEXT("the save is damaged");
		case ESaveGameResult::Failed:
			break;
	}
	return TEXT("failed");
}

// FGenericSaveGameSystem

FGenericSaveGameSystem::FGenericSaveGameSystem()
	: SaveDirectory(FPaths::ProjectSavedDir() + TEXT("SaveGames/"))
{
}

FString FGenericSaveGameSystem::GetSaveGamePath(const TCHAR* Name) const
{
	return FPaths::Combine(SaveDirectory, FString(Name) + TEXT(".sav"));
}

ISaveGameSystem::ESaveExistsResult FGenericSaveGameSystem::DoesSaveGameExistWithResult(
	const TCHAR* Name, int32 /*UserIndex*/)
{
	if (IFileManager::Get().FileExists(*GetSaveGamePath(Name)))
	{
		LastResult = ESaveGameResult::Succeeded;
		return ESaveExistsResult::OK;
	}
	LastResult = ESaveGameResult::NotFound;
	return ESaveExistsResult::DoesNotExist;
}

bool FGenericSaveGameSystem::SaveGame(
	bool /*bAttemptToUseUI*/, const TCHAR* Name, int32 /*UserIndex*/, const TArray<uint8>& Data)
{
	(void)IFileManager::Get().MakeDirectory(*SaveDirectory, true);
	const bool bSaved = FFileHelper::SaveArrayToFile(Data, *GetSaveGamePath(Name));
	LastResult = bSaved ? ESaveGameResult::Succeeded : ESaveGameResult::Failed;
	return bSaved;
}

bool FGenericSaveGameSystem::LoadGame(bool /*bAttemptToUseUI*/, const TCHAR* Name, int32 UserIndex, TArray<uint8>& Data)
{
	if (DoesSaveGameExistWithResult(Name, UserIndex) != ESaveExistsResult::OK)
	{
		return false;
	}
	const bool bLoaded = FFileHelper::LoadFileToArray(Data, *GetSaveGamePath(Name));
	LastResult = bLoaded ? ESaveGameResult::Succeeded : ESaveGameResult::Failed;
	return bLoaded;
}

bool FGenericSaveGameSystem::DeleteGame(bool /*bAttemptToUseUI*/, const TCHAR* Name, int32 UserIndex)
{
	if (DoesSaveGameExistWithResult(Name, UserIndex) != ESaveExistsResult::OK)
	{
		return false;
	}
	const bool bDeleted = IFileManager::Get().Delete(*GetSaveGamePath(Name));
	LastResult = bDeleted ? ESaveGameResult::Succeeded : ESaveGameResult::Failed;
	return bDeleted;
}

// IPlatformFeaturesModule

IPlatformFeaturesModule& IPlatformFeaturesModule::Get()
{
	static IPlatformFeaturesModule Module;
	return Module;
}

ISaveGameSystem* IPlatformFeaturesModule::GetSaveGameSystem()
{
	return SaveGameSystemOverride != nullptr ? SaveGameSystemOverride : &GetPlatformSaveGameSystem();
}
