#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "ProjectDescriptor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDescriptorTest, "System.Engine.Projects.ProjectDescriptor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FProjectDescriptorTest::RunTest(const FString& Parameters)
{
	const FString Text =
		"{ \"FileVersion\": 1, \"Description\": \"Test\","
		" \"Modules\": [ { \"Name\": \"Game\", \"Type\": \"Runtime\", \"LoadingPhase\": \"Default\" } ],"
		" \"Plugins\": [ { \"Name\": \"SamplePlugin\", \"Enabled\": true, \"PlatformAllowList\": [ \"Win64\" ] } ],"
		" \"TargetPlatforms\": [ \"Win64\", \"PS2\" ] }";

	TSharedPtr<FJsonObject> Object;
	TestTrue("JSON", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object));
	FProjectDescriptor Descriptor;
	FText FailReason;
	TestTrue("Read", Object.IsValid() && Descriptor.Read(*Object, FailReason));
	TestEqual("Description", Descriptor.Description, TEXT("Test"));
	TestEqual("Modules", Descriptor.Modules.Num(), 1);
	TestTrue("Module type", Descriptor.Modules[0].Type == EHostType::Runtime);
	TestTrue("Module name", Descriptor.Modules[0].Name == FName("Game"));
	TestEqual("Plugins", Descriptor.Plugins.Num(), 1);
	TestTrue("Plugin enabled on Win64", Descriptor.Plugins[0].IsEnabledForPlatform("Win64"));
	TestFalse("Plugin not on PS2", Descriptor.Plugins[0].IsEnabledForPlatform("PS2"));
	TestEqual("Target platforms", Descriptor.TargetPlatforms.Num(), 2);

	// Write and read back.
	FString Written;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Written);
	Descriptor.Write(*Writer);
	TestTrue("Writer closed", Writer->Close());
	TSharedPtr<FJsonObject> Reread;
	FProjectDescriptor Copy;
	TestTrue("Round trip",
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Written), Reread) && Copy.Read(*Reread, FailReason));
	TestTrue(
		"Round trip content", Copy.Modules.Num() == 1 && Copy.Plugins.Num() == 1 && Copy.TargetPlatforms.Num() == 2);

	// Bad descriptors say why.
	TSharedPtr<FJsonObject> BadObject;
	FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(
			"{ \"FileVersion\": 1, \"Modules\": [ { \"Name\": \"X\", \"Type\": \"Nope\" } ] }"),
		BadObject);
	FProjectDescriptor Bad;
	TestFalse("Unknown module type", Bad.Read(*BadObject, FailReason));
	TestTrue("Fail reason", FailReason.ToString().Contains("unknown 'Type'"));
	return true;
}

	#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealDescriptorsTest, "System.Engine.Projects.RealDescriptors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRealDescriptorsTest::RunTest(const FString& Parameters)
{
	// The files in the repository parse.
	FProjectDescriptor ShooterGame;
	FText FailReason;
	TestTrue(
		"ShooterGame.lproj", ShooterGame.Load(FPaths::RootDir() + "Game/ShooterGame/ShooterGame.lproj", FailReason));
	TestTrue("ShooterGame targets Win64 and PS2",
		ShooterGame.TargetPlatforms.Contains("Win64") && ShooterGame.TargetPlatforms.Contains("PS2"));
	TestTrue(
		"ShooterGame module", ShooterGame.Modules.Num() == 1 && ShooterGame.Modules[0].Name == FName("ShooterGame"));

	// A plugin saved into the project's Plugins folder is discovered there, with its content's mount point, and stays
	// off until a project enables it (the engine ships no plugin of its own).
	FPluginDescriptor Sample;
	TestTrue("A descriptor",
		Sample.Read("{ \"FileVersion\": 3, \"FriendlyName\": \"Sample\", \"EnabledByDefault\": false,"
					" \"CanContainContent\": true, \"Modules\": [ { \"Name\": \"SamplePlugin\", \"Type\": \"Runtime\","
					" \"LoadingPhase\": \"Default\", \"PlatformAllowList\": [ \"Win64\" ] } ] }",
			FailReason));
	TestTrue("Off by default", Sample.EnabledByDefault == EPluginEnabledByDefault::Disabled);
	TestTrue("Its module on Win64 only",
		Sample.Modules.Num() == 1 && Sample.Modules[0].IsCompiledForPlatform("Win64") &&
			!Sample.Modules[0].IsCompiledForPlatform("PS2"));
	const FString PluginDir = FPaths::ProjectPluginsDir() + "SamplePlugin/";
	TestTrue("Save", Sample.Save(PluginDir + "SamplePlugin.lplugin", FailReason));
	FPluginDescriptor Reloaded;
	TestTrue("Reload",
		Reloaded.Load(PluginDir + "SamplePlugin.lplugin", FailReason) && Reloaded.FriendlyName == Sample.FriendlyName);

	IPluginManager::Get().RefreshPluginsList();
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("SamplePlugin");
	TestTrue("Discovered", Plugin.IsValid());
	if (Plugin.IsValid())
	{
		TestEqual("Mount point", Plugin->GetMountedAssetPath(), TEXT("/SamplePlugin/"));
		TestTrue("A project plugin", Plugin->GetType() == EPluginType::Project);
		TestFalse("Disabled without a project reference", Plugin->IsEnabled());
	}
	IFileManager::Get().DeleteDirectory(*PluginDir, false, true);
	IPluginManager::Get().RefreshPluginsList();
	TestFalse("Gone with its folder", IPluginManager::Get().FindPlugin("SamplePlugin").IsValid());
	return true;
}

	#endif // PLATFORM_DESKTOP

#endif // WITH_DEV_AUTOMATION_TESTS
