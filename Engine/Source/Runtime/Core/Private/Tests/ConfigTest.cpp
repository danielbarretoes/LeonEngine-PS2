#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Logging/LogSuppressionInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogConfigTest, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigSyntaxTest, "System.Core.Config.Syntax",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FConfigSyntaxTest::RunTest(const FString& Parameters)
{
	FConfigFile File;
	File.ProcessInputFileContents("; comment\n"
								  "[/Script/Engine.Engine]\n"
								  "GameEngine=/Script/Engine.GameEngine\n"
								  "  bSmooth = True  \n"
								  "Speed=2.5\n"
								  "Count=42\n"
								  "Quoted=\"  spaced \\\"text\\\" \"\n"
								  "+Paths=A\n"
								  "+Paths=B\n"
								  "+Paths=A\n"
								  ".Dup=X\n"
								  ".Dup=X\n"
								  "[/script/engine.engine]\n"
								  "Speed=3.5\n");

	FString Value;
	TestTrue("GetString", File.GetString("/Script/Engine.Engine", "GameEngine", Value));
	TestEqual("GetString value", Value, TEXT("/Script/Engine.GameEngine"));
	bool bSmooth = false;
	TestTrue("GetBool trims spaces", File.GetBool("/Script/Engine.Engine", "bSmooth", bSmooth) && bSmooth);
	float Speed = 0.f;
	TestTrue("Section and key names ignore case", File.GetFloat("/SCRIPT/Engine.Engine", "speed", Speed));
	TestEqual("Later value replaces", Speed, 3.5f);
	int32 Count = 0;
	TestTrue("GetInt", File.GetInt("/Script/Engine.Engine", "Count", Count) && Count == 42);
	File.GetString("/Script/Engine.Engine", "Quoted", Value);
	TestEqual("Quoted value", Value, TEXT("  spaced \"text\" "));

	TArray<FString> Paths;
	TestEqual("+ adds unique values", File.GetArray("/Script/Engine.Engine", "Paths", Paths), 2);
	TestTrue("Array order", Paths[0] == TEXT("A") && Paths[1] == TEXT("B"));
	TArray<FString> Dups;
	TestEqual(". adds duplicates", File.GetArray("/Script/Engine.Engine", "Dup", Dups), 2);

	// A higher layer: remove one value, clear another key, replace a value.
	File.CombineFromBuffer("[/Script/Engine.Engine]\n-Paths=A\n!Dup\nCount=7\n[New]\nKey=Value\n");
	File.GetArray("/Script/Engine.Engine", "Paths", Paths);
	TestTrue("- removes a value", Paths.Num() == 1 && Paths[0] == TEXT("B"));
	TestEqual("! clears a key", File.GetArray("/Script/Engine.Engine", "Dup", Dups), 0);
	File.GetInt("/Script/Engine.Engine", "Count", Count);
	TestEqual("Replaced", Count, 7);
	TestTrue("New section", File.GetString("New", "Key", Value));

	// Writing and reading back gives the same values.
	FConfigFile RoundTrip;
	RoundTrip.ProcessInputFileContents(File.ToIniString());
	RoundTrip.GetString("/Script/Engine.Engine", "Quoted", Value);
	TestEqual("Round trip keeps quoted spaces", Value, TEXT("  spaced \"text\" "));
	RoundTrip.GetArray("/Script/Engine.Engine", "Paths", Paths);
	TestEqual("Round trip array", Paths.Num(), 1);

	// Command line overrides.
	FConfigFile Engine;
	Engine.Name = "Engine";
	Engine.ProcessInputFileContents("[Core.System]\nA=1\n");
	Engine.OverrideFromCommandline(
		"-foo -ini:Engine:[Core.System]:A=2 -ini:Game:[Core.System]:A=3 -ini:Engine:[New]:B=\"x y\"");
	Engine.GetString("Core.System", "A", Value);
	TestEqual("-ini: override", Value, TEXT("2"));
	Engine.GetString("New", "B", Value);
	TestEqual("-ini: quoted override", Value, TEXT("x y"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigHierarchyTest, "System.Core.Config.Hierarchy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FConfigHierarchyTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Layers = FConfigCacheIni::GetHierarchy("Engine", "PS2");
	TestTrue("Base first", Layers[0].EndsWith("Engine/Config/Base.ini"));
	TestTrue("Base<Name>", Layers[1].EndsWith("Engine/Config/BaseEngine.ini"));
	TestTrue("Engine platform extension", Layers[3].EndsWith("Engine/Platforms/PS2/Config/PS2Engine.ini"));
	TestTrue("Project default", Layers[4].EndsWith("Config/DefaultEngine.ini"));
	TestTrue("Project platform", Layers[5].EndsWith("Config/PS2/PS2Engine.ini"));

	// The real engine files load (PS2: when staged next to the ELF).
	FConfigFile Engine;
	if (FConfigCacheIni::LoadLocalIniFile(Engine, "Engine", true, "PS2"))
	{
		FString GameEngine;
		TestTrue("BaseEngine.ini", Engine.GetString("/Script/Engine.Engine", "GameEngine", GameEngine));
		int32 ResolutionX = 0;
		TestTrue("PS2Engine.ini overrides the base",
			Engine.GetInt("/Script/Engine.GameViewportClient", "DefaultResolutionX", ResolutionX));
		TestEqual("PS2 resolution", ResolutionX, 640);
	}
	else
	{
		AddInfo("Engine config not found (not staged): layer order checked only");
	}
	return true;
}

	#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigCacheTest, "System.Core.Config.Cache",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigCacheTest::RunTest(const FString& Parameters)
{
	// A private cache over a temporary user layer: what SetX + Flush write is what the next load sees.
	const FString Dir = FPaths::ProjectIntermediateDir() + "Tests/Config/";
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	const FString UserFile = Dir + "User.ini";

	FConfigCacheIni Cache;
	FConfigFile& File = Cache.Add(UserFile);
	File.ProcessInputFileContents("[Section]\nKept=1\n");

	Cache.SetString("Section", "Key", "Value", UserFile);
	Cache.SetInt("Section", "Number", 5, UserFile);
	TArray<FString> List = {"a", "b"};
	Cache.SetArray("Section", "List", List, UserFile);
	FString Value;
	TestTrue("Set is visible", Cache.GetString("Section", "Key", Value, UserFile) && Value == TEXT("Value"));
	Cache.Flush(false);

	FConfigFile Saved;
	Saved.Read(UserFile);
	TestTrue("Saved value", Saved.GetString("Section", "Key", Value) && Value == TEXT("Value"));
	TestFalse("Only changes are saved", Saved.GetString("Section", "Kept", Value));
	TArray<FString> SavedList;
	TestEqual("Saved array", Saved.GetArray("Section", "List", SavedList), 2);

	// The file is a user layer: it clears the array it replaces.
	FConfigFile Lower;
	Lower.ProcessInputFileContents("[Section]\n+List=old\n");
	FString SavedText;
	FFileHelper::LoadFileToString(SavedText, *UserFile);
	Lower.CombineFromBuffer(SavedText);
	Lower.GetArray("Section", "List", SavedList);
	TestTrue("User layer replaces arrays", SavedList.Num() == 2 && SavedList[0] == TEXT("a"));

	IFileManager::Get().DeleteDirectory(*Dir, false, true);

	// The global config is up in programs that call InitializeConfigSystem.
	if (GConfig != nullptr)
	{
		TestTrue("GEngineIni loaded", GConfig->DoesSectionExist("/Script/Engine.Engine", GEngineIni));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigUserLayerArraysAndRemovalsTest, "System.Core.Config.UserLayerArraysAndRemovals",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigUserLayerArraysAndRemovalsTest::RunTest(const FString& Parameters)
{
	// The saved user layer, applied over the lower layers, gives back what was set: a one-value array replaces the
	// whole array, duplicates stay, and a removed key is gone from the lower layers too.
	const FString Dir = FPaths::ProjectIntermediateDir() + "Tests/ConfigArrays/";
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	const FString UserFile = Dir + "User.ini";
	const FString LowerText = "[Section]\n+List=a\n+List=b\nGone=1\n+GoneList=g\nKeep=2\n";

	FConfigCacheIni Cache;
	Cache.Add(UserFile).ProcessInputFileContents(LowerText);
	Cache.SetArray("Section", "List", {TEXT("c")}, UserFile);
	Cache.SetArray("Section", "Dups", {TEXT("x"), TEXT("x")}, UserFile);
	TestTrue("Removed", Cache.RemoveKey("Section", "Gone", UserFile));
	TestTrue("Removed too", Cache.RemoveKey("Section", "GoneList", UserFile));
	Cache.Flush(false);

	FString SavedText;
	TestTrue("Saved", FFileHelper::LoadFileToString(SavedText, *UserFile));
	FConfigFile Loaded;
	Loaded.ProcessInputFileContents(LowerText);
	Loaded.CombineFromBuffer(SavedText);
	TArray<FString> Values;
	TestEqual("A one-value array replaces the array", Loaded.GetArray("Section", "List", Values), 1);
	TestTrue("Its value", Values.Num() == 1 && Values[0] == TEXT("c"));
	TestEqual("Duplicates kept", Loaded.GetArray("Section", "Dups", Values), 2);
	FString Value;
	TestFalse("A removed key is gone", Loaded.GetString("Section", "Gone", Value));
	TestEqual("A removed array is gone", Loaded.GetArray("Section", "GoneList", Values), 0);
	TestTrue("The rest kept", Loaded.GetString("Section", "Keep", Value) && Value == TEXT("2"));

	// Saving the user layer again (a second flush merges into the file on disk) keeps the same meaning.
	Cache.SetString("Section", "Other", "1", UserFile);
	Cache.Flush(false);
	TestTrue("Saved again", FFileHelper::LoadFileToString(SavedText, *UserFile));
	FConfigFile Again;
	Again.ProcessInputFileContents(LowerText);
	Again.CombineFromBuffer(SavedText);
	TestEqual("Still one value", Again.GetArray("Section", "List", Values), 1);
	TestFalse("Still gone", Again.GetString("Section", "Gone", Value));

	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogFileAndVerbosityTest, "System.Core.Logging.FileAndVerbosity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLogFileAndVerbosityTest::RunTest(const FString& Parameters)
{
	const FString Filename = FPaths::ProjectIntermediateDir() + "Tests/Log/Test.log";
	IFileManager::Get().Delete(*Filename);
	{
		FOutputDeviceFile LogFile(*Filename, true);
		LogFile.Serialize("first line", ELogVerbosity::Display, FName("LogConfigTest"));
		LogFile.TearDown();
	}
	FString Text;
	TestTrue("Log file written", FFileHelper::LoadFileToString(Text, *Filename));
	TestTrue("Line format", Text.Contains("]LogConfigTest: Display: first line"));

	// A second log backs the first one up.
	{
		FOutputDeviceFile LogFile(*Filename);
		LogFile.Serialize("second", ELogVerbosity::Log, FName("LogConfigTest"));
	}
	TArray<FString> Backups;
	IFileManager::Get().FindFiles(Backups, *(FPaths::GetPath(Filename) + "/Test-backup-*.log"), true, false);
	TestEqual("Backup created", Backups.Num(), 1);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Filename), false, true);

	// Verbosity from "Category Verbosity" commands.
	FLogSuppressionInterface::Get().ApplyLogCommands("LogConfigTest Verbose, LogUnknownCategory Log");
	TestTrue("Verbose on", LogConfigTest.GetVerbosity() == ELogVerbosity::Verbose);
	FLogSuppressionInterface::Get().ApplyLogCommands("LogConfigTest Off");
	TestTrue("Off", LogConfigTest.GetVerbosity() == ELogVerbosity::NoLogging);
	LogConfigTest.ResetToDefault();
	return true;
}

	#endif // PLATFORM_DESKTOP

#endif // WITH_DEV_AUTOMATION_TESTS
