#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The engine's fonts (UFont, /Engine/EngineFonts) and the canvas's text, textured tiles and rotated quads.

namespace
{

	/** The runs of a canvas and their vertices. */
	struct FCanvasPrimitives
	{
		TArray<FCanvasVertex> Vertices;
		TArray<FCanvasPrimitiveRun> Runs;

		explicit FCanvasPrimitives(const FCanvas& Canvas)
		{
			Canvas.GetPrimitives(Vertices, Runs);
		}

		/** The rectangles of runs textured by Texture (any texture when null). */
		[[nodiscard]] int32 CountRectangles(const UTexture2D* Texture = nullptr) const
		{
			int32 Count = 0;
			for (const FCanvasPrimitiveRun& Run : Runs)
			{
				if (Run.Type == ECanvasPrimitive::Rectangle && Run.Texture != nullptr &&
					(Texture == nullptr || Run.Texture == Texture))
				{
					Count += Run.NumVertices / 2;
				}
			}
			return Count;
		}
	};

	/** The visible glyphs of UTF-8 text (no spaces). */
	int32 CountGlyphs(const UFont& Font, const TCHAR* Text)
	{
		int32 Count = 0;
		const TCHAR* End = Text + FCString::Strlen(Text);
		for (const TCHAR* Cursor = Text; Cursor < End;)
		{
			const FFontCharacter* Character = Font.FindCharacter(UFont::DecodeCodePoint(Cursor, End));
			Count += Character != nullptr && Character->USize > 0 ? 1 : 0;
		}
		return Count;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFontMetricsTest, "System.Engine.Font.MetricsAndKerning",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFontMetricsTest::RunTest(const FString& Parameters)
{
	// The four engine fonts load from their packages at their sizes; a string's width is its advances plus the
	// kerning pairs; UTF-8 decodes to Latin-1 (and a stray byte reads as Latin-1); a character without a glyph is '?'.
	const UFont* Fonts[4] = {
		UEngine::GetTinyFont(), UEngine::GetSmallFont(), UEngine::GetMediumFont(), UEngine::GetLargeFont()};
	const int32 Sizes[4] = {10, 14, 20, 32};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!TestNotNull(*FString::Printf("The %d-pixel font", Sizes[Index]), Fonts[Index]))
		{
			return false;
		}
		TestEqual("Its size", Fonts[Index]->LegacyFontSize, Sizes[Index]);
		TestEqual("A line's height", FMath::RoundToInt(Fonts[Index]->GetMaxCharHeight()), Sizes[Index]);
		bool bPages = Fonts[Index]->Textures.Num() > 0;
		for (const UTexture2D* Page : Fonts[Index]->Textures)
		{
			bPages &= Page != nullptr && Page->GetPixelFormat() == PF_P4 && Page->GetSizeX() <= 256 &&
				Page->GetSizeY() <= 256;
		}
		TestTrue("PSMT4 pages within the GS's budget", bPages);
		if (bPages)
		{
			UE_LOG(LogTemp, Display, "%s",
				*FString::Printf("Font %d px: %d page(s), the first %dx%d, %d kerning pair(s)", Sizes[Index],
					Fonts[Index]->Textures.Num(), Fonts[Index]->Textures[0]->GetSizeX(),
					Fonts[Index]->Textures[0]->GetSizeY(), Fonts[Index]->KerningPairs.Num()));
		}
	}
	const UFont& Font = *Fonts[1];

	// Kerning: A and V sit closer than their advances.
	const int32 AdvanceA = Font.Characters['A'].Advance;
	const int32 AdvanceV = Font.Characters['V'].Advance;
	TestTrue("A-V kerns", Font.GetCharKerning('A', 'V') < 0);
	TestEqual("No kerning for a pair without one", Font.GetCharKerning('H', 'H'), 0);
	TestEqual("AV's width", Font.GetStringSize(TEXT("AV")), AdvanceA + AdvanceV + Font.GetCharKerning('A', 'V'));
	TestEqual("The longest line", Font.GetStringSize(TEXT("A\nAV")), Font.GetStringSize(TEXT("AV")));
	TestEqual(
		"Two lines' height", Font.GetStringHeightSize(TEXT("A\nB")), FMath::RoundToInt(2.0f * Font.GetLineHeight()));

