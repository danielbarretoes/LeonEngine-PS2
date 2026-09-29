#include "CoreMinimal.h"
#include "EngineTestTypes.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MemoryCardSaveGameSystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SaveGameSystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// Saves (Docs/PLANS/ps2-shipping.md N24): UGameplayStatics' round trip through the desktop's files, and the memory
// card's rules against a card in memory.

namespace
{
	/** Points UGameplayStatics at a save game system for a scope. */
	class FScopedSaveGameSystem
	{
	public:
		explicit FScopedSaveGameSystem(ISaveGameSystem& System)
		{
			IPlatformFeaturesModule::Get().SetSaveGameSystemOverride(&System);
		}
		~FScopedSaveGameSystem()
		{
			IPlatformFeaturesModule::Get().SetSaveGameSystemOverride(nullptr);
		}
	};

	/** A memory card in memory: its files, its room, and its troubles on demand. */
	class FTestMemoryCard final : public IMemoryCardDevice
	{
	public:
		EMemoryCardState State = EMemoryCardState::Ready;
		int32 FreeKB = 8000;
		/** Pulled out while the next file is written. */
		bool bPullOutOnWrite = false;
		bool bChangedOnce = false;
		TMap<FString, TArray<uint8>> Files;
		TSet<FString> Directories;

		virtual EMemoryCardState GetState(int32& OutFreeKB, bool& bOutChanged) override
		{
			OutFreeKB = FreeKB;
			bOutChanged = bChangedOnce;
			bChangedOnce = false;
			return State;
		}
		virtual bool DirectoryExists(const FString& Directory) override
		{
			return Directories.Contains(Directory);
		}
		virtual ESaveGameResult MakeDirectory(const FString& Directory) override
		{
			Directories.Add(Directory);
			FreeKB -= FMemoryCardSaveGameSystem::DirectoryKB;
			return ESaveGameResult::Succeeded;
		}
		virtual bool FileExists(const FString& Path) override
		{
			return Files.Contains(Path);
		}
		virtual ESaveGameResult WriteFile(const FString& Path, const TArray<uint8>& Data) override
		{
			if (bPullOutOnWrite)
			{
				State = EMemoryCardState::NoCard;
				return ESaveGameResult::Failed;
			}
			if (const TArray<uint8>* Old = Files.Find(Path))
			{
				FreeKB += FMemoryCardSaveGameSystem::GetFileKB(Old->Num());
			}
			FreeKB -= FMemoryCardSaveGameSystem::GetFileKB(Data.Num());
			Files.Add(Path, Data);
			return ESaveGameResult::Succeeded;
		}
		virtual ESaveGameResult ReadFile(const FString& Path, TArray<uint8>& OutData) override
		{
			const TArray<uint8>* Found = Files.Find(Path);
			if (Found == nullptr)
			{
				return ESaveGameResult::NotFound;
			}
			OutData = *Found;
			return ESaveGameResult::Succeeded;
		}
		virtual ESaveGameResult DeleteFile(const FString& Path) override
		{
			return Files.Remove(Path) > 0 ? ESaveGameResult::Succeeded : ESaveGameResult::NotFound;
		}
	};

	UEngineTestSaveGame* MakeOptions()
	{
		UEngineTestSaveGame* Save =
			Cast<UEngineTestSaveGame>(UGameplayStatics::CreateSaveGameObject(UEngineTestSaveGame::StaticClass()));
		if (Save != nullptr)
		{
			Save->Sensitivity = 2.5f;
			Save->bInvertY = true;
			Save->Volume = 40;
			Save->PlayerName = TEXT("Leon");
			Save->Color = FLinearColor(1.0f, 0.5f, 0.25f);
			Save->Tags = {FName(TEXT("A")), FName(TEXT("B"))};
		}
		return Save;
	}

