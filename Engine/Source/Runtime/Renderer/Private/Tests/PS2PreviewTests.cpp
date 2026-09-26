#include "CoreMinimal.h"
#include "FramePacer.h"
#include "GSEmulator/GSOpenGLEmulator.h"
#include "Misc/AutomationTest.h"
#include "RendererSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

// The desktop shows the PS2's frame as the console does (Docs/PLANS/ps2-preview.md V1): the TV's aspect ratio, and a
// frame every SyncInterval fields.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2PreviewSettingsTest, "System.Renderer.PS2Preview.Settings",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPS2PreviewSettingsTest::RunTest(const FString& Parameters)
{
	// BaseEngine.ini gives the PS2's values: a 4:3 TV and 30 fps.
	const FRendererSettings Settings = FRendererSettings::Load();
	TestEqual("4:3", Settings.DisplayAspectRatio, 4.0f / 3.0f, 1.0e-4f);
	TestEqual("A frame every 2 fields", Settings.SyncInterval, 2);
	TestEqual("The frame's aspect is not the TV's", Settings.GetDisplayAspectRatio(FIntPoint(640, 448)), 4.0f / 3.0f,
		1.0e-4f);
	FRendererSettings Square;
	Square.DisplayAspectRatio = 0.0f;
	TestEqual("0: the frame's own", Square.GetDisplayAspectRatio(FIntPoint(640, 448)), 640.0f / 448.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2PreviewPresentRectTest, "System.Renderer.PS2Preview.PresentRect",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPS2PreviewPresentRectTest::RunTest(const FString& Parameters)
{
	// The frame's lines at a whole scale, its width from the aspect, centred.
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;
	FGSOpenGLEmulator::GetPresentRect(1280, 896, 4.0f / 3.0f, X, Y, Width, Height);
	TestTrue("1280 x 896: two rows a line, 4:3", Height == 896 && Width == 1195 && X == 42 && Y == 0);
	FGSOpenGLEmulator::GetPresentRect(1920, 1080, 4.0f / 3.0f, X, Y, Width, Height);
	TestTrue("1920 x 1080: two rows a line", Height == 896 && Width == 1195 && Y == 92);
	FGSOpenGLEmulator::GetPresentRect(1280, 896, 0.0f, X, Y, Width, Height);
	TestTrue("Square pixels: the frame twice", Height == 896 && Width == 1280 && X == 0);
	FGSOpenGLEmulator::GetPresentRect(320, 200, 4.0f / 3.0f, X, Y, Width, Height);
	TestTrue("A small window still shows every line", Height == 448 && Width == 597);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2PreviewFramePacerTest, "System.Renderer.PS2Preview.FramePacer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPS2PreviewFramePacerTest::RunTest(const FString& Parameters)
{
	// A frame goes two fields after the last one; a late frame goes at once and the next ones count from it.
	const double Field = FFramePacer::FieldSeconds;
	TestEqual("On time: two fields after", FFramePacer::NextFrameSeconds(10.0, 10.01, 2), 10.0 + (2.0 * Field), 1.0e-9);
	TestEqual("Slightly late: still on the grid", FFramePacer::NextFrameSeconds(10.0, 10.0 + (2.5 * Field), 2),
		10.0 + (2.0 * Field), 1.0e-9);
	TestEqual("A hitch: now", FFramePacer::NextFrameSeconds(10.0, 10.2, 2), 10.2, 1.0e-9);
	TestEqual("60 Hz: one field", FFramePacer::NextFrameSeconds(10.0, 10.0, 1), 10.0 + Field, 1.0e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
