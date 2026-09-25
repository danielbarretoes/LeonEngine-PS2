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
		" \"Plugins\": [ { \"Name\": \"JoltPhysics\", \"Enabled\": true, \"PlatformAllowList\": [ \"Win64\" ] } ],"
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
	FProjectDescriptor ThirdPerson;
	FText FailReason;
	TestTrue(
		"ThirdPerson.lproj", ThirdPerson.Load(FPaths::RootDir() + "Game/ThirdPerson/ThirdPerson.lproj", FailReason));
	TestTrue("ThirdPerson targets PS2", ThirdPerson.TargetPlatforms.Contains("PS2"));
	TestTrue(
		"ThirdPerson module", ThirdPerson.Modules.Num() == 1 && ThirdPerson.Modules[0].Name == FName("ThirdPerson"));

	FPluginDescriptor Jolt;
	TestTrue("JoltPhysics.lplugin",
		Jolt.Load(FPaths::EnginePluginsDir() + "Runtime/JoltPhysics/JoltPhysics.lplugin", FailReason));
	TestTrue("Jolt off by default", Jolt.EnabledByDefault == EPluginEnabledByDefault::Disabled);
	TestTrue("Jolt module on Win64 only",
		Jolt.Modules.Num() == 1 && Jolt.Modules[0].IsCompiledForPlatform("Win64") &&
			!Jolt.Modules[0].IsCompiledForPlatform("PS2"));

	// Discovery.
	IPluginManager::Get().RefreshPluginsList();
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("JoltPhysics");
	TestTrue("JoltPhysics discovered", Plugin.IsValid());
	if (Plugin.IsValid())
	{
		TestEqual("Mount point", Plugin->GetMountedAssetPath(), TEXT("/JoltPhysics/"));
		TestTrue("Engine plugin", Plugin->GetType() == EPluginType::Engine);
		TestFalse("Disabled without a project reference", Plugin->IsEnabled());
	}

	// A saved plugin descriptor loads back.
	const FString Temp = FPaths::ProjectIntermediateDir() + "Tests/Projects/Test.lplugin";
	TestTrue("Save", Jolt.Save(Temp, FailReason));
	FPluginDescriptor Reloaded;
	TestTrue("Reload", Reloaded.Load(Temp, FailReason) && Reloaded.FriendlyName == Jolt.FriendlyName);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Temp), false, true);
	return true;
}

	#endif // PLATFORM_DESKTOP

#endif // WITH_DEV_AUTOMATION_TESTS
