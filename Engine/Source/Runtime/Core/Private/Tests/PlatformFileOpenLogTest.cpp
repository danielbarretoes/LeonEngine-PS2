#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/IPlatformFileOpenLogWrapper.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformFileOpenLogTest, "System.Core.HAL.PlatformFileOpenLog",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPlatformFileOpenLogTest::RunTest(const FString& Parameters)
{
	// -LogFileOpenOrder (Docs/PLANS/ps2-shipping.md N23): the wrapper notes each file once, the first time it opens
	// for reading, by its staged path ("Engine/..." under the engine), and its order text is what LeonPak -order=
	// reads.
	FPlatformFileOpenLog OpenLog;
	TestFalse("Off without the switch", OpenLog.ShouldBeUsed(nullptr, TEXT("-nullrhi")));
	TestTrue("On with it", OpenLog.ShouldBeUsed(nullptr, TEXT("-nullrhi -LogFileOpenOrder")));
	TestTrue("Initialized", OpenLog.Initialize(&IPlatformFile::GetPlatformPhysical(), TEXT("")));
	TestEqual("An engine file's staged path",
		FPlatformFileOpenLog::GetOrderPath(*(FPaths::EngineDir() + TEXT("Config/BaseEngine.ini"))),
		FString(TEXT("Engine/Config/BaseEngine.ini")));

	const FString Filename = FPaths::ProjectIntermediateDir() + TEXT("Tests/FileOpenLog/Opened.txt");
	if (!FFileHelper::SaveStringToFile(TEXT("Leon"), *Filename))
	{
		// A read-only device (the PS2's host: in TestPAL): the path rules above are the test.
		return true;
	}
	TestTrue("Not a missing file", OpenLog.OpenRead(*(Filename + TEXT(".missing"))) == nullptr);
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		IFileHandle* Handle = OpenLog.OpenRead(*Filename);
		TestNotNull("Opened", Handle);
		delete Handle;
	}
	TestEqual("Noted once", OpenLog.GetOpenOrder().Num(), 1);
	TestEqual("The order text", OpenLog.MakeOrderText(),
		TEXT("\"") + FPlatformFileOpenLog::GetOrderPath(*Filename) + TEXT("\" 1\n"));
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Filename), false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
