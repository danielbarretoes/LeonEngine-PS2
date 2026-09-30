#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Font.generated.h"

class UAssetImportData;
class UTexture2D;

/**
 * One glyph of an offline font (UE: FFontCharacter): its rectangle in one of the font's texture pages and how it sits
 * on the line. StartU / StartV are the glyph's top-left texel counted from the page's top-left corner (the page's
 * texels are stored bottom row first, as every Leon texture's; the canvas turns the rows). Leon adds the glyph's
 * horizontal bearing and advance, which UE's offline fonts fold into USize.
 */
struct ENGINE_API FFontCharacter
{
	int32 StartU = 0;
	int32 StartV = 0;
	int32 USize = 0;
	int32 VSize = 0;
	/** The page in UFont::Textures (UE: TextureIndex). */
	uint8 TextureIndex = 0;
	/** Pixels from the line's top to the glyph's top (UE: VerticalOffset). */
	int32 VerticalOffset = 0;
	/** Pixels from the pen to the glyph's left edge (Leon: the left side bearing). */
	int32 HorizontalOffset = 0;
	/** Pixels the pen moves after the glyph (Leon: the advance width, rounded). */
	int32 Advance = 0;

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FFontCharacter& Character);
};

/** A pair of characters drawn closer or further apart than their advances say (Leon: the font's kerning table). */
struct ENGINE_API FFontKerningPair
{
	/** The first character's code point in the high 16 bits, the second's in the low 16 bits. */
	uint32 Pair = 0;
	/** Pixels added to the first character's advance when the second follows it. */
	int32 Amount = 0;

	[[nodiscard]] static uint32 MakePair(uint32 First, uint32 Second)
	{
		return (First << 16) | (Second & 0xffffu);
	}

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FFontKerningPair& KerningPair);
};

/**
 * A font whose glyphs were rasterized at import into texture pages (UE: UFont with EFontCacheType::Offline; Leon has
 * no runtime font cache and no composite fonts). The TrueType factory (LeonEd's UTrueTypeFontFactory) makes one font
 * per pixel height: the engine's are DejaVu Sans Condensed at 10, 14, 20 and 32 pixels (UEngine::GetTinyFont to
 * GetLargeFont).
 *
 * - Pages: PF_P4 textures of white texels whose CLUT carries the glyphs' coverage in its alpha (16 levels), at most
 *   256 x 256 each (the GS's budget); the canvas draws each glyph as one textured SPRITE, tinted by the text's colour.
 * - Characters: indexed by code point, 0 to 255 (ASCII and Latin-1, UE's not remapped offline font); a character the
 *   font has no glyph for draws as '?'.
 * - Metrics in whole pixels: Ascent (the line's top to the baseline), Descent (below it), Leading (the line gap), each
 *   glyph's advance and the kerning pairs (the TrueType font's pairs at the font's size).
 *
 * The characters and the kerning pairs are serialized after the tagged properties, as a texture's platform data is.
 */
UCLASS()
class ENGINE_API UFont : public UObject
{
	GENERATED_BODY()

public:
	UFont(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The code point drawn for a character the font has no glyph for (UE's NULLCHARACTER is 127; Leon draws '?'). */
	static constexpr uint32 NullCharacter = '?';

	/** The glyph pages (UE: Textures): subobjects of the font. */
	UPROPERTY()
	TArray<UTexture2D*> Textures;

	/** Pixels from the top of a line to the baseline (UE: Ascent). */
	UPROPERTY()
	float Ascent = 0.0f;

	/** Pixels from the baseline to the bottom of a line (UE: Descent; positive in Leon). */
	UPROPERTY()
	float Descent = 0.0f;

	/** Pixels between one line's bottom and the next one's top (UE: Leading). */
	UPROPERTY()
	float Leading = 0.0f;

	/** Pixels added between every two characters (UE: Kerning). */
	UPROPERTY()
	int32 Kerning = 0;

	/** The pixel height the font was rasterized at (UE: LegacyFontSize). */
	UPROPERTY()
	int32 LegacyFontSize = 0;

	/** The TrueType family it came from (UE: LegacyFontName). */
	UPROPERTY()
	FName LegacyFontName;

#if WITH_EDITORONLY_DATA
	/** Where the font was imported from, and its import settings (UE: AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/** The glyphs, indexed by code point (UE: Characters). */
	TArray<FFontCharacter> Characters;

	/** The kerning pairs, sorted by Pair (Leon). */
	TArray<FFontKerningPair> KerningPairs;

	/**
	 * The glyph of a code point: the font's, else NullCharacter's; null when the font has neither (UE: RemapChar and
	 * Characters[]). A glyph of zero size (a space) is still a glyph: it advances the pen.
	 */
	[[nodiscard]] const FFontCharacter* FindCharacter(uint32 CodePoint) const;

	/** The pixels between two code points besides the first one's advance, Kerning included (UE: GetCharKerning). */
	[[nodiscard]] int32 GetCharKerning(uint32 First, uint32 Second) const;

	/** A character's advance and the line's height, pixels (UE: GetCharSize). */
	void GetCharSize(uint32 CodePoint, float& OutWidth, float& OutHeight) const;

	/** The height of a line: Ascent + Descent (UE: GetMaxCharHeight). */
	[[nodiscard]] float GetMaxCharHeight() const
	{
		return Ascent + Descent;
	}

	/** The distance between two lines' tops: Ascent + Descent + Leading (Leon). */
	[[nodiscard]] float GetLineHeight() const
	{
		return Ascent + Descent + Leading;
	}

	/** The width of the longest line of Text, pixels (UE: GetStringSize). */
	[[nodiscard]] int32 GetStringSize(const TCHAR* Text) const;

	/** The height of Text's lines, pixels: GetLineHeight a line (UE: GetStringHeightSize). */
	[[nodiscard]] int32 GetStringHeightSize(const TCHAR* Text) const;

	/** The width of one line of text, Count bytes without line breaks (Leon: what GetStringSize measures lines by). */
	[[nodiscard]] int32 GetLineWidth(const TCHAR* Text, int32 Count) const;

	/**
	 * The next code point of UTF-8 text at Cursor, which moves past it (Leon: TCHAR is a byte). A byte that does not
	 * start a valid sequence is taken as its Latin-1 character, so Latin-1 text reads too (FChar::DecodeCodePoint,
	 * which the GS's debug text shares).
	 */
	[[nodiscard]] static uint32 DecodeCodePoint(const TCHAR*& Cursor, const TCHAR* End);

	void Serialize(FArchive& Ar) override;
};
