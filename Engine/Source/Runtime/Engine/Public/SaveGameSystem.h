#pragma once

// Where saves live (UE: Runtime/Engine/Public/SaveGameSystem.h; Docs/PLANS/ps2-shipping.md N24).

#include "CoreMinimal.h"

/**
 * Why a save, a load or a delete ended as it did (Leon: UE's ISaveGameSystem only returns false; the PS2's memory card
 * has more to say, and the game tells the player).
 */
enum class ESaveGameResult : uint8
{
	Succeeded,
	/** No save in that slot. */
	NotFound,
	/** Nothing to save to: no memory card in the slot. */
	NoStorage,
	/** The memory card is not formatted. */
	Unformatted,
	/** The memory card has no room for the save. */
	NoSpace,
	/** The memory card was pulled out, or another put in, during the operation. */
	Removed,
	/** The save's bytes are damaged (their checksum, or not a save at all). */
	Corrupt,
	/** Anything else. */
	Failed,
};

/** The text of a result, for the log and the game's messages. */
ENGINE_API const TCHAR* LexToString(ESaveGameResult Result);

/**
 * A platform's storage of saves (UE: ISaveGameSystem): a save is bytes under a name (a slot) and a user. The game goes
 * through UGameplayStatics (SaveGameToSlot, LoadGameFromSlot...), which asks IPlatformFeaturesModule for the
 * platform's.
 */
class ENGINE_API ISaveGameSystem
{
public:
	/** UE's answer to DoesSaveGameExistWithResult. */
	enum class ESaveExistsResult : uint8
	{
		OK,
		DoesNotExist,
		Corrupt,
		UnspecifiedError,
	};

	virtual ~ISaveGameSystem() = default;

	/** Whether the platform shows its own messages about saves (UE); Leon's never do: the game does. */
	virtual bool PlatformHasNativeUI()
	{
		return false;
	}

	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, int32 UserIndex) = 0;

	virtual bool DoesSaveGameExist(const TCHAR* Name, int32 UserIndex)
	{
		return DoesSaveGameExistWithResult(Name, UserIndex) == ESaveExistsResult::OK;
	}

	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex, const TArray<uint8>& Data) = 0;
	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex, TArray<uint8>& Data) = 0;
	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex) = 0;

	/** How the last call ended (Leon). */
	[[nodiscard]] ESaveGameResult GetLastResult() const
	{
		return LastResult;
	}

protected:
	ESaveGameResult LastResult = ESaveGameResult::Succeeded;
};

/**
 * The desktop's saves (UE: FGenericSaveGameSystem): <Project>/Saved/SaveGames/<Name>.sav, one file a slot (UserIndex
 * does not change the file, as UE). SetSaveDirectory moves them (the tests).
 */
class ENGINE_API FGenericSaveGameSystem : public ISaveGameSystem
{
public:
	FGenericSaveGameSystem();

	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, int32 UserIndex) override;
	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex, const TArray<uint8>& Data) override;
	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex, TArray<uint8>& Data) override;
	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex) override;

	/** The folder of the saves (default: FPaths::ProjectSavedDir() + "SaveGames/"). */
	void SetSaveDirectory(const FString& InDirectory)
	{
		SaveDirectory = InDirectory;
	}

	/** The file of a slot. */
	[[nodiscard]] FString GetSaveGamePath(const TCHAR* Name) const;

private:
	FString SaveDirectory;
};

/**
 * The platform's features (UE: IPlatformFeaturesModule); Leon has one: its save game system, the generic one on the
 * desktop, the memory card's on the PS2 (FMemoryCardSaveGameSystem).
 */
class ENGINE_API IPlatformFeaturesModule
{
public:
	static IPlatformFeaturesModule& Get();

	/** The platform's save game system, or the override's (UE). */
	ISaveGameSystem* GetSaveGameSystem();

	/** Makes GetSaveGameSystem return InSaveGameSystem (Leon: the tests); null returns the platform's again. */
	void SetSaveGameSystemOverride(ISaveGameSystem* InSaveGameSystem)
	{
		SaveGameSystemOverride = InSaveGameSystem;
	}

private:
	ISaveGameSystem* SaveGameSystemOverride = nullptr;
};

/** The platform's own save game system (defined by each platform: the desktop's files, the PS2's memory card). */
ISaveGameSystem& GetPlatformSaveGameSystem();
