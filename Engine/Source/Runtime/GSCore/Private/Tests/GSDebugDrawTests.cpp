#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDebugDraw.h"
#include "GSDrawEnvironment.h"
#include "GSTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// FGSDebugDraw (Docs/PLANS/ps2-shipping.md N2): the debug text and rectangles every backend draws the same, recorded as
// GS register writes.

namespace
{
	[[nodiscard]] FGSDrawEnvironment MakeEnvironment()
	{
		FGSDrawEnvironment Environment;
		Environment.Width = 640;
		Environment.Height = 448;
		return Environment;
	}

	[[nodiscard]] int32 CountWrites(const FGSCommandList& List, EGSRegister Register)
	{
		int32 Count = 0;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			Count += Write.Register == Register ? 1 : 0;
		}
		return Count;
	}

	[[nodiscard]] const FGSRegisterWrite* FindFirst(const FGSCommandList& List, EGSRegister Register)
	{
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			if (Write.Register == Register)
			{
				return &Write;
			}
		}
		return nullptr;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSDebugDrawStringTest, "System.GSCore.DebugDraw.String",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSDebugDrawStringTest::RunTest(const FString& Parameters)
{
	const FGSDrawEnvironment Environment = MakeEnvironment();
	const FGSRGBAQ Color = FGSDebugDraw::UnitColor(1.0f, 0.5f, 0.0f);
	TestEqual("Opaque colour", uint32(Color.A), 0x80u);
	TestEqual("Red at 0xff", uint32(Color.R), 0xffu);

	FGSCommandList List;
	FGSDebugDraw::DrawString(List, Environment, 10.0f, 20.0f, "I", Color);
	const TArray<FGSRegisterWrite>& Writes = List.GetWrites();
	if (!TestTrue("Something recorded", Writes.Num() > 4))
	{
		return false;
	}
	// An overlay: the depth test off first, back on last.
	TestTrue("Depth test off first",
		Writes[0].Register == EGSRegister::TEST_1 && Writes[0].Value == FGSDrawEnvironment::DepthTest(false).Encode());
	TestTrue("Depth test back on",
		Writes.Last().Register == EGSRegister::TEST_1 &&
			Writes.Last().Value == FGSDrawEnvironment::DepthTest(true).Encode());
	const FGSRegisterWrite* Prim = FindFirst(List, EGSRegister::PRIM);
	TestTrue("Sprites", Prim != nullptr && FGSPrim::Decode(Prim->Value).Type == EGSPrimitive::Sprite);
	// 'I' is a bar on top, a stem and a bar at the bottom: one sprite (two vertices) per run of each of its 7 rows.
	TestEqual("Two vertices per row", CountWrites(List, EGSRegister::XYZ2), 14);
	const FGSRegisterWrite* First = FindFirst(List, EGSRegister::XYZ2);
	const FGSXYZ Expected = Environment.PixelVertex(10.0f + 2.0f, 20.0f);
	TestTrue("The top bar starts one cell in", First != nullptr && First->Value == Expected.Encode());

	// Lowercase draws uppercase (but for the "ms" of the timings); what the font lacks draws nothing.
	FGSCommandList Upper;
	FGSDebugDraw::DrawString(Upper, Environment, 0.0f, 0.0f, "A", Color);
	FGSCommandList Lower;
	FGSDebugDraw::DrawString(Lower, Environment, 0.0f, 0.0f, "a", Color);
	TestEqual("a is A", Lower.GetWrites().Num(), Upper.GetWrites().Num());
	FGSCommandList Unknown;
	FGSDebugDraw::DrawString(Unknown, Environment, 0.0f, 0.0f, "~", Color);
	TestEqual("No glyph, no vertex", CountWrites(Unknown, EGSRegister::XYZ2), 0);
	FGSCommandList Empty;
	FGSDebugDraw::DrawString(Empty, Environment, 0.0f, 0.0f, "", Color);
	TestEqual("An empty string records nothing", Empty.GetWrites().Num(), 0);

	TestEqual("Width at scale 1", FGSDebugDraw::GetTextWidth("ms 12"), 60.0f);
	TestEqual("Width at scale 0.5", FGSDebugDraw::GetTextWidth("ab", 0.5f), 12.0f);
	TestEqual("Height", FGSDebugDraw::GetTextHeight(), 14.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSDebugDrawRectTest, "System.GSCore.DebugDraw.Rect",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSDebugDrawRectTest::RunTest(const FString& Parameters)
{
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList Opaque;
	FGSDebugDraw::DrawRect(Opaque, Environment, 30.0f, 40.0f, 10.0f, 5.0f, FGSDebugDraw::UnitColor(0.2f, 0.2f, 0.2f));
	const FGSRegisterWrite* Prim = FindFirst(Opaque, EGSRegister::PRIM);
	TestTrue("An opaque sprite",
		Prim != nullptr && FGSPrim::Decode(Prim->Value).Type == EGSPrimitive::Sprite &&
			!FGSPrim::Decode(Prim->Value).bAlphaBlend);
	TestEqual("Two corners", CountWrites(Opaque, EGSRegister::XYZ2), 2);
	const FGSRegisterWrite* Corner = FindFirst(Opaque, EGSRegister::XYZ2);
	TestTrue("The top left corner first, whatever the order given",
		Corner != nullptr && Corner->Value == Environment.PixelVertex(10.0f, 5.0f).Encode());

	FGSCommandList Blended;
	FGSDebugDraw::DrawRect(
		Blended, Environment, 0.0f, 0.0f, 8.0f, 8.0f, FGSDebugDraw::UnitColor(0.0f, 0.0f, 0.0f, 0.5f));
	const FGSRegisterWrite* BlendedPrim = FindFirst(Blended, EGSRegister::PRIM);
	TestTrue("Blended below full alpha", BlendedPrim != nullptr && FGSPrim::Decode(BlendedPrim->Value).bAlphaBlend);
	const FGSRegisterWrite* Color = FindFirst(Blended, EGSRegister::RGBAQ);
	TestTrue("Half alpha is 0x40", Color != nullptr && FGSRGBAQ::Decode(Color->Value).A == 0x40);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
