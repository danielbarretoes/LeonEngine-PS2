#include "CoreMinimal.h"
#include "GSFieldPacer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// FGSFieldPacer (Docs/PLANS/ps2-shipping.md N10): which vertical blank the PS2 shows a frame on, from the count of
// fields its interrupt handler keeps.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSFieldPacerFlipFieldTest, "System.GSCore.FieldPacer.FlipField",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSFieldPacerFlipFieldTest::RunTest(const FString& Parameters)
{
	// Shown on field 10, an interval of 2: field 12, whether the frame is ready on field 10 or 11.
	TestEqual("Ready in the first field", FGSFieldPacer::GetFlipField(10, 10, 2), 12u);
	TestEqual("Ready in the second field", FGSFieldPacer::GetFlipField(10, 11, 2), 12u);
	// Blank 12 has begun: the frame waits for the next one instead of flipping in the middle of a field.
	TestEqual("Late for its blank", FGSFieldPacer::GetFlipField(10, 12, 2), 13u);
	TestEqual("Much later", FGSFieldPacer::GetFlipField(10, 15, 2), 16u);
	// 60 (NTSC) or 50 (PAL) frames a second, and an interval of 0 is 1.
	TestEqual("Interval 1", FGSFieldPacer::GetFlipField(10, 10, 1), 11u);
	TestEqual("Interval 1, late", FGSFieldPacer::GetFlipField(10, 11, 1), 12u);
	TestEqual("Interval 0", FGSFieldPacer::GetFlipField(10, 10, 0), 11u);
	TestEqual("Interval 4", FGSFieldPacer::GetFlipField(10, 11, 4), 14u);
	// The count wraps around after 2^32 fields (2.3 years of NTSC).
	TestEqual("Across the wrap", FGSFieldPacer::GetFlipField(0xffffffffu, 0xffffffffu, 2), 1u);
	TestEqual("Late across the wrap", FGSFieldPacer::GetFlipField(0xfffffffeu, 0u, 2), 1u);
	TestTrue("Begun across the wrap", FGSFieldPacer::HasBegun(0xffffffffu, 0u));
	TestFalse("Not begun across the wrap", FGSFieldPacer::HasBegun(1u, 0xffffffffu));
	TestTrue("Begun on its own count", FGSFieldPacer::HasBegun(7u, 7u));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSFieldPacerPaceTest, "System.GSCore.FieldPacer.Pace",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSFieldPacerPaceTest::RunTest(const FString& Parameters)
{
	// A game loop with an interval of 2 whose frames take the given work (in 1/10 of a field) before they are ready:
	// each frame is shown for a whole number of fields, 2 while the work fits in them and 3 when it does not.
	const uint32 WorkTenths[] = {12, 19, 21, 5, 29, 31, 15};
	const uint32 ExpectedFields[] = {2, 2, 3, 2, 3, 4, 2};
	uint32 LastFlip = 100;
	for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(WorkTenths)); ++Index)
	{
		// The loop starts right after the last flip (the blank's start); its work ends WorkTenths later.
		const uint32 ReadyField = LastFlip + (WorkTenths[Index] / 10);
		const uint32 Flip = FGSFieldPacer::GetFlipField(LastFlip, ReadyField, 2);
		TestEqual(*FString::Printf("Frame %d's fields", Index), Flip - LastFlip, ExpectedFields[Index]);
		LastFlip = Flip;
	}

	// The field of each mode: 30 fps is 33.37 ms on NTSC, 25 fps is 40 ms on PAL.
	TestEqual("NTSC field", FGSFieldPacer::GetFieldMicroseconds(EGSVideoMode::Ntsc), 16683u);
	TestEqual("PAL field", FGSFieldPacer::GetFieldMicroseconds(EGSVideoMode::Pal), 20000u);
	TestEqual("NTSC lines", FGSFieldPacer::GetVisibleLines(EGSVideoMode::Ntsc), 448);
	TestEqual("PAL lines", FGSFieldPacer::GetVisibleLines(EGSVideoMode::Pal), 512);
	TestTrue("NTSC rate", FMath::Abs(FGSFieldPacer::GetFieldsPerSecond(EGSVideoMode::Ntsc) - 59.94f) < 0.01f);
	TestEqual("PAL rate", FGSFieldPacer::GetFieldsPerSecond(EGSVideoMode::Pal), 50.0f);
	return true;
}

#endif
