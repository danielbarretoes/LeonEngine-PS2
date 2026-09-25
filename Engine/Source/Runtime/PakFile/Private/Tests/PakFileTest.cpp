#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "IPlatformFilePak.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PakWriter.h"
#include "Templates/UniquePtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Size bytes of a pattern that depends on Seed. */
	TArray<uint8> MakeData(uint8 Seed, int32 Size)
	{
		TArray<uint8> Data;
		Data.SetNumUninitialized(Size);
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Data[Index] = uint8(Seed * 31 + Index * 7 + (Index >> 8));
		}
		return Data;
	}

	/** A pak of a few files as LeonPak lays them out: a mount point of "../../../", then Engine and project paths. */
	TArray<uint8> MakeTestPak(int64 Alignment = 0, bool bReverseOrder = false)
	{
		struct FSource
		{
			const TCHAR* Dest;
			uint8 Seed;
			int32 Size;
		};
		const FSource Sources[] = {
			{"../../../Engine/Content/Maps/Entry.lmap", 1, 3000},
			{"../../../Engine/Config/BaseEngine.ini", 2, 10},
			{"../../../MyGame/Content/Data/Empty.bin", 3, 0},
			{"../../../MyGame/Content/Data/Sub/Deep.bin", 4, 70000},
		};
		FPakWriter Writer(Alignment);
		const int32 Num = UE_ARRAY_COUNT(Sources);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FSource& Source = Sources[bReverseOrder ? Num - 1 - Index : Index];
			Writer.AddFile(Source.Dest, MakeData(Source.Seed, Source.Size));
		}
		TArray<uint8> Pak;
		(void)Writer.Finalize(Pak);
		return Pak;
	}

	/** Reads a whole file through a platform file. */
	bool ReadAll(IPlatformFile& PlatformFile, const FString& Filename, TArray<uint8>& OutData)
	{
		TUniquePtr<IFileHandle> Handle(PlatformFile.OpenRead(*Filename));
		if (!Handle)
		{
			return false;
		}
		OutData.SetNumUninitialized(int32(Handle->Size()));
		return Handle->Read(OutData.GetData(), OutData.Num());
	}

	/** Puts a platform file on top of the chain for a scope, and the previous one back after. */
	class FScopedTopmostPlatformFile
	{
	public:
		explicit FScopedTopmostPlatformFile(IPlatformFile& PlatformFile)
			: Previous(FPlatformFileManager::Get().GetPlatformFile())
		{
			FPlatformFileManager::Get().SetPlatformFile(PlatformFile);
		}

		~FScopedTopmostPlatformFile()
		{
			FPlatformFileManager::Get().SetPlatformFile(Previous);
		}

		FScopedTopmostPlatformFile(const FScopedTopmostPlatformFile&) = delete;
		FScopedTopmostPlatformFile& operator=(const FScopedTopmostPlatformFile&) = delete;

	private:
		IPlatformFile& Previous;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPakFormatRoundTripTest, "System.PakFile.Format.RoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPakFormatRoundTripTest::RunTest(const FString& Parameters)
{
	// What FPakWriter writes, FPakFile reads back: the mount point, every entry's bytes (an empty one too), lookups
	// that ignore case, and the footer's "LPAK" magic at the end of the file.
	const TArray<uint8> Pak = MakeTestPak();
	const int64 FooterSize = FPakInfo::GetSerializedSize();
	if (!TestTrue("A pak was written", Pak.Num() > FooterSize))
	{
		return false;
	}
	const uint8* Footer = Pak.GetData() + Pak.Num() - FooterSize;
	TestTrue(
		"The footer starts with LPAK", Footer[0] == 'L' && Footer[1] == 'P' && Footer[2] == 'A' && Footer[3] == 'K');

	FPakFile PakFile(TEXT("RoundTrip.lpak"), CopyTemp(Pak));
	if (!TestTrue("Valid", PakFile.IsValid()))
	{
		return false;
	}
	TestEqual("Version", PakFile.GetInfo().Version, int32(FPakInfo::PakFile_Version_Latest));
	TestEqual("The mount point is the common folder", PakFile.GetIndexMountPoint(), FString("../../../"));
	TestEqual("Files", PakFile.GetNumFiles(), 4);
	TestEqual("The index ends where the footer starts", PakFile.GetInfo().IndexOffset + PakFile.GetInfo().IndexSize,
		int64(Pak.Num()) - FooterSize);

	TArray<uint8> Data;
	const FPakIndexEntry* Map = PakFile.FindRelative("Engine/Content/Maps/Entry.lmap");
	if (TestNotNull("The map", Map))
	{
		TestTrue("Its bytes", PakFile.ReadEntry(Map->Entry, Data) && Data == MakeData(1, 3000));
	}
	TestTrue("A lookup ignores case", PakFile.FindRelative("engine/CONTENT/maps/entry.LMAP") == Map);
	const FPakIndexEntry* Deep = PakFile.FindRelative("MyGame/Content/Data/Sub/Deep.bin");
	if (TestNotNull("A file two folders down", Deep))
	{
		TestTrue("Its bytes", PakFile.ReadEntry(Deep->Entry, Data) && Data == MakeData(4, 70000));
	}
	const FPakIndexEntry* Empty = PakFile.FindRelative("MyGame/Content/Data/Empty.bin");
	TestTrue("An empty file", Empty != nullptr && Empty->Entry.Size == 0);
	TestNull("A missing file", PakFile.FindRelative("Engine/Content/Maps/Missing.lmap"));
	TestNull("A folder is not a file", PakFile.FindRelative("Engine/Content"));

	bool bSorted = true;
	for (int32 Index = 1; Index < PakFile.GetNumFiles(); ++Index)
	{
		bSorted &= FPakFile::IndexLess(PakFile.GetEntries()[Index - 1], PakFile.GetEntries()[Index]);
	}
	TestTrue("The index is sorted by path hash", bSorted);
	TestTrue("Every hash checks", PakFile.Check());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPakFormatDeterministicTest, "System.PakFile.Format.Deterministic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPakFormatDeterministicTest::RunTest(const FString& Parameters)
{
	// The same files give the same bytes, whatever order they were added in (no times, data in path order); another
	// file's bytes change the pak.
	const TArray<uint8> Forward = MakeTestPak();
	const TArray<uint8> Reversed = MakeTestPak(0, true);
	TestTrue("Byte for byte", Forward == Reversed);

	FPakWriter Changed;
	Changed.AddFile("../../../Engine/Content/Maps/Entry.lmap", MakeData(9, 3000));
	TArray<uint8> Other;
	TestTrue("Written", Changed.Finalize(Other));
	TestFalse("Other bytes, another pak", Other == Forward);

	FPakWriter Duplicate;
	Duplicate.AddFile("../../../A/B.bin", MakeData(1, 10));
	Duplicate.AddFile("../../../a/b.BIN", MakeData(2, 10));
	AddExpectedError("is in the pak twice");
	TestFalse("A path twice (ignoring case) is refused", Duplicate.Finalize(Other));

	TArray<FString> Dests;
	Dests.Add("../../../Engine/Content/A.lasset");
	Dests.Add("../../../MyGame/Content/B.lasset");
	TestEqual("Mount point", FPakWriter::ComputeMountPoint(Dests), FString("../../../"));
	Dests.Reset();
	Dests.Add("Content/Maps/A.lmap");
	Dests.Add("Content/Maps/Sub/B.lmap");
	TestEqual("Mount point, one folder", FPakWriter::ComputeMountPoint(Dests), FString("Content/Maps/"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPakFormatAlignmentTest, "System.PakFile.Format.Alignment",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPakFormatAlignmentTest::RunTest(const FString& Parameters)
{
	// -align=2048 starts every entry's data on a CD sector; the bytes read back the same.
	const TArray<uint8> Packed = MakeTestPak();
	TArray<uint8> Aligned = MakeTestPak(2048);
	TestTrue("Padding makes the pak bigger", Aligned.Num() > Packed.Num());
	FPakFile PakFile(TEXT("Aligned.lpak"), MoveTemp(Aligned));
	if (!TestTrue("Valid", PakFile.IsValid()))
	{
		return false;
	}
	bool bAligned = true;
	for (const FPakIndexEntry& Entry : PakFile.GetEntries())
	{
		bAligned &= Entry.Entry.Offset % 2048 == 0;
	}
	TestTrue("Every entry starts on a 2048-byte boundary", bAligned);
	TArray<uint8> Data;
	const FPakIndexEntry* Deep = PakFile.FindRelative("MyGame/Content/Data/Sub/Deep.bin");
	TestTrue("The bytes", Deep != nullptr && PakFile.ReadEntry(Deep->Entry, Data) && Data == MakeData(4, 70000));
	TestTrue("Every hash checks", PakFile.Check());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPakFormatCorruptionTest, "System.PakFile.Format.CheckDetectsCorruption",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPakFormatCorruptionTest::RunTest(const FString& Parameters)
{
	// LeonPak -test (FPakFile::Check) finds a changed byte in an entry through its SHA-1; a changed byte in the index,
	// a wrong magic or a truncated file stop the pak from opening at all.
	const TArray<uint8> Pak = MakeTestPak();
	TArray<uint8> BadEntry = Pak;
	{
		FPakFile Good(TEXT("Good.lpak"), CopyTemp(Pak));
		const FPakIndexEntry* Deep = Good.FindRelative("MyGame/Content/Data/Sub/Deep.bin");
		if (!TestNotNull("The entry", Deep))
		{
			return false;
		}
		BadEntry[int32(Deep->Entry.Offset + 12345)] ^= 0x01;
	}
	FPakFile Corrupt(TEXT("BadEntry.lpak"), MoveTemp(BadEntry));
	TestTrue("The index is intact", Corrupt.IsValid());
	AddExpectedError("is corrupt: SHA-1");
	TestFalse("Check finds the changed byte", Corrupt.Check());

	TArray<uint8> BadIndex = Pak;
	const int64 IndexOffset = FPakFile(TEXT("Index.lpak"), CopyTemp(Pak)).GetInfo().IndexOffset;
	BadIndex[int32(IndexOffset + 20)] ^= 0x40;
	AddExpectedError("the index is corrupt");
	TestFalse("A changed index does not open", FPakFile(TEXT("BadIndex.lpak"), MoveTemp(BadIndex)).IsValid());

	TArray<uint8> BadMagic = Pak;
	BadMagic[BadMagic.Num() - int32(FPakInfo::GetSerializedSize())] = 'X';
	AddExpectedError("no LPAK magic");
	TestFalse("A wrong magic does not open", FPakFile(TEXT("BadMagic.lpak"), MoveTemp(BadMagic)).IsValid());

	TArray<uint8> Truncated = Pak;
	Truncated.SetNum(Truncated.Num() - 7);
	AddExpectedError("no LPAK magic");
	TestFalse("A truncated pak does not open", FPakFile(TEXT("Truncated.lpak"), MoveTemp(Truncated)).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPakPlatformFileMountTest, "System.PakFile.PlatformFile.MountAndRead",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPakPlatformFileMountTest::RunTest(const FString& Parameters)
{
	// FPakPlatformFile in the chain: the files of a pak mounted at a folder exist, read, list and stat there; they are
	// read-only; IFileManager and FFileHelper reach them once it is the topmost platform file; a later pak with a
	// higher order wins a path both have. The folder does not exist on disk (a pak in memory, PS2 included).
	const FString Root = FPaths::ProjectIntermediateDir() + "Tests/PakFileMount/";
	FPakPlatformFile PakPlatformFile;
	TestTrue("Initialize (no pak in the pak folders)",
		PakPlatformFile.Initialize(&IPlatformFile::GetPlatformPhysical(), ""));
	TestTrue("The lower level", PakPlatformFile.GetLowerLevel() == &IPlatformFile::GetPlatformPhysical());
	if (!TestTrue("Mounted", PakPlatformFile.MountFromMemory(TEXT("Mount.lpak"), MakeTestPak(), 0, *Root)))
	{
		return false;
	}
	TArray<FString> Mounted;
	PakPlatformFile.GetMountedPakFilenames(Mounted);
	TestEqual("One pak", Mounted.Num(), 1);

	const FString Deep = Root + "MyGame/Content/Data/Sub/Deep.bin";
	TestTrue("A file exists", PakPlatformFile.FileExists(*Deep));
	FString Backslashes = Deep;
	Backslashes.ReplaceCharInline('/', '\\');
	TestTrue("With backslashes", PakPlatformFile.FileExists(*Backslashes));
	TestFalse("A missing file", PakPlatformFile.FileExists(*(Root + "MyGame/Content/Data/Missing.bin")));
	TestEqual("Its size", PakPlatformFile.FileSize(*Deep), int64(70000));
	TArray<uint8> Data;
	TestTrue("It reads", ReadAll(PakPlatformFile, Deep, Data) && Data == MakeData(4, 70000));
	{
		TUniquePtr<IFileHandle> Handle(PakPlatformFile.OpenRead(*Deep));
		uint8 Byte = 0;
		TestTrue("Seek and read",
			Handle && Handle->Seek(69999) && Handle->Read(&Byte, 1) && Byte == MakeData(4, 70000)[69999]);
		TestFalse("No read past the end", Handle && Handle->Read(&Byte, 1));
		TestFalse("No write", Handle && Handle->Write(&Byte, 1));
	}
	TestTrue("Read-only", PakPlatformFile.IsReadOnly(*Deep));
	TestNull("Cannot be opened for writing", PakPlatformFile.OpenWrite(*Deep));
	TestFalse("Cannot be deleted", PakPlatformFile.DeleteFile(*Deep));
	const FFileStatData Stat = PakPlatformFile.GetStatData(*Deep);
	TestTrue("Stat", Stat.bIsValid && !Stat.bIsDirectory && Stat.FileSize == 70000 && Stat.bIsReadOnly);

	TestTrue("Its folder exists", PakPlatformFile.DirectoryExists(*(Root + "MyGame/Content/Data")));
	TestTrue("The mount point exists", PakPlatformFile.DirectoryExists(*Root));
	TestFalse("A folder the pak does not have", PakPlatformFile.DirectoryExists(*(Root + "MyGame/Content/Nope")));
	TArray<FString> Files;
	TArray<FString> Folders;
	PakPlatformFile.IterateDirectory(*(Root + "MyGame/Content/Data"),
		[&Files, &Folders](const TCHAR* Path, bool bIsDirectory)
		{
			(bIsDirectory ? Folders : Files).Add(FPaths::GetCleanFilename(FString(Path)));
			return true;
		});
	TestTrue("The folder lists its file", Files.Num() == 1 && Files[0] == "Empty.bin");
	TestTrue("... and its sub folder", Folders.Num() == 1 && Folders[0] == "Sub");
	TArray<FString> Found;
	PakPlatformFile.FindFilesRecursively(Found, *Root, ".bin");
	TestEqual("Recursively", Found.Num(), 2);

	// The chain: IFileManager and FFileHelper go through the topmost platform file.
	{
		FScopedTopmostPlatformFile Topmost(PakPlatformFile);
		TestTrue("Found in the chain",
			FPlatformFileManager::Get().FindPlatformFile("PakFile") == static_cast<IPlatformFile*>(&PakPlatformFile));
		TestTrue("IFileManager", IFileManager::Get().FileExists(*Deep));
		TArray<uint8> Loaded;
		TestTrue("FFileHelper", FFileHelper::LoadFileToArray(Loaded, *(Root + "Engine/Config/BaseEngine.ini")));
		TestTrue("... its bytes", Loaded == MakeData(2, 10));
	}

	// A later pak with a higher order wins; unmounting it shows the first one again.
	FPakWriter Patch;
	Patch.AddFile("../../../MyGame/Content/Data/Sub/Deep.bin", MakeData(7, 100));
	TArray<uint8> PatchPak;
	TestTrue("Patch written", Patch.Finalize(PatchPak));
	TestTrue("Patch mounted",
		PakPlatformFile.MountFromMemory(
			TEXT("Patch.lpak"), MoveTemp(PatchPak), 1, *(Root + "MyGame/Content/Data/Sub/")));
	TestTrue("The patch wins", ReadAll(PakPlatformFile, Deep, Data) && Data == MakeData(7, 100));
	TestTrue("Unmounted", PakPlatformFile.Unmount(TEXT("Patch.lpak")));
	TestTrue("The first pak again", ReadAll(PakPlatformFile, Deep, Data) && Data == MakeData(4, 70000));

	#if PLATFORM_DESKTOP
	// Loose files: allowed outside Shipping, refused when the paks are the only source, except under Saved.
	const FString Loose = FPaths::EngineConfigDir() + "BaseEngine.ini";
	TestTrue("A loose file", PakPlatformFile.FileExists(*Loose));
	PakPlatformFile.SetAllowLooseFiles(false);
	TestFalse("A loose file refused", PakPlatformFile.FileExists(*Loose));
	TestNull("... cannot be opened", PakPlatformFile.OpenRead(*Loose));
	TestTrue("A pak file still reads", PakPlatformFile.FileExists(*Deep));
	TestTrue("The Saved folder stays readable",
		PakPlatformFile.IsNonPakFilenameAllowed(FPaths::ProjectSavedDir() + "Logs/Game.log"));
	PakPlatformFile.SetAllowLooseFiles(true);

	// A relative mount point ("../../../", as LeonPak writes it) is taken from the executable's folder, as in UE.
	FPakFile Relative(TEXT("Relative.lpak"), MakeTestPak());
	FString Expected = FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::BaseDir()), "../../../");
	if (!Expected.EndsWith("/"))
	{
		Expected += "/";
	}
	TestEqual("../../../ from the executable's folder", Relative.GetMountPoint(), Expected);
	#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
