#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDebugDraw.h"
#include "GSDrawEnvironment.h"
#include "GSTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// FGSDebugDraw (Docs/PLANS/ps2-shipping.md N2, ps2-polish.md P5b): the debug text in the game's font, compiled in, and
// the rectangles every backend draws the same, recorded as GS register writes.

namespace
{
	/** Where the tests put the font: the first page of the 640 x 448 layout's texture arena. */
	constexpr uint32 FontBlock = FGSDrawEnvironment::TextureArenaFirstBlock;

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

	/** The UV writes of a list, in order. */
	[[nodiscard]] TArray<uint64> GetUVs(const FGSCommandList& List)
	{
		TArray<uint64> Out;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			if (Write.Register == EGSRegister::UV)
			{
				Out.Add(Write.Value);
			}
		}
		return Out;
	}

	/** The error screen's hint for a staged game (PS2ErrorScreen.cpp), with ShooterGame's names. */
	const char* const ErrorScreenHint =
		"The game reads its config and content from 'host:' (the ELF's folder).\n"
		"PCSX2: Settings > Advanced > Enable Host Filesystem, then boot the staged ELF, not Binaries/PS2's:\n"
		"Game/ShooterGame/Packages/PS2/ShooterGame.elf (Package.bat) or "
		"Game/ShooterGame/Saved/StagedBuilds/PS2/ShooterGame.elf (BuildCookRun "
		"-stage -pak), beside its ShooterGame/Content/Paks/ShooterGame-PS2.lpak.\n"
		"The EE log has the whole log.";
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSDebugDrawUploadTest, "System.GSCore.DebugDraw.Upload",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSDebugDrawUploadTest::RunTest(const FString& Parameters)
{
	// Two 128 x 128 PSMT4 pages (a page of the GS each) and their shared CLUT: 65 blocks, held in place (static data),
	// then TEXFLUSH.
	TestEqual("The font's blocks", FGSDebugDraw::GetFontBlocks(), 65u);
	FGSCommandList List;
	FGSDebugDraw::UploadFont(List, FontBlock);
	if (!TestEqual("The CLUT and two pages", List.GetNumImages(), 3))
	{
		return false;
	}
	bool bInPlace = true;
	for (int32 Index = 0; Index < List.GetNumImages(); ++Index)
	{
		bInPlace &= List.IsImageInPlace(Index);
	}
	TestTrue("Held in place", bInPlace);
	TestEqual("The CLUT: 16 PSMCT32 entries", List.GetImage(0).Num(), 64);
	TestEqual("A page: 128 x 128 texels of 4 bits", List.GetImage(1).Num(), 8192);
	TestTrue("A TEXFLUSH last", List.GetWrites().Last().Register == EGSRegister::TEXFLUSH);
	int32 Pages = 0;
	for (const FGSRegisterWrite& Write : List.GetWrites())
	{
		if (Write.Register == EGSRegister::BITBLTBUF)
		{
			const FGSBitBltBuf Destination = FGSBitBltBuf::Decode(Write.Value);
			Pages += Destination.DPSM == EGSPixelFormat::PSMT4 ? 1 : 0;
			TestTrue("Inside the font's blocks",
				Destination.DBP >= FontBlock && Destination.DBP < FontBlock + FGSDebugDraw::GetFontBlocks());
		}
	}
	TestEqual("Two PSMT4 pages", Pages, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSDebugDrawStringTest, "System.GSCore.DebugDraw.String",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSDebugDrawStringTest::RunTest(const FString& Parameters)
{
	const FGSDrawEnvironment Environment = MakeEnvironment();
	const FGSRGBAQ Color = FGSDebugDraw::UnitColor(1.0f, 0.5f, 0.0f);
	TestEqual("Opaque colour", uint32(Color.A), 0x80u);
	TestEqual("Red at 0xff", uint32(Color.R), 0xffu);

	FGSCommandList List;
	FGSDebugDraw::DrawString(List, Environment, FontBlock, 10.0f, 20.0f, "Hi", Color);
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
	// The 14-pixel page at the font's block, its CLUT loaded, MODULATE; nearest and clamped.
	const FGSRegisterWrite* Tex0Write = FindFirst(List, EGSRegister::TEX0_1);
	const FGSTex0 Tex0 = Tex0Write != nullptr ? FGSTex0::Decode(Tex0Write->Value) : FGSTex0();
	TestTrue("PSMT4 at the font's block",
		Tex0.PSM == EGSPixelFormat::PSMT4 && Tex0.TBP0 >= FontBlock &&
			Tex0.TBP0 < FontBlock + FGSDebugDraw::GetFontBlocks());
	TestTrue("Its CLUT loaded (CLD 1), MODULATE, RGBA",
		Tex0.CLD == 1 && Tex0.TFX == EGSTextureFunction::Modulate && Tex0.bRGBA &&
			Tex0.CPSM == EGSPixelFormat::PSMCT32);
	const FGSRegisterWrite* Tex1Write = FindFirst(List, EGSRegister::TEX1_1);
	TestTrue("Nearest", Tex1Write != nullptr && FGSTex1::Decode(Tex1Write->Value).MMAG == EGSFilter::Nearest);
	const FGSRegisterWrite* Prim = FindFirst(List, EGSRegister::PRIM);
	const FGSPrim Sprite = Prim != nullptr ? FGSPrim::Decode(Prim->Value) : FGSPrim();
	TestTrue("Textured, blended UV sprites",
		Sprite.Type == EGSPrimitive::Sprite && Sprite.bTextured && Sprite.bUseUV && Sprite.bAlphaBlend);
	// MODULATE takes 0x80 as 1.0: the colour is halved, the alpha kept.
	const FGSRegisterWrite* Tint = FindFirst(List, EGSRegister::RGBAQ);
	TestTrue("The colour for MODULATE",
		Tint != nullptr && FGSRGBAQ::Decode(Tint->Value).R == 0x80 && FGSRGBAQ::Decode(Tint->Value).G == 0x40 &&
			FGSRGBAQ::Decode(Tint->Value).A == 0x80);
	// One sprite (two vertices) a glyph; the pen starts at the rounded X.
	TestEqual("A sprite a glyph", CountWrites(List, EGSRegister::XYZ2), 4);

	// Proper case: 'a' is its own glyph, not 'A'; a space advances without a sprite; a character past Latin-1 draws as
	// '?'; an empty string records nothing.
	FGSCommandList Upper;
	FGSDebugDraw::DrawString(Upper, Environment, FontBlock, 0.0f, 0.0f, "A", Color);
	FGSCommandList Lower;
	FGSDebugDraw::DrawString(Lower, Environment, FontBlock, 0.0f, 0.0f, "a", Color);
	TestTrue("a is not A", GetUVs(Lower) != GetUVs(Upper));
	FGSCommandList Space;
	FGSDebugDraw::DrawString(Space, Environment, FontBlock, 0.0f, 0.0f, "a a", Color);
	TestEqual("A space draws nothing", CountWrites(Space, EGSRegister::XYZ2), 4);
	FGSCommandList Unknown;
	FGSDebugDraw::DrawString(Unknown, Environment, FontBlock, 0.0f, 0.0f, "\xc4\x80", Color);
	FGSCommandList Question;
	FGSDebugDraw::DrawString(Question, Environment, FontBlock, 0.0f, 0.0f, "?", Color);
	TestTrue("U+0100 draws as '?'", GetUVs(Unknown) == GetUVs(Question) && GetUVs(Unknown).Num() == 2);
	FGSCommandList Empty;
	FGSDebugDraw::DrawString(Empty, Environment, FontBlock, 0.0f, 0.0f, "", Color);
	TestEqual("An empty string records nothing", Empty.GetWrites().Num(), 0);

	// The glyph's corners half a pixel up and left of its pixels, its texels one to one.
	FGSCommandList Offset;
	FGSDebugDraw::DrawString(Offset, Environment, FontBlock, 100.4f, 50.0f, "A", Color, EGSDebugFont::Tiny);
	TArray<FGSXYZ> Corners;
	for (const FGSRegisterWrite& Write : Offset.GetWrites())
	{
		if (Write.Register == EGSRegister::XYZ2)
		{
			Corners.Add(FGSXYZ::Decode(Write.Value));
		}
	}
	const TArray<uint64> UVs = GetUVs(Offset);
	if (TestEqual("Two corners", Corners.Num(), 2) && TestEqual("Two UVs", UVs.Num(), 2))
	{
		const int32 Width = int32(Corners[1].X - Corners[0].X);
		const int32 Height = int32(Corners[1].Y - Corners[0].Y);
		const FGSUV First = FGSUV::Decode(UVs[0]);
		const FGSUV Second = FGSUV::Decode(UVs[1]);
		TestEqual("As wide as its texels", Width, int32(Second.U - First.U));
		TestEqual("As tall as its texels", Height, int32(Second.V - First.V));
		TestEqual("At half a pixel", int32(Corners[0].X & 15), 8);
		TestTrue("From the rounded pen",
			Corners[0].X >= Environment.PixelVertex(99.5f, 0.0f).X &&
				Corners[0].X <= Environment.PixelVertex(101.5f, 0.0f).X);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSDebugDrawMeasureTest, "System.GSCore.DebugDraw.Measure",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSDebugDrawMeasureTest::RunTest(const FString& Parameters)
{
	// The widths the embedded metrics give (DejaVu Sans Condensed's advances and kerning at whole pixels), as the
	// game's UFont measures them.
	TestEqual("Tiny line", FGSDebugDraw::GetLineHeight(EGSDebugFont::Tiny), 10);
	TestEqual("Small line", FGSDebugDraw::GetLineHeight(EGSDebugFont::Small), 14);
	TestEqual("'ms 12' at 10", FGSDebugDraw::MeasureString("ms 12", EGSDebugFont::Tiny), 24);
	TestEqual("'ms 12' at 14", FGSDebugDraw::MeasureString("ms 12"), 34);
	TestEqual("'Hello, World' at 10", FGSDebugDraw::MeasureString("Hello, World", EGSDebugFont::Tiny), 46);
	TestEqual("'TexturedCanvas' at 14", FGSDebugDraw::MeasureString("TexturedCanvas"), 88);
	// Kerning: A then V is a pixel closer than their advances.
	TestEqual("AV kerned", FGSDebugDraw::MeasureString("AV"),
		FGSDebugDraw::MeasureString("A") + FGSDebugDraw::MeasureString("V") - 1);
	TestEqual("A prefix", FGSDebugDraw::MeasureString("Hello, World", EGSDebugFont::Tiny, 5),
		FGSDebugDraw::MeasureString("Hello", EGSDebugFont::Tiny));
	// UTF-8: 'n' with tilde is one character; a missing one measures as '?'.
	TestEqual("UTF-8 is one character", FGSDebugDraw::MeasureString("\xc3\xb1"), FGSDebugDraw::MeasureString("n"));
	TestEqual("Missing is '?'", FGSDebugDraw::MeasureString("\xc4\x80"), FGSDebugDraw::MeasureString("?"));
	TestEqual("Empty", FGSDebugDraw::MeasureString(""), 0);
	TestEqual("Null", FGSDebugDraw::MeasureString(nullptr), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSDebugDrawLineBreakTest, "System.GSCore.DebugDraw.LineBreak",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSDebugDrawLineBreakTest::RunTest(const FString& Parameters)
{
	// The error screen's layout (PS2ErrorScreen.cpp): its hint wraps inside the 592 pixels between the margins, at
	// spaces, every character kept, and the screen's worst case (the title, eight errors of three lines, the hint)
	// fits the 448 lines.
	constexpr int32 TextWidth = 640 - (2 * 24);
	int32 Lines = 0;
	bool bInside = true;
	bool bAtSpaces = true;
	FString Rebuilt;
	for (const char* Cursor = ErrorScreenHint; *Cursor != '\0';)
	{
		const int32 Length = FGSDebugDraw::FindLineBreak(Cursor, TextWidth, EGSDebugFont::Tiny);
		// A trailing space draws nothing.
		const int32 Drawn = Length > 0 && Cursor[Length - 1] == ' ' ? Length - 1 : Length;
		bInside &= FGSDebugDraw::MeasureString(Cursor, EGSDebugFont::Tiny, Drawn) <= TextWidth;
		const char Next = Cursor[Length];
		bAtSpaces &= Next == '\0' || Next == '\n' || Cursor[Length - 1] == ' ';
		for (int32 Index = 0; Index < Length; ++Index)
		{
			Rebuilt.AppendChar(Cursor[Index]);
		}
		Cursor += Length;
		if (*Cursor == '\n')
		{
			Rebuilt.AppendChar('\n');
			++Cursor;
		}
		++Lines;
	}
	TestTrue("Every line inside the margins", bInside);
	TestTrue("Broken at spaces", bAtSpaces);
	TestEqual("Every character kept", Rebuilt, FString(ErrorScreenHint));
	TestTrue("A few lines", Lines >= 5 && Lines <= 8);
	const int32 LinePitch = FGSDebugDraw::GetLineHeight(EGSDebugFont::Tiny) + 1;
	const int32 WorstCase = 24 + FGSDebugDraw::GetLineHeight(EGSDebugFont::Small) + 14 + (8 * ((3 * LinePitch) + 4)) +
		16 + (Lines * LinePitch);
	TestTrue(*FString::Printf("The worst case fits the screen (%d pixels)", WorstCase), WorstCase <= 448 - 8);

	// A word longer than the line is cut where it no longer fits; at least one character each time; a line break ends
	// a line and is not part of it.
	const char* const Path = "host:/a/very/long/path/without/any/spaces/in/it/at/all/that/goes/on/and/on";
	const int32 Cut = FGSDebugDraw::FindLineBreak(Path, 100, EGSDebugFont::Tiny);
	TestTrue("Cut inside",
		Cut > 0 && FGSDebugDraw::MeasureString(Path, EGSDebugFont::Tiny, Cut) <= 100 &&
			FGSDebugDraw::MeasureString(Path, EGSDebugFont::Tiny, Cut + 1) > 100);
	TestEqual("At least one character", FGSDebugDraw::FindLineBreak("W", 1, EGSDebugFont::Small), 1);
	TestEqual("Up to the line break", FGSDebugDraw::FindLineBreak("ab\ncd", TextWidth, EGSDebugFont::Tiny), 2);
	TestEqual("An empty line", FGSDebugDraw::FindLineBreak("\ncd", TextWidth, EGSDebugFont::Tiny), 0);
	TestEqual("A UTF-8 character is not split", FGSDebugDraw::FindLineBreak("\xc3\xb1\xc3\xb1", 1), 2);
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
