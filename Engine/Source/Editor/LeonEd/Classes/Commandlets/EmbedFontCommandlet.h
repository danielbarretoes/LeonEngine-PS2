#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "EmbedFontCommandlet.generated.h"

/**
 * Writes the GS debug font compiled into GSCore (Leon; UE compiles its fallback fonts into the binary the same way):
 * `LeonCook -run=EmbedFont` rasterizes the engine's TrueType font (DejaVu Sans Condensed,
 * Engine/SourceArt/EngineFonts) with UTrueTypeFontFactory, the importer of the game's UFonts, at the debug font's sizes
 * (10 and 14 pixels, ASCII and Latin-1, a 128 x 128 PSMT4 page each) and writes the pages, the shared 16-level alpha
 * CLUT, their layout in the GS's local memory, the glyph metrics and the kerning pairs as C++ data
 * (`Engine/Source/Runtime/GSCore/Private/GSDebugFontData.inl`), which FGSDebugDraw draws the PS2's error screen and
 * GSConformance's labels with: no asset to load. The same font gives the same bytes; `-check` compares instead of
 * writing and fails when the checked-in file differs (System.LeonEd.Commandlets.EmbedFont.MatchesSource does the same).
 *
 * `-source=<file.ttf>` and `-output=<file.inl>` override the defaults (relative paths from the working directory).
 */
UCLASS()
class LEONED_API UEmbedFontCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UEmbedFontCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/** The engine's TrueType file and the generated source, full paths. */
	[[nodiscard]] static FString GetDefaultSourcePath();
	[[nodiscard]] static FString GetDefaultOutputPath();

	/**
	 * The generated source of a TrueType file, LF line endings; false with an error when a size does not rasterize or
	 * does not fit its page.
	 */
	static bool GenerateSource(const TArray<uint8>& TrueTypeFile, FString& OutSource, FString& OutError);
};
