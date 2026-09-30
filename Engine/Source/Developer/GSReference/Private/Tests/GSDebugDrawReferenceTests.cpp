#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDebugDraw.h"
#include "GSDrawEnvironment.h"
#include "GSReferenceRasterizer.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"

#if WITH_DEV_AUTOMATION_TESTS

// FGSDebugDraw's text on the reference rasterizer (Docs/PLANS/ps2-polish.md P5b): the compiled-in font uploaded to the
// texture arena and drawn over a frame the way the PS2's error screen draws it.

namespace
{

	constexpr int32 FrameWidth = 640;
	constexpr int32 FrameHeight = 448;
	/** The frame as verified by eye (Docs/PLANS/ps2-polish.md P5b). */
	constexpr uint32 ExpectedDebugTextCrc = 0xa5b04557u;

	/** A 640 x 448 PSMCT32 frame at FBP 0, its PSMZ24 Z buffer after it (the scene renderer's tests' layout). */
	FGSDrawEnvironment MakeEnvironment()
	{
		FGSDrawEnvironment Environment;
		Environment.Frame.FBP = 0;
		Environment.Frame.FBW = FrameWidth / 64;
		Environment.Frame.PSM = EGSPixelFormat::PSMCT32;
		Environment.ZBuf.ZBP = 140;
		Environment.ZBuf.PSM = EGSPixelFormat::PSMZ24;
		Environment.Width = FrameWidth;
		Environment.Height = FrameHeight;
		return Environment;
	}

	/** Whether a pixel shows something other than Backdrop (its colour: a glyph's empty texels still write alpha). */
	bool IsInk(const FColor& Pixel, const FColor& Backdrop)
	{
		return Pixel.R != Backdrop.R || Pixel.G != Backdrop.G || Pixel.B != Backdrop.B;
	}

	/** The pixels of a box of the frame that show something other than Backdrop. */
	int32 CountInk(const TArray<FColor>& Frame, const FColor& Backdrop, int32 X0, int32 Y0, int32 X1, int32 Y1)
	{
		int32 Count = 0;
		for (int32 Y = FMath::Max(Y0, 0); Y < FMath::Min(Y1, FrameHeight); ++Y)
		{
			for (int32 X = FMath::Max(X0, 0); X < FMath::Min(X1, FrameWidth); ++X)
			{
				Count += IsInk(Frame[(Y * FrameWidth) + X], Backdrop) ? 1 : 0;
			}
		}
		return Count;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSReferenceDebugTextTest, "System.GSReference.DebugDraw.Text",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSReferenceDebugTextTest::RunTest(const FString& Parameters)
{
	// Both sizes over the error screen's red, in its colours, with a panel behind a line: every line's ink lies in the
	// box its metrics give (from its pen to MeasureString, from its top to its line height), the glyphs' edges are
	// blended (colours between the text's and the backdrop's), and the frame is the one verified by eye (its CRC).
	const FGSDrawEnvironment Environment = MakeEnvironment();
	const uint32 FontBlock = FGSDrawEnvironment::TextureArenaFirstBlock;
	FGSCommandList List;
	Environment.Append(List);
	FGSDebugDraw::UploadFont(List, FontBlock);
	FGSDebugDraw::DrawRect(List, Environment, 0.0f, 0.0f, float(FrameWidth), float(FrameHeight),
		FGSDebugDraw::UnitColor(0.25f, 0.02f, 0.02f));
	FGSDebugDraw::DrawRect(
		List, Environment, 20.0f, 150.0f, 320.0f, 170.0f, FGSDebugDraw::UnitColor(0.0f, 0.0f, 0.0f, 0.5f));

	struct FLine
	{
		const char* Text;
		EGSDebugFont Font;
		int32 X;
		int32 Y;
		FGSRGBAQ Color;
	};
	const FLine Lines[] = {
		{"The game stopped (exit code 1)", EGSDebugFont::Small, 24, 24, FGSDebugDraw::UnitColor(1.0f, 0.9f, 0.6f)},
		{"LogPakFile: Error: cannot open 'host:ShooterGame/Content/Paks/ShooterGame-PS2.lpak'", EGSDebugFont::Tiny, 24,
			52, FGSDebugDraw::UnitColor(1.0f, 0.75f, 0.75f)},
		// UTF-8 Latin-1: an inverted question mark, n with tilde, the acute capitals and u with diaeresis.
		{"Kerning: AVA Wave To; Latin-1: \xc2\xbf"
		 "animal a\xc3\xb1o? \xc3\x81\xc3\x89\xc3\x8d\xc3\x93\xc3\x9a \xc3\xbc",
			EGSDebugFont::Tiny, 24, 70, FGSDebugDraw::UnitColor(0.85f, 0.85f, 0.95f)},
		{"FPS 30.0  MS 33.3  RAM 21.4 / 24.0 MB", EGSDebugFont::Small, 24, 152,
			FGSDebugDraw::UnitColor(1.0f, 1.0f, 1.0f)},
		{"TexturedCanvas", EGSDebugFont::Tiny, 400, 200, FGSDebugDraw::UnitColor(0.9f, 0.9f, 0.8f)},
	};
	for (const FLine& Line : Lines)
	{
		FGSDebugDraw::DrawString(
			List, Environment, FontBlock, float(Line.X), float(Line.Y), Line.Text, Line.Color, Line.Font);
	}

	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const TArray<FColor> Frame = Rasterizer.ReadFrame(Environment.Frame, FrameWidth, FrameHeight);
	const FColor Backdrop = Frame[(300 * FrameWidth) + 600];
	const FColor Panel = Frame[(169 * FrameWidth) + 319];

	for (const FLine& Line : Lines)
	{
		const FColor& Behind = Line.Y == 152 ? Panel : Backdrop;
		const int32 Width = FGSDebugDraw::MeasureString(Line.Text, Line.Font);
		const int32 Height = FGSDebugDraw::GetLineHeight(Line.Font);
		const int32 Inside = CountInk(Frame, Behind, Line.X, Line.Y, Line.X + Width + 1, Line.Y + Height);
		// A glyph may overhang its advance by a pixel (its bearing), never more.
		const int32 Around = CountInk(Frame, Behind, Line.X - 2, Line.Y - 2, Line.X + Width + 3, Line.Y + Height + 2);
		TestTrue(*FString::Printf("'%s' draws", Line.Text), Inside > Width);
		TestEqual(*FString::Printf("'%s' inside its box", Line.Text), Around, Inside);
	}
	// Antialiased: the title's pixels include both full-colour and in-between ones.
	int32 Full = 0;
	int32 Partial = 0;
	for (int32 Y = 24; Y < 38; ++Y)
	{
		for (int32 X = 24; X < 24 + FGSDebugDraw::MeasureString(Lines[0].Text); ++X)
		{
			const FColor Pixel = Frame[(Y * FrameWidth) + X];
			Full += Pixel.R == 255 && Pixel.G >= 228 && Pixel.G <= 231 ? 1 : 0;
			Partial += IsInk(Pixel, Backdrop) && Pixel.R < 250 && Pixel.R > Backdrop.R ? 1 : 0;
		}
	}
	TestTrue("Opaque glyph pixels in the text's colour", Full > 20);
	TestTrue("Blended edges", Partial > 50);

	const uint32 Crc = FCrc::MemCrc32(Frame.GetData(), Frame.Num() * int32(sizeof(FColor)));
	UE_LOG(LogTemp, Display, "%s", *FString::Printf("Debug text frame CRC 0x%08x", Crc));
	TestEqual("The frame as verified", Crc, ExpectedDebugTextCrc);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
