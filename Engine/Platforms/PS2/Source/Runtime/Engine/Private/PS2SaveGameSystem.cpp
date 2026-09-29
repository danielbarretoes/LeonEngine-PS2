#include "HAL/PlatformMisc.h"
#include "MemoryCardSaveGameSystem.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

#include <fcntl.h>
#include <libmc.h>
#include <stdio.h>
#include <string.h>

// The PS2's saves go to the memory card in slot 1 (mc0:) through libmc over the ROM's MCMAN and MCSERV
// (Docs/PLANS/ps2-shipping.md N24). FMemoryCardSaveGameSystem has the rules (the game's folder, icon.sys and the icon,
// the room, the checks); this device only speaks to the card. Every call waits for the card (mcSync): a save takes a
// moment, so the game saves when an option changes, not every frame.

DEFINE_LOG_CATEGORY_STATIC(LogPS2MemoryCard, Log, All);

namespace
{
	constexpr int32 Port = 0;
	constexpr int32 Slot = 0;
	/** libmc moves the bytes by DMA from and to this buffer: 64-byte aligned. */
	constexpr int32 ChunkBytes = 2048;
	uint8 GTransferBuffer[ChunkBytes] __attribute__((aligned(64)));

	/** Holds the IOP lock (FPS2PlatformMisc::LockIop) for a card call: the IO thread's reads wait meanwhile. */
	struct FIopLockScope
	{
		FIopLockScope()
		{
			FPlatformMisc::LockIop();
		}
		~FIopLockScope()
		{
			FPlatformMisc::UnlockIop();
		}
	};

	/** A libmc call's result, waited for. */
	int32 Sync()
	{
		int32 Command = 0;
		int32 Result = 0;
		(void)mcSync(0, &Command, &Result);
		return Result;
	}

	/** libmc's error code as a save result. */
	ESaveGameResult ToResult(int32 Code)
	{
		switch (Code)
		{
			case sceMcResSucceed:
				return ESaveGameResult::Succeeded;
			case sceMcResChangedCard:
				return ESaveGameResult::Removed;
			case sceMcResNoFormat:
				return ESaveGameResult::Unformatted;
			case sceMcResFullDevice:
				return ESaveGameResult::NoSpace;
			case sceMcResNoEntry:
				return ESaveGameResult::NotFound;
			default:
				return Code >= 0 ? ESaveGameResult::Succeeded : ESaveGameResult::Failed;
		}
	}

	class FPS2MemoryCard final : public IMemoryCardDevice
	{
	public:
		/** The card's modules load with the engine (UEngine::Init), before the IO thread's first read. */
		FPS2MemoryCard()
		{
			(void)Initialize();
		}

		virtual EMemoryCardState GetState(int32& OutFreeKB, bool& bOutChanged) override
		{
			OutFreeKB = 0;
			bOutChanged = true;
			if (!Initialize())
			{
				return EMemoryCardState::NoCard;
			}
			FIopLockScope Lock;
			int32 Type = 0;
			int32 Free = 0;
			int32 Formatted = 0;
			(void)mcGetInfo(Port, Slot, &Type, &Free, &Formatted);
			const int32 Result = Sync();
			// 0: the same card as the last call; -1 / -2: a card (formatted or not) put in since; below: none, or an
			// error.
			bOutChanged = Result != 0;
			if (Result < -2 || Type != sceMcTypePS2)
			{
				return EMemoryCardState::NoCard;
			}
			if (Result == -2 || Formatted == 0)
			{
				return EMemoryCardState::Unformatted;
			}
			OutFreeKB = Free;
			return EMemoryCardState::Ready;
		}

		virtual bool DirectoryExists(const FString& Directory) override
		{
			FIopLockScope Lock;
			static sceMcTblGetDir Table __attribute__((aligned(64)));
			(void)mcGetDir(Port, Slot, *Directory, 0, 1, &Table);
			return Sync() > 0;
		}

		virtual ESaveGameResult MakeDirectory(const FString& Directory) override
		{
			FIopLockScope Lock;
			(void)mcMkDir(Port, Slot, *Directory);
			const int32 Result = Sync();
			// -4 on an existing folder: it is there.
			return Result == sceMcResNoEntry ? ESaveGameResult::Succeeded : ToResult(Result);
		}

		virtual bool FileExists(const FString& Path) override
		{
			FIopLockScope Lock;
			(void)mcOpen(Port, Slot, *Path, O_RDONLY);
			const int32 Handle = Sync();
			if (Handle < 0)
			{
				return false;
			}
			(void)mcClose(Handle);
			(void)Sync();
			return true;
		}

