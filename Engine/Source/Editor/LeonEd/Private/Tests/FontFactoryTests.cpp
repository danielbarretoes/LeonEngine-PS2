#include "AssetImportUtils.h"
#include "Commandlets/EmbedFontCommandlet.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Factories/TrueTypeFontFactory.h"
#include "Misc/AutomationTest.h"
#include "Tests/LeonEdTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// The TrueType importer (UTrueTypeFontFactory): DejaVu Sans Condensed, the engine's font, rasterized into PSMT4 pages
// the same way every time.

namespace
{

	/** The engine's TrueType source (Engine/SourceArt/EngineFonts). */
	TArray<uint8> ReadEngineFont()
	{
		return LeonEdTest::ReadBytes(FPaths::ConvertRelativePathToFull(
			FPaths::EngineDir() + TEXT("SourceArt/EngineFonts/DejaVuSansCondensed.ttf")));
	}

	/** Code points of UTF-8 text. */
	TArray<uint32> CodePoints(const TCHAR* Text)
	{
		TArray<uint32> Out;
		const TCHAR* End = Text + FCString::Strlen(Text);
		for (const TCHAR* Cursor = Text; Cursor < End;)
		{
			Out.Add(UFont::DecodeCodePoint(Cursor, End));
		}
		return Out;
	}

