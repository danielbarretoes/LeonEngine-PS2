#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Engine/Font.h"
#include "Factories/Factory.h"
#include "TrueTypeFontFactory.generated.h"

/** A font's glyphs rasterized into pages (UTrueTypeFontFactory::RasterizeFont), before they become assets. */
struct LEONED_API FRasterizedFont
{
	/** One page: Width x Height coverage texels (0 to 15, the CLUT's index), top row first. */
	struct FPage
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<uint8> Coverage;
	};

	TArray<FPage> Pages;
	/** Indexed by code point, 0 to 255 (UFont::Characters). */
	TArray<FFontCharacter> Characters;
	/** Sorted by Pair (UFont::KerningPairs). */
	TArray<FFontKerningPair> KerningPairs;
	float Ascent = 0.0f;
	float Descent = 0.0f;
	float Leading = 0.0f;
	/** The TrueType family name (the name table's family), for UFont::LegacyFontName. */
	FString FamilyName;
};

/**
 * Imports TrueType fonts (`.ttf`) as offline UFonts (UE: UTrueTypeFontFactory): stb_truetype rasterizes the
 * characters of UnicodeRange at Height pixels, antialiased, and packs them into PF_P4 pages of at most
 * TexturePageWidth x TexturePageMaxHeight (16 coverage levels in the CLUT's alpha, the texels white), with the font's
 * advances, bearings and kerning pairs rounded to whole pixels (Docs/ASSET_FORMATS.md, "Fonts"). The pages are the
 * font's subobjects (`Texture0` ...). The same file and settings give the same bytes (gate G5). ImportList.ini keys
 * set the options per font; the engine imports DejaVu Sans Condensed four times (10, 14, 20 and 32 pixels).
 */
UCLASS()
class LEONED_API UTrueTypeFontFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UTrueTypeFontFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The height in pixels of a line's ascent plus descent (UE: FFontImportOptionsData::Height, points there). */
	UPROPERTY()
	int32 Height = 14;

	/**
	 * The characters, as hexadecimal ranges (UE: UnicodeRange): ASCII's printable ones and Latin-1's, what Spanish
	 * needs (the acute vowels, the n with tilde, the inverted question and exclamation marks). Code points past 255
	 * are not kept (UFont indexes its characters by code point).
	 */
	UPROPERTY()
	FString UnicodeRange = TEXT("0020-007E,00A0-00FF");

	/** A page's widest and tallest (UE: TexturePageWidth, TexturePageMaxHeight): the GS's 256 at most. */
	UPROPERTY()
	int32 TexturePageWidth = 256;
	UPROPERTY()
	int32 TexturePageMaxHeight = 256;

	/** Empty texels around each glyph in its page (UE: XPadding, YPadding): scaled glyphs do not sample their
	 * neighbours. */
	UPROPERTY()
	int32 XPadding = 1;
	UPROPERTY()
	int32 YPadding = 1;

	UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled) override;

	/** Rasterizes a TrueType file with the factory's settings; false with an error (Leon). */
	bool RasterizeFont(const uint8* Buffer, int64 Size, FRasterizedFont& Out, FString& OutError) const;

	/** Parses UnicodeRange into code points, sorted and unique, 255 at most; false when it is malformed (Leon). */
	[[nodiscard]] static bool ParseUnicodeRange(const FString& Range, TArray<uint32>& OutCodePoints);

	/** Fills Font from Rasterized: its metrics, characters, kerning and pages (Leon). */
	static void FillFont(UFont& Font, const FRasterizedFont& Rasterized, int32 InHeight);

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;
};