		virtual ESaveGameResult WriteFile(const FString& Path, const TArray<uint8>& Data) override
		{
			FIopLockScope Lock;
			(void)mcOpen(Port, Slot, *Path, O_WRONLY | O_CREAT | O_TRUNC);
			const int32 Handle = Sync();
			if (Handle < 0)
			{
				return ToResult(Handle);
			}
			ESaveGameResult Result = ESaveGameResult::Succeeded;
			for (int32 Offset = 0; Offset < Data.Num() && Result == ESaveGameResult::Succeeded; Offset += ChunkBytes)
			{
				const int32 Bytes = FMath::Min(ChunkBytes, Data.Num() - Offset);
				FMemory::Memcpy(GTransferBuffer, Data.GetData() + Offset, SIZE_T(Bytes));
				(void)mcWrite(Handle, GTransferBuffer, Bytes);
				const int32 Written = Sync();
				Result = Written == Bytes ? ESaveGameResult::Succeeded
					: Written < 0         ? ToResult(Written)
										  : ESaveGameResult::NoSpace;
			}
			(void)mcClose(Handle);
			const int32 Closed = Sync();
			return Result == ESaveGameResult::Succeeded ? ToResult(Closed) : Result;
		}

		virtual ESaveGameResult ReadFile(const FString& Path, TArray<uint8>& OutData) override
		{
			FIopLockScope Lock;
			(void)mcOpen(Port, Slot, *Path, O_RDONLY);
			const int32 Handle = Sync();
			if (Handle < 0)
			{
				return ToResult(Handle);
			}
			(void)mcSeek(Handle, 0, SEEK_END);
			const int32 Size = Sync();
			(void)mcSeek(Handle, 0, SEEK_SET);
			(void)Sync();
			ESaveGameResult Result = Size >= 0 ? ESaveGameResult::Succeeded : ToResult(Size);
			OutData.SetNumUninitialized(FMath::Max(Size, 0));
			for (int32 Offset = 0; Offset < OutData.Num() && Result == ESaveGameResult::Succeeded; Offset += ChunkBytes)
			{
				const int32 Bytes = FMath::Min(ChunkBytes, OutData.Num() - Offset);
				(void)mcRead(Handle, GTransferBuffer, Bytes);
				const int32 Read = Sync();
				Result = Read == Bytes ? ESaveGameResult::Succeeded
					: Read < 0         ? ToResult(Read)
									   : ESaveGameResult::Failed;
				FMemory::Memcpy(OutData.GetData() + Offset, GTransferBuffer, SIZE_T(Bytes));
			}
			(void)mcClose(Handle);
			(void)Sync();
			return Result;
		}

		virtual ESaveGameResult DeleteFile(const FString& Path) override
		{
			FIopLockScope Lock;
			(void)mcDelete(Port, Slot, *Path);
			return ToResult(Sync());
		}

	private:
		/** The ROM's card modules and libmc, once; false when they did not load (no card then). */
		bool Initialize()
		{
			if (!bTried)
			{
				bTried = true;
				bReady = FPlatformMisc::LoadIopModule("rom0:SIO2MAN") && FPlatformMisc::LoadIopModule("rom0:MCMAN") &&
					FPlatformMisc::LoadIopModule("rom0:MCSERV") && InitLibmc();
				UE_LOG(LogPS2MemoryCard, Log, "Memory card: libmc %s", bReady ? "ready" : "not available");
			}
			return bReady;
		}

		static bool InitLibmc()
		{
			FIopLockScope Lock;
			return mcInit(MC_TYPE_MC) >= 0;
		}

		bool bTried = false;
		bool bReady = false;
	};
} // namespace

ISaveGameSystem& GetPlatformSaveGameSystem()
{
	static FPS2MemoryCard Card;
	// The game's folder and title on the card (DefaultGame.ini's [MemoryCard]); by default the disc's serial and the
	// project's name.
	static FMemoryCardSaveGameSystem* SaveGameSystem = nullptr;
	if (SaveGameSystem == nullptr)
	{
		const FString ProjectName = FApp::HasProjectName() ? FString(FApp::GetProjectName()) : FString(TEXT("LEON"));
		FString Directory = FString(TEXT("BASLUS-99001")) + ProjectName.ToUpper();
		FString Title = ProjectName;
		if (GConfig != nullptr)
		{
			(void)GConfig->GetString(TEXT("MemoryCard"), TEXT("Directory"), Directory, GGameIni);
			(void)GConfig->GetString(TEXT("MemoryCard"), TEXT("Title"), Title, GGameIni);
		}
		SaveGameSystem = new FMemoryCardSaveGameSystem(Card, Directory, Title);
	}
	return *SaveGameSystem;
}
