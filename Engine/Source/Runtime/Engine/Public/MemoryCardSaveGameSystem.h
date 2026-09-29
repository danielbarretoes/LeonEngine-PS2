#pragma once

// The PS2's memory card as a save game system (Leon; Docs/PLANS/ps2-shipping.md N24). The card's rules are here, on
// every platform, so the tests run them against a card in memory; the PS2 gives the real card (libmc) as the device.

#include "CoreMinimal.h"
#include "SaveGameSystem.h"

/** What a memory card slot holds. */
enum class EMemoryCardState : uint8
{
	NoCard,
	Unformatted,
	Ready,
};

/**
 * A memory card, as FMemoryCardSaveGameSystem uses it: its state and free room, and files in folders by their card
 * paths ("/BASLUS-99001SHOOTER/Settings"). The PS2's is libmc's card in slot 1 (mc0:); the tests' is in memory.
 */
class ENGINE_API IMemoryCardDevice
{
public:
	virtual ~IMemoryCardDevice() = default;

	/**
	 * The card's state and its free kilobytes (clusters); bOutChanged when the card in the slot is not the one of the
	 * last call (another card, or the first call).
	 */
	virtual EMemoryCardState GetState(int32& OutFreeKB, bool& bOutChanged) = 0;

	virtual bool DirectoryExists(const FString& Directory) = 0;
	virtual ESaveGameResult MakeDirectory(const FString& Directory) = 0;
	virtual bool FileExists(const FString& Path) = 0;
	/** Creates or replaces a file. */
	virtual ESaveGameResult WriteFile(const FString& Path, const TArray<uint8>& Data) = 0;
	virtual ESaveGameResult ReadFile(const FString& Path, TArray<uint8>& OutData) = 0;
	virtual ESaveGameResult DeleteFile(const FString& Path) = 0;
};

/**
 * Saves on a memory card (Leon's ISaveGameSystem for the PS2): a slot is a file in the game's folder on the card, which
 * the PS2's browser shows with the game's title and icon (icon.sys and the icon, written with the folder). A save
 * checks the card first and says why it cannot save (no card, unformatted, not enough room: the folder, icon.sys and
 * the icon the first time), and again after writing (a card pulled out or swapped meanwhile). Each file carries a
 * header with its size and CRC, so a damaged save loads as Corrupt, not as garbage.
 */
class ENGINE_API FMemoryCardSaveGameSystem : public ISaveGameSystem
{
public:
	/** The file header's tag: "LSAV". */
	static constexpr uint32 FileTag = 0x5641534C;
	/** The size of icon.sys (libmc's mcIcon). */
	static constexpr int32 IconSysSize = 964;
	/** The card's name of the icon file. */
	static const TCHAR* GetIconFilename()
	{
		return TEXT("icon.ico");
	}

	/**
	 * InDirectory: the game's folder on the card (the PS2 names it after the disc: "BASLUS-99001SHOOTER", 32 characters
	 * at most); InTitle: what the browser shows (ASCII, 32 characters at most).
	 */
	FMemoryCardSaveGameSystem(IMemoryCardDevice& InDevice, const FString& InDirectory, const FString& InTitle);

	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, int32 UserIndex) override;
	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex, const TArray<uint8>& Data) override;
	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex, TArray<uint8>& Data) override;
	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, int32 UserIndex) override;

	/** The card path of a slot's file. */
	[[nodiscard]] FString GetSavePath(const TCHAR* Name) const;
	[[nodiscard]] const FString& GetDirectory() const
	{
		return Directory;
	}

	/**
	 * icon.sys for Title (libmc's mcIcon, 964 bytes): "PS2D", the title in Shift-JIS (ASCII letters, digits and a few
	 * signs become their full-width forms), the background and the lights, and IconFilename for the list, copy and
	 * delete icons.
	 */
	[[nodiscard]] static TArray<uint8> MakeIconSys(const FString& Title, const FString& IconFilename);

	/**
	 * The browser's icon (the PS2's .ico: one shape of a textured square turned to the viewer, no animation, a 128 x
	 * 128 16-bit texture): Leon's crosshair on a dark tile.
	 */
	[[nodiscard]] static TArray<uint8> MakeIcon();

	/** The kilobytes (clusters) a file of Size bytes takes on the card, and the folder's own. */
	[[nodiscard]] static int32 GetFileKB(int32 Size);
	static constexpr int32 DirectoryKB = 2;

private:
	/** The card's state as a result (Succeeded when it is ready). */
	ESaveGameResult CheckCard(int32& OutFreeKB);

	IMemoryCardDevice& Device;
	FString Directory;
	FString Title;
};