	void CheckOptions(FAutomationTestBase& Test, const USaveGame* Loaded)
	{
		const UEngineTestSaveGame* Save = Cast<UEngineTestSaveGame>(Loaded);
		if (!Test.TestNotNull(TEXT("The save loads as its class"), Save))
		{
			return;
		}
		Test.TestEqual(TEXT("A float"), Save->Sensitivity, 2.5f);
		Test.TestTrue(TEXT("A bool"), Save->bInvertY);
		Test.TestEqual(TEXT("An int"), Save->Volume, 40);
		Test.TestEqual(TEXT("A string"), Save->PlayerName, FString(TEXT("Leon")));
		Test.TestTrue(TEXT("A struct"), Save->Color.Equals(FLinearColor(1.0f, 0.5f, 0.25f)));
		Test.TestTrue(TEXT("An array of names"), Save->Tags.Num() == 2 && Save->Tags[1] == FName(TEXT("B")));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameRoundTripTest, "System.Engine.SaveGame.RoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSaveGameRoundTripTest::RunTest(const FString& Parameters)
{
	// A USaveGame's properties through a slot of the desktop's save game system (a folder of the tests' instead of
	// Saved/SaveGames/), and what goes wrong: no save, damaged bytes, a class that is not a save.
	FGenericSaveGameSystem Files;
	TestTrue("The desktop saves under Saved/SaveGames/",
		Files.GetSaveGamePath(TEXT("Options")).EndsWith(TEXT("Saved/SaveGames/Options.sav")));
	const FString Dir = FPaths::ProjectIntermediateDir() + TEXT("Tests/SaveGames/");
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Files.SetSaveDirectory(Dir);
	FScopedSaveGameSystem Scope(Files);

	TStrongObjectPtr<UEngineTestSaveGame> Options(MakeOptions());
	TestNull("An abstract class makes no save", UGameplayStatics::CreateSaveGameObject(USaveGame::StaticClass()));
	TestFalse("No save yet", UGameplayStatics::DoesSaveGameExist(TEXT("Options"), 0));
	TestNull("Loading nothing", UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("... says so", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::NotFound);
	TestTrue("Saved", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestTrue("A file", IFileManager::Get().FileExists(*(Dir + TEXT("Options.sav"))));
	TestTrue("It exists", UGameplayStatics::DoesSaveGameExist(TEXT("Options"), 0));
	CheckOptions(*this, UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("Loaded", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::Succeeded);

	// Only what differs from the class's defaults is written: a default save loads as the defaults.
	TStrongObjectPtr<USaveGame> Defaults(UGameplayStatics::CreateSaveGameObject(UEngineTestSaveGame::StaticClass()));
	TArray<uint8> DefaultBytes;
	TArray<uint8> OptionBytes;
	TestTrue("To memory",
		UGameplayStatics::SaveGameToMemory(Defaults.Get(), DefaultBytes) &&
			UGameplayStatics::SaveGameToMemory(Options.Get(), OptionBytes));
	TestTrue("Defaults are smaller", DefaultBytes.Num() < OptionBytes.Num());
	const UEngineTestSaveGame* LoadedDefaults =
		Cast<UEngineTestSaveGame>(UGameplayStatics::LoadGameFromMemory(DefaultBytes));
	TestTrue("The defaults", LoadedDefaults != nullptr && LoadedDefaults->Volume == 100 && !LoadedDefaults->bInvertY);

	// Damaged: not a save, cut short.
	TArray<uint8> Garbage = {1, 2, 3, 4, 5, 6, 7, 8};
	TestNull("Not a save", UGameplayStatics::LoadGameFromMemory(Garbage));
	TArray<uint8> Cut = OptionBytes;
	Cut.SetNum(20);
	TestNull("Cut short", UGameplayStatics::LoadGameFromMemory(Cut));
	TestTrue("A damaged file", FFileHelper::SaveArrayToFile(Garbage, *(Dir + TEXT("Damaged.sav"))));
	TestNull("... does not load", UGameplayStatics::LoadGameFromSlot(TEXT("Damaged"), 0));
	TestTrue("... as damaged", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::Corrupt);

	TestTrue("Deleted", UGameplayStatics::DeleteGameInSlot(TEXT("Options"), 0));
	TestFalse("Gone", UGameplayStatics::DoesSaveGameExist(TEXT("Options"), 0));
	TestFalse("No save to save", UGameplayStatics::SaveGameToSlot(nullptr, TEXT("Options"), 0));
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameMemoryCardTest, "System.Engine.SaveGame.MemoryCard",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSaveGameMemoryCardTest::RunTest(const FString& Parameters)
{
	// The PS2's memory card rules against a card in memory: the game's folder with icon.sys and the icon the first
	// time, and a reason for each failure (no card, unformatted, full, pulled out, damaged).
	FTestMemoryCard Card;
	FMemoryCardSaveGameSystem System(Card, TEXT("BASLUS-99001TEST"), TEXT("Leon Test Settings"));
	FScopedSaveGameSystem Scope(System);
	TStrongObjectPtr<UEngineTestSaveGame> Options(MakeOptions());

	Card.State = EMemoryCardState::NoCard;
	TestFalse("No card", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestTrue("... says so", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::NoStorage);
	TestNull("Nothing to load", UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("... no card", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::NoStorage);
	Card.State = EMemoryCardState::Unformatted;
	TestFalse("Unformatted", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestTrue("... says so", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::Unformatted);

	Card.State = EMemoryCardState::Ready;
	Card.FreeKB = 20;
	TestFalse(
		"No room for the folder and its icon", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestTrue("... full", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::NoSpace);
	TestEqual("Nothing written", Card.Files.Num(), 0);

	Card.FreeKB = 8000;
	TestTrue("Saved", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestTrue("The folder", Card.Directories.Contains(TEXT("/BASLUS-99001TEST")));
	const TArray<uint8>* IconSys = Card.Files.Find(TEXT("/BASLUS-99001TEST/icon.sys"));
	TestTrue("icon.sys",
		IconSys != nullptr && IconSys->Num() == FMemoryCardSaveGameSystem::IconSysSize &&
			FMemory::Memcmp(IconSys->GetData(), "PS2D", 4) == 0);
	TestTrue("The icon", Card.Files.Contains(TEXT("/BASLUS-99001TEST/icon.ico")));
	TestTrue("The save", Card.Files.Contains(TEXT("/BASLUS-99001TEST/Options")));
	CheckOptions(*this, UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("It exists", UGameplayStatics::DoesSaveGameExist(TEXT("Options"), 0));

	// A second save needs no room for the icons, and replaces the first.
	const int32 FreeBefore = Card.FreeKB;
	Options->Volume = 70;
	TestTrue("Saved again", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestEqual("In place", Card.FreeKB, FreeBefore);
	const UEngineTestSaveGame* Again =
		Cast<UEngineTestSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("The new value", Again != nullptr && Again->Volume == 70);

	// Pulled out while writing; another card put in since the save began.
	Card.bPullOutOnWrite = true;
	TestFalse("Pulled out", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));
	TestTrue("... removed", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::Removed);
	Card.bPullOutOnWrite = false;
	Card.State = EMemoryCardState::Ready;
	Card.bChangedOnce = false;
	TestTrue("Back in: it saves", UGameplayStatics::SaveGameToSlot(Options.Get(), TEXT("Options"), 0));

	// A damaged byte on the card.
	TArray<uint8>& Stored = *Card.Files.Find(TEXT("/BASLUS-99001TEST/Options"));
	Stored[Stored.Num() - 1] ^= 0x5a;
	TestNull("A damaged save", UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("... damaged", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::Corrupt);
	TestTrue("Deleted", UGameplayStatics::DeleteGameInSlot(TEXT("Options"), 0));
	TestNull("None", UGameplayStatics::LoadGameFromSlot(TEXT("Options"), 0));
	TestTrue("... not found", UGameplayStatics::GetLastSaveGameResult() == ESaveGameResult::NotFound);

	// The browser's files.
	const TArray<uint8> Sys = FMemoryCardSaveGameSystem::MakeIconSys(TEXT("ShooterGame Settings"), TEXT("icon.ico"));
	TestEqual("The title's first letter in Shift-JIS (full-width S)", (int32(Sys[192]) << 8) | Sys[193], 0x8272);
	TestEqual("The line break after \"ShooterGame \"", (int32(Sys[7]) << 8) | Sys[6], 24);
	TestEqual("The list icon's file", FString(reinterpret_cast<const TCHAR*>(Sys.GetData() + 260)),
		FString(TEXT("icon.ico")));
	const TArray<uint8> Icon = FMemoryCardSaveGameSystem::MakeIcon();
	TestTrue("The icon's id", Icon.Num() > 32768 && Icon[0] == 0 && Icon[1] == 0 && Icon[2] == 1 && Icon[3] == 0);
	TestEqual("A 128 x 128 16-bit texture at its end", Icon.Num(), 20 + 6 * 24 + 20 + 24 + 128 * 128 * 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
