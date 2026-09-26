#include "CoreMinimal.h"
#include "GenericPlatform/DualShockAnalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDualShockAnalogTest, "System.ApplicationCore.DualShock.Analog",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDualShockAnalogTest::RunTest(const FString& Parameters)
{
	// libpad's bytes (128 at rest) and the dead zone of 0.18, rescaled to the full range; a desktop axis goes through
	// the same bytes, so its small tilts are lost and its steps are the DualShock's.
	TestEqual("Rest", FDualShockAnalog::FromByte(128), 0.0f);
	TestEqual("Inside the dead zone", FDualShockAnalog::FromByte(128 + 20), 0.0f);
	TestEqual("Full right", FDualShockAnalog::FromByte(255), (127.0f / 128.0f - 0.18f) / 0.82f, 1.0e-5f);
	TestEqual("Full left", FDualShockAnalog::FromByte(0), -1.0f, 1.0e-5f);
	TestEqual("-1 is byte 0", int32(FDualShockAnalog::ToByte(-1.0f)), 0);
	TestEqual("0 is byte 128", int32(FDualShockAnalog::ToByte(0.0f)), 128);
	TestEqual("1 is byte 255", int32(FDualShockAnalog::ToByte(1.0f)), 255);
	TestEqual("A small desktop tilt is rest", FDualShockAnalog::FromAxis(0.15f), 0.0f);
	TestEqual(
		"Half tilt", FDualShockAnalog::FromAxis(0.5f), FDualShockAnalog::FromByte(FDualShockAnalog::ToByte(0.5f)));
	TestTrue("Half tilt is past the dead zone", FDualShockAnalog::FromAxis(0.5f) > 0.35f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