	bool SameCharacters(const TArray<FFontCharacter>& A, const TArray<FFontCharacter>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			const FFontCharacter& X = A[Index];
			const FFontCharacter& Y = B[Index];
			if (X.StartU != Y.StartU || X.StartV != Y.StartV || X.USize != Y.USize || X.VSize != Y.VSize ||
				X.TextureIndex != Y.TextureIndex || X.VerticalOffset != Y.VerticalOffset ||
				X.HorizontalOffset != Y.HorizontalOffset || X.Advance != Y.Advance)
			{
				return false;
			}
		}
		return true;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdTrueTypeRasterizeTest, "System.LeonEd.Factories.TrueTypeFont.Rasterize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdTrueTypeRasterizeTest::RunTest(const FString& Parameters)
{
	// 14 pixels: the ascent and descent add up to the height; ASCII and the Latin-1 letters Spanish needs have glyphs
	// inside non-overlapping page rectangles; 16 coverage levels; kerning pairs sorted; twice, the same result.
	const TArray<uint8> Ttf = ReadEngineFont();
	if (!TestTrue("The engine's TrueType file", Ttf.Num() > 1000))
	{
		return false;
	}
	UTrueTypeFontFactory* Factory = NewObject<UTrueTypeFontFactory>();
	Factory->Height = 14;
	FRasterizedFont First;
	FRasterizedFont Second;
	FString Error;
	if (!TestTrue("Rasterized", Factory->RasterizeFont(Ttf.GetData(), Ttf.Num(), First, Error)) ||
		!TestTrue("Again", Factory->RasterizeFont(Ttf.GetData(), Ttf.Num(), Second, Error)))
	{
		return false;
	}
	TestEqual("The family", First.FamilyName, FString(TEXT("DejaVu Sans Condensed")));
	TestEqual("Ascent + descent = the height", FMath::RoundToInt(First.Ascent + First.Descent), 14);
	TestEqual("Indexed by code point", First.Characters.Num(), 256);
	for (const uint32 CodePoint : CodePoints(TEXT(
			 "AZaz09$:!?\xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba\xc3\xb1\xc3\x91\xc2\xbf\xc2\xa1\xc3\x81\xc3\x9c")))
	{
		const FFontCharacter& Character = First.Characters[int32(CodePoint)];
		TestTrue(*FString::Printf("A glyph for U+%04X", CodePoint),
			Character.Advance > 0 && Character.USize > 0 && Character.VSize > 0);
	}
	TestTrue(
		"The space advances, drawing nothing", First.Characters[' '].Advance > 0 && First.Characters[' '].USize == 0);
	TestTrue("No glyph for a control character", First.Characters[0x07].Advance == 0);

	bool bPagesValid = First.Pages.Num() >= 1;
	for (const FRasterizedFont::FPage& Page : First.Pages)
	{
		bPagesValid &= FMath::IsPowerOfTwo(Page.Width) && FMath::IsPowerOfTwo(Page.Height) && Page.Width <= 256 &&
			Page.Height <= 256 && Page.Coverage.Num() == Page.Width * Page.Height;
		for (const uint8 Level : Page.Coverage)
		{
			bPagesValid &= Level < 16;
		}
	}
	TestTrue("Pages: powers of two up to 256, 16 levels", bPagesValid);
	// Every glyph's rectangle lies in its page and overlaps no other one on it.
	bool bPlaced = true;
	for (int32 A = 0; A < First.Characters.Num(); ++A)
	{
		const FFontCharacter& Glyph = First.Characters[A];
		if (Glyph.USize == 0)
		{
			continue;
		}
		const FRasterizedFont::FPage& Page = First.Pages[Glyph.TextureIndex];
		bPlaced &= Glyph.StartU >= 0 && Glyph.StartV >= 0 && Glyph.StartU + Glyph.USize <= Page.Width &&
			Glyph.StartV + Glyph.VSize <= Page.Height;
		for (int32 B = A + 1; B < First.Characters.Num(); ++B)
		{
			const FFontCharacter& Other = First.Characters[B];
			if (Other.USize == 0 || Other.TextureIndex != Glyph.TextureIndex)
			{
				continue;
			}
			const bool bApart = Glyph.StartU + Glyph.USize <= Other.StartU ||
				Other.StartU + Other.USize <= Glyph.StartU || Glyph.StartV + Glyph.VSize <= Other.StartV ||
				Other.StartV + Other.VSize <= Glyph.StartV;
			bPlaced &= bApart;
		}
	}
	TestTrue("Glyphs inside their page, apart", bPlaced);
	bool bSorted = First.KerningPairs.Num() > 0;
	for (int32 Index = 1; Index < First.KerningPairs.Num(); ++Index)
	{
		bSorted &= First.KerningPairs[Index - 1].Pair < First.KerningPairs[Index].Pair;
	}
	TestTrue("Kerning pairs, sorted", bSorted);

	TestTrue("Deterministic: the same characters", SameCharacters(First.Characters, Second.Characters));
	bool bSamePages = First.Pages.Num() == Second.Pages.Num();
	for (int32 Index = 0; bSamePages && Index < First.Pages.Num(); ++Index)
	{
		bSamePages = First.Pages[Index].Coverage == Second.Pages[Index].Coverage;
	}
	TestTrue("Deterministic: the same pages", bSamePages);

	// 32 pixels fill one 256 x 256 page; 64 need more than one.
	Factory->Height = 32;
	FRasterizedFont Large;
	TestTrue("32 pixels: one page",
		Factory->RasterizeFont(Ttf.GetData(), Ttf.Num(), Large, Error) && Large.Pages.Num() == 1 &&
			Large.Pages[0].Width == 256 && Large.Pages[0].Height == 256);
	Factory->Height = 64;
	FRasterizedFont Huge;
	TestTrue(
		"64 pixels: pages", Factory->RasterizeFont(Ttf.GetData(), Ttf.Num(), Huge, Error) && Huge.Pages.Num() >= 2);
	bool bOnPages = true;
	for (const FFontCharacter& Character : Huge.Characters)
	{
		bOnPages &= Character.USize == 0 || Character.TextureIndex < Huge.Pages.Num();
	}
	TestTrue("Every glyph on one of them", bOnPages);

	// Bad settings and files are refused.
	Factory->UnicodeRange = TEXT("zz-12");
	FRasterizedFont Bad;
	TestFalse("A bad range", Factory->RasterizeFont(Ttf.GetData(), Ttf.Num(), Bad, Error));
	Factory->UnicodeRange = TEXT("0020-007E");
	const uint8 NotAFont[16] = {'n', 'o', 't', ' ', 'a', ' ', 'f', 'o', 'n', 't'};
	TestFalse("Not a TrueType file", Factory->RasterizeFont(NotAFont, sizeof(NotAFont), Bad, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdTrueTypeImportTest, "System.LeonEd.Factories.TrueTypeFont.Import",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdTrueTypeImportTest::RunTest(const FString& Parameters)
{
	// A .ttf imports as a UFont with PF_P4 pages (white, the coverage in the CLUT's alpha), its metrics, kerning and
	// import data; importing it again over itself saves the same bytes (gate G5).
	LeonEdTest::FScopedTestContent Content;
	const FString File = LeonEdTest::WriteSource(TEXT("Fonts/DejaVuSansCondensed.ttf"), ReadEngineFont());
	const auto Import = [&File]()
	{
		UTrueTypeFontFactory* Factory = NewObject<UTrueTypeFontFactory>();
		TMap<FString, FString> Settings;
		Settings.Add(TEXT("Height"), TEXT("10"));
		(void)Factory->ApplyImportSettings(Settings);
		UPackage* Package = CreatePackage(*(FString(LeonEdTest::Root) + TEXT("Font_Test")));
		return Cast<UFont>(UFactory::StaticImportObject(
			nullptr, Package, FName(TEXT("Font_Test")), RF_Public | RF_Standalone, File, Factory));
	};
	UFont* Font = Import();
	if (!TestNotNull("Imported", Font))
	{
		return false;
	}
	TestEqual("The size", Font->LegacyFontSize, 10);
	TestEqual("The family", Font->LegacyFontName, FName(TEXT("DejaVu Sans Condensed")));
	TestTrue("A page", Font->Textures.Num() == 1 && Font->Textures[0] != nullptr);
	const UTexture2D* Page = Font->Textures.Num() > 0 ? Font->Textures[0] : nullptr;
	if (Page != nullptr)
	{
		TestEqual("PF_P4", static_cast<int32>(Page->GetPixelFormat()), static_cast<int32>(PF_P4));
		TestTrue("The font's subobject", Page->GetOuter() == Font);
		TestTrue("Fits the GS's budget", Page->GetSizeX() <= 256 && Page->GetSizeY() <= 256);
	}
	TestTrue("Kerning: A then V closer", Font->GetCharKerning('A', 'V') < 0);
	TestEqual("A string's width: advances and kerning", Font->GetStringSize(TEXT("AV")),
		Font->Characters['A'].Advance + Font->Characters['V'].Advance + Font->GetCharKerning('A', 'V'));
	TestTrue("Import data",
		Font->AssetImportData != nullptr &&
			Font->AssetImportData->ImportSettings.FindRef(TEXT("Height")) == FString(TEXT("10")));

	UPackage* Package = Font->GetOutermost();
	TestTrue("Saved", FAssetImportUtils::SavePackage(Package, Font));
	const FString Saved = FAssetImportUtils::GetPackageFilename(Package->GetName());
	const TArray<uint8> FirstBytes = LeonEdTest::ReadBytes(Saved);
	UFont* Again = Import();
	TestTrue("Reimported over itself", Again == Font);
	TestTrue("Saved again", FAssetImportUtils::SavePackage(Package, Font));
	const TArray<uint8> SecondBytes = LeonEdTest::ReadBytes(Saved);
	TestTrue("The same bytes", FirstBytes.Num() > 0 && FirstBytes == SecondBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdEmbedFontTest, "System.LeonEd.Commandlets.EmbedFont.MatchesSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdEmbedFontTest::RunTest(const FString& Parameters)
{
	// The GS debug font compiled into GSCore (GSDebugFontData.inl) is what LeonCook -run=EmbedFont makes of the
	// engine's TrueType file today, byte for byte (its glyphs are the game's UFonts'); -check says so too. After a
	// change to the font, the importer or the generator: run the commandlet and commit the file (Docs/TOOLS.md).
	const TArray<uint8> Ttf = ReadEngineFont();
	FString Source;
	FString Error;
	if (!TestTrue("Generated", UEmbedFontCommandlet::GenerateSource(Ttf, Source, Error)))
	{
		AddError(Error);
		return false;
	}
	FString Again;
	TestTrue("Generated again", UEmbedFontCommandlet::GenerateSource(Ttf, Again, Error));
	TestTrue("Deterministic", Source == Again);
	TestFalse("LF line endings", Source.Contains(TEXT("\r")));
	const TArray<uint8> CheckedIn = LeonEdTest::ReadBytes(UEmbedFontCommandlet::GetDefaultOutputPath());
	const bool bSame =
		CheckedIn.Num() == Source.Len() && FMemory::Memcmp(CheckedIn.GetData(), *Source, SIZE_T(CheckedIn.Num())) == 0;
	TestTrue("GSDebugFontData.inl is up to date (LeonCook -run=EmbedFont)", bSame);
	UEmbedFontCommandlet* Commandlet = NewObject<UEmbedFontCommandlet>();
	TestEqual("-check agrees", Commandlet->Main(TEXT("-check")), bSame ? 0 : 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