	// UTF-8: "n with tilde" is two bytes, one character; a lone Latin-1 byte is that character.
	const TCHAR Utf8[] = "\xc3\xb1";
	const TCHAR* Cursor = Utf8;
	TestEqual("UTF-8 decodes", UFont::DecodeCodePoint(Cursor, Utf8 + 2), 0xf1u);
	TestTrue("Past both bytes", Cursor == Utf8 + 2);
	const TCHAR Latin1[] = "\xf1";
	Cursor = Latin1;
	TestEqual("A stray byte is Latin-1", UFont::DecodeCodePoint(Cursor, Latin1 + 1), 0xf1u);
	TestEqual("The same width either way", Font.GetStringSize(TEXT("\xc3\xb1")), Font.Characters[0xf1].Advance);
	// Spanish: every letter and sign has its glyph.
	for (const uint32 CodePoint : {0xe1u, 0xe9u, 0xedu, 0xf3u, 0xfau, 0xf1u, 0xd1u, 0xbfu, 0xa1u, 0xfcu})
	{
		const FFontCharacter* Character = Font.FindCharacter(CodePoint);
		TestTrue(*FString::Printf("U+%04X has a glyph", CodePoint),
			Character != nullptr && Character == &Font.Characters[int32(CodePoint)] && Character->USize > 0);
	}
	// The euro sign (U+20AC, three bytes) is past Latin-1: drawn as '?'.
	TestTrue("Past Latin-1: '?'", Font.FindCharacter(0x20ac) == &Font.Characters['?']);
	TestEqual("Measured as '?'", Font.GetStringSize(TEXT("\xe2\x82\xac")), Font.Characters['?'].Advance);
	float Width = 0.0f;
	float Height = 0.0f;
	FCanvas::MeasureText(nullptr, TEXT("Hola"), Width, Height);
	TestEqual("The canvas measures in the small font", Width, float(Font.GetStringSize(TEXT("Hola"))));
	TestEqual("A line", Height, Font.GetLineHeight());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanvasTextGlyphsTest, "System.Engine.Canvas.TextGlyphs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCanvasTextGlyphsTest::RunTest(const FString& Parameters)
{
	// Text is a textured sprite a glyph, sampled nearest from the font's page, at the pen's whole pixels: its
	// advances and kerning, each line justified; a shadow adds a copy, an outline four, under the text.
	const UFont* Font = UEngine::GetSmallFont();
	if (!TestNotNull("The small font", Font))
	{
		return false;
	}
	const TCHAR* Text = "AVA b\xc3\xb1";
	const int32 Glyphs = CountGlyphs(*Font, Text);
	TestEqual("Five glyphs (no space)", Glyphs, 5);
	{
		FCanvas Canvas(640, 448);
		Canvas.DrawText(Font, Text, 100.0f, 50.0f, FLinearColor(1.0f, 0.5f, 0.0f, 0.75f));
		const FCanvasPrimitives Primitives(Canvas);
		if (!TestTrue("One run of textured rectangles, nearest",
				Primitives.Runs.Num() == 1 && Primitives.Runs[0].Texture == Font->Textures[0] &&
					Primitives.Runs[0].bNearest && Primitives.Runs[0].Type == ECanvasPrimitive::Rectangle))
		{
			return false;
		}
		TestEqual("A sprite a glyph", Primitives.CountRectangles(), Glyphs);
		const FCanvasVertex& TopLeft = Primitives.Vertices[0];
		const FFontCharacter& A = Font->Characters['A'];
		TestEqual("The first glyph's left: pen + bearing", TopLeft.X, 100.0f + float(A.HorizontalOffset));
		TestEqual("Its top: line + offset", TopLeft.Y, 50.0f + float(A.VerticalOffset));
		TestEqual("Its U", TopLeft.U, float(A.StartU) / float(Font->Textures[0]->GetSizeX()));
		TestEqual("Its V (from the page's top)", TopLeft.V, float(A.StartV) / float(Font->Textures[0]->GetSizeY()));
		TestEqual("The colour's alpha is kept", TopLeft.A, 0.75f);
		// The second glyph, V: after A's advance and the A-V kerning.
		const FFontCharacter& V = Font->Characters['V'];
		TestEqual("Kerned", Primitives.Vertices[2].X,
			100.0f + float(A.Advance + Font->GetCharKerning('A', 'V') + V.HorizontalOffset));
	}
	{
		// Right justified at 300: the line ends there; centred at 300: its middle is there (whole pixels).
		FCanvas Canvas(640, 448);
		Canvas.DrawText(Font, TEXT("AV"), 300.0f, 10.0f, FLinearColor::White, ETextJustify::Right);
		Canvas.DrawText(Font, TEXT("AV"), 300.0f, 40.0f, FLinearColor::White, ETextJustify::Center);
		const FCanvasPrimitives Primitives(Canvas);
		const float LineWidth = float(Font->GetStringSize(TEXT("AV")));
		TestEqual("Right: the pen starts a line's width before", Primitives.Vertices[0].X,
			FMath::RoundToFloat(300.0f - LineWidth) + float(Font->Characters['A'].HorizontalOffset));
		TestEqual("Centre: half a line's width before", Primitives.Vertices[4].X,
			FMath::RoundToFloat(300.0f - (LineWidth * 0.5f)) + float(Font->Characters['A'].HorizontalOffset));
	}
	{
		// A second line is a line height down.
		FCanvas Canvas(640, 448);
		Canvas.DrawText(Font, TEXT("A\nA"), 0.0f, 0.0f, FLinearColor::White);
		const FCanvasPrimitives Primitives(Canvas);
		TestEqual("Line two", Primitives.Vertices[2].Y - Primitives.Vertices[0].Y, Font->GetLineHeight());
	}
	{
		// A shadow and an outline: 1 + 1 and 4 + 1 copies, the text last (on top).
		FCanvas Canvas(640, 448);
		Canvas.DrawShadowedString(10.0f, 10.0f, TEXT("AV"), Font, FLinearColor::White, FLinearColor::Black);
		FCanvasTextItem Outlined(FVector2D(10.0f, 40.0f), FText::FromString(TEXT("AV")), Font, FLinearColor::White);
		Outlined.bOutlined = true;
		Canvas.DrawItem(Outlined);
		const FCanvasPrimitives Primitives(Canvas);
		TestEqual("Shadow and outline copies", Primitives.CountRectangles(), (2 * 2) + (5 * 2));
		TestEqual("The shadow first, a pixel down and right", Primitives.Vertices[0].X + 0.0f,
			Primitives.Vertices[4].X + 1.0f);
		TestEqual("Black", Primitives.Vertices[0].R, 0.0f);
	}
	{
		// Scaled text is sampled bilinear.
		FCanvas Canvas(640, 448);
		FCanvasTextItem Scaled(FVector2D(10.0f, 10.0f), FText::FromString(TEXT("A")), Font, FLinearColor::White);
		Scaled.Scale = FVector2D(2.0f, 2.0f);
		Canvas.DrawItem(Scaled);
		const FCanvasPrimitives Primitives(Canvas);
		TestTrue("Bilinear when scaled", Primitives.Runs.Num() == 1 && !Primitives.Runs[0].bNearest);
		TestEqual("Twice as wide", Primitives.Vertices[1].X - Primitives.Vertices[0].X,
			2.0f * float(Font->Characters['A'].USize));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanvasTexturedTileTest, "System.Engine.Canvas.TexturedTile",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCanvasTexturedTileTest::RunTest(const FString& Parameters)
{
	// DrawTile with a texture: its UVs, colour and alpha, nearest one to one and bilinear scaled; a flat tile keeps its
	// alpha; a tile item turned 90 degrees about its centre is two triangles with its corners turned.
	UTexture2D* Texture = UTexture2D::CreateTransient(16, 8);
	if (!TestNotNull("A texture", Texture))
	{
		return false;
	}
	FCanvas Canvas(640, 448);
	Canvas.DrawTile(10.0f, 20.0f, 16.0f, 8.0f, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.5f), Texture);
	Canvas.DrawTile(40.0f, 20.0f, 32.0f, 4.0f, 0.25f, 0.5f, 0.5f, 0.5f, FLinearColor::White, Texture);
	Canvas.DrawTile(80.0f, 20.0f, 5.0f, 5.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.25f));
	FCanvasTileItem Turned(FVector2D(100.0f, 100.0f), Texture, FVector2D(20.0f, 10.0f), FVector2D(0.0f, 0.0f),
		FVector2D(1.0f, 1.0f), FLinearColor::White);
	Turned.Rotation = FRotator(0.0f, 90.0f, 0.0f);
	Turned.PivotPoint = FVector2D(0.5f, 0.5f);
	Canvas.DrawItem(Turned);
	const FCanvasPrimitives Primitives(Canvas);
	if (!TestEqual("Runs: nearest, bilinear, flat, triangles", Primitives.Runs.Num(), 4))
	{
		return false;
	}
	const FCanvasPrimitiveRun& Exact = Primitives.Runs[0];
	TestTrue("One to one: nearest", Exact.Texture == Texture && Exact.bNearest && Exact.NumVertices == 2);
	const FCanvasVertex& TopLeft = Primitives.Vertices[Exact.FirstVertex];
	const FCanvasVertex& BottomRight = Primitives.Vertices[Exact.FirstVertex + 1];
	TestTrue("Its corners and UVs",
		TopLeft.X == 10.0f && TopLeft.Y == 20.0f && BottomRight.X == 26.0f && BottomRight.Y == 28.0f &&
			TopLeft.U == 0.0f && BottomRight.V == 1.0f);
	TestEqual("Its alpha", TopLeft.A, 0.5f);
	const FCanvasPrimitiveRun& Region = Primitives.Runs[1];
	TestTrue("Scaled: bilinear", Region.Texture == Texture && !Region.bNearest);
	TestTrue("A UV region",
		Primitives.Vertices[Region.FirstVertex].U == 0.25f && Primitives.Vertices[Region.FirstVertex + 1].U == 0.75f &&
			Primitives.Vertices[Region.FirstVertex + 1].V == 1.0f);
	const FCanvasPrimitiveRun& Flat = Primitives.Runs[2];
	TestTrue(
		"A flat tile keeps its alpha", Flat.Texture == nullptr && Primitives.Vertices[Flat.FirstVertex].A == 0.25f);
	const FCanvasPrimitiveRun& Quad = Primitives.Runs[3];
	if (TestTrue("Rotated: two textured triangles, bilinear",
			Quad.Type == ECanvasPrimitive::Triangle && Quad.NumVertices == 6 && Quad.Texture == Texture &&
				!Quad.bNearest))
	{
		// Clockwise on the screen about (110, 105): the top-left corner (100, 100) goes to (115, 95).
		const FCanvasVertex& Corner = Primitives.Vertices[Quad.FirstVertex];
		TestTrue("The top-left corner turned",
			FMath::IsNearlyEqual(Corner.X, 115.0f, 1.0e-3f) && FMath::IsNearlyEqual(Corner.Y, 95.0f, 1.0e-3f) &&
				Corner.U == 0.0f && Corner.V == 0.0f);
		const FCanvasVertex& Opposite = Primitives.Vertices[Quad.FirstVertex + 2];
		TestTrue("The bottom-right corner turned",
			FMath::IsNearlyEqual(Opposite.X, 105.0f, 1.0e-3f) && FMath::IsNearlyEqual(Opposite.Y, 115.0f, 1.0e-3f) &&
				Opposite.U == 1.0f && Opposite.V == 1.0f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
