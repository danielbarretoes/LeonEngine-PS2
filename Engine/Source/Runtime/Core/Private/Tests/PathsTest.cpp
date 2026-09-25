#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathsStringTest, "System.Core.Misc.Paths.Strings",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPathsStringTest::RunTest(const FString& Parameters)
{
	TestEqual("GetExtension", FPaths::GetExtension("Dir/File.Ext.png"), TEXT("png"));
	TestEqual("GetExtension with dot", FPaths::GetExtension("File.png", true), TEXT(".png"));
	TestEqual("GetExtension of a dotted folder", FPaths::GetExtension("Dir.d/File"), TEXT(""));
	TestEqual("GetCleanFilename", FPaths::GetCleanFilename("A/B\\C.txt"), TEXT("C.txt"));
	TestEqual("GetCleanFilename of a device path", FPaths::GetCleanFilename("host:Game.elf"), TEXT("Game.elf"));
	TestEqual("GetBaseFilename", FPaths::GetBaseFilename("A/B/C.txt"), TEXT("C"));
	TestEqual("GetBaseFilename keeping the path", FPaths::GetBaseFilename("A/B/C.txt", false), TEXT("A/B/C"));
	TestEqual("GetPath", FPaths::GetPath("A/B/C.txt"), TEXT("A/B"));
	TestEqual("GetPathLeaf", FPaths::GetPathLeaf("A/B/"), TEXT("B"));
	TestEqual("ChangeExtension", FPaths::ChangeExtension("A/B.txt", "ini"), TEXT("A/B.ini"));
	TestEqual("ChangeExtension without one", FPaths::ChangeExtension("A.d/B", "ini"), TEXT("A.d/B"));
	TestEqual("SetExtension without one", FPaths::SetExtension("A/B", ".ini"), TEXT("A/B.ini"));
	TestEqual("Combine", FPaths::Combine("A", FString("B/"), "C.txt"), TEXT("A/B/C.txt"));

	TestTrue("IsRelative", FPaths::IsRelative("A/B"));
	TestFalse("Absolute (drive)", FPaths::IsRelative("C:/A"));
	TestFalse("Absolute (root)", FPaths::IsRelative("/usr/lib"));
	TestFalse("Absolute (PS2 device)", FPaths::IsRelative("host:Engine/Config"));
	TestTrue("IsDrive", FPaths::IsDrive("C:"));
	TestTrue("IsDrive device", FPaths::IsDrive("cdrom0:"));
	TestFalse("IsDrive path", FPaths::IsDrive("C:/Dir"));

	FString Collapsed = "C:/A/B/../C/./D.txt";
	TestTrue("CollapseRelativeDirectories", FPaths::CollapseRelativeDirectories(Collapsed));
	TestEqual("Collapsed", Collapsed, TEXT("C:/A/C/D.txt"));
	FString KeepsSlash = "A/B/../";
	FPaths::CollapseRelativeDirectories(KeepsSlash);
	TestEqual("Collapse keeps the trailing slash", KeepsSlash, TEXT("A/"));
	FString AboveStart = "A/../../B";
	TestFalse("Collapse above the start", FPaths::CollapseRelativeDirectories(AboveStart));

	FString Directory = "A\\B\\";
	FPaths::NormalizeDirectoryName(Directory);
	TestEqual("NormalizeDirectoryName", Directory, TEXT("A/B"));
	FString DriveRoot = "C:/";
	FPaths::NormalizeDirectoryName(DriveRoot);
	TestEqual("NormalizeDirectoryName keeps C:/", DriveRoot, TEXT("C:/"));

	FString Relative = "C:/Root/Game/Content/Map.lmap";
	TestTrue("MakePathRelativeTo", FPaths::MakePathRelativeTo(Relative, "C:/Root/Engine/"));
	TestEqual("MakePathRelativeTo result", Relative, TEXT("../Game/Content/Map.lmap"));
	TestEqual("ConvertRelativePathToFull with a base",
		FPaths::ConvertRelativePathToFull("C:/Root/Bin/", "../Config/A.ini"), TEXT("C:/Root/Config/A.ini"));
	TestTrue("IsUnderDirectory", FPaths::IsUnderDirectory("C:/Root/Game/X", "C:/Root/Game/"));
	TestFalse("IsUnderDirectory prefix only", FPaths::IsUnderDirectory("C:/Root/GameX", "C:/Root/Game"));
	TestTrue("IsSamePath", FPaths::IsSamePath("C:/Root/./A/", "c:/root/a"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathsDirectoriesTest, "System.Core.Misc.Paths.Directories",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPathsDirectoriesTest::RunTest(const FString& Parameters)
{
	const FString BaseDir = FPlatformProcess::BaseDir();
	TestTrue("BaseDir ends with a separator", BaseDir.EndsWith("/") || BaseDir.EndsWith(":") || BaseDir.EndsWith("\\"));
	TestTrue("EngineDir ends with Engine/", FPaths::EngineDir().EndsWith("Engine/"));
	TestTrue("Engine content is under the engine", FPaths::EngineContentDir().StartsWith(FPaths::EngineDir()));
	TestTrue("ProjectDir ends with '/'", FPaths::ProjectDir().EndsWith("/"));

	// The running executable is where BaseDir says (PS2: read through host: in PCSX2).
	const FString Executable = BaseDir + FPlatformProcess::ExecutableName(false);
	TestTrue("Executable exists", FPaths::FileExists(Executable));
	TestTrue("Executable size", IFileManager::Get().FileSize(*Executable) > 0);

	#if PLATFORM_DESKTOP
	TestTrue("Engine folder exists", FPaths::DirectoryExists(FPaths::EngineDir()));
	TestTrue("Engine config exists", FPaths::FileExists(FPaths::EngineConfigDir() + "BaseEngine.ini"));
	TestTrue("Root holds Engine/", FPaths::DirectoryExists(FPaths::RootDir() + "Engine"));
	TestFalse("Engine dir is absolute", FPaths::IsRelative(FPaths::EngineDir()));

	// The renderer's shaders sit under Engine/Shaders (UE: the /Engine/Shaders virtual folder).
	TestTrue("Engine shaders", FPaths::FileExists(FPaths::Combine(FPaths::EngineDir(), "Shaders/blinn_phong.vert")));
	#endif
	return true;
}

	#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformFileTest, "System.Core.HAL.PlatformFile",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlatformFileTest::RunTest(const FString& Parameters)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager& FileManager = IFileManager::Get();

	const FString Root = FPaths::ProjectIntermediateDir() + "Tests/PlatformFile/";
	FileManager.DeleteDirectory(*Root, false, true);
	TestTrue("CreateDirectoryTree", PlatformFile.CreateDirectoryTree(*(Root + "A/B")));
	TestTrue("DirectoryExists", PlatformFile.DirectoryExists(*(Root + "A/B/")));

	// Save / load through the temporary file and rename.
	const FString File = Root + "A/Text.txt";
	TestTrue("SaveStringToFile", FFileHelper::SaveStringToFile("Line 1\nLine 2", *File));
	FString Loaded;
	TestTrue("LoadFileToString", FFileHelper::LoadFileToString(Loaded, *File));
	TestEqual("Round trip", Loaded, TEXT("Line 1\nLine 2"));
	TestTrue("SaveStringToFile replaces", FFileHelper::SaveStringToFile("New", *File));
	FFileHelper::LoadFileToString(Loaded, *File);
	TestEqual("Replaced", Loaded, TEXT("New"));
	TestEqual("FileSize", PlatformFile.FileSize(*File), int64(3));

	TArray<FString> Lines;
	FFileHelper::SaveStringToFile("a\r\nb\nc", *File);
	TestTrue("LoadFileToStringArray", FFileHelper::LoadFileToStringArray(Lines, *File));
	TestEqual("Line count", Lines.Num(), 3);

	// A UTF-8 BOM is dropped when loading.
	TestTrue("Save with BOM", FFileHelper::SaveStringToFile("bom", *File, FFileHelper::EEncodingOptions::ForceUTF8));
	FFileHelper::LoadFileToString(Loaded, *File);
	TestEqual("BOM stripped", Loaded, TEXT("bom"));

	// Binary through archives, larger than the archive buffers.
	TArray<uint8> Bytes;
	for (int32 Index = 0; Index < 10000; ++Index)
	{
		Bytes.Add(uint8(Index * 7));
	}
	const FString BinaryFile = Root + "A/B/Data.bin";
	TestTrue("SaveArrayToFile", FFileHelper::SaveArrayToFile(Bytes, *BinaryFile));
	TArray<uint8> LoadedBytes;
	TestTrue("LoadFileToArray", FFileHelper::LoadFileToArray(LoadedBytes, *BinaryFile));
	TestTrue("Binary round trip", LoadedBytes == Bytes);

	TArray<FString> Found;
	FileManager.FindFiles(Found, *(Root + "A/*.txt"), true, false);
	TestEqual("FindFiles wildcard", Found.Num(), 1);
	FileManager.FindFilesRecursive(Found, *Root, "*.bin", true, false);
	TestEqual("FindFilesRecursive", Found.Num(), 1);

	TestTrue("Move", FileManager.Move(*(Root + "Moved.bin"), *BinaryFile));
	TestFalse("Moved away", PlatformFile.FileExists(*BinaryFile));
	TestTrue("Copy", FileManager.Copy(*(Root + "Copy.bin"), *(Root + "Moved.bin")));
	TestEqual("Copy size", PlatformFile.FileSize(*(Root + "Copy.bin")), int64(10000));
	TestTrue("Timestamp", PlatformFile.GetTimeStamp(*(Root + "Copy.bin")) > FDateTime(2020, 1, 1));

	TestTrue("DeleteDirectory tree", FileManager.DeleteDirectory(*Root, true, true));
	TestFalse("Tree gone", PlatformFile.DirectoryExists(*Root));
	return true;
}

	#endif // PLATFORM_DESKTOP

#endif // WITH_DEV_AUTOMATION_TESTS
