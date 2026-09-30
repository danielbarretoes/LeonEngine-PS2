#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSTypes.h"

/** The sizes of FGSDebugDraw's font, named as the engine's UFonts of the same pixel heights (UEngine::GetTinyFont). */
enum class EGSDebugFont : uint8
{
	/** 10 pixels: the error screen's body, GSConformance's labels. */
	Tiny,
	/** 14 pixels: titles. */
	Small,
};

/**
 * Debug text and rectangles recorded into an FGSCommandList (Leon): the PS2's error screen and the GS conformance
 * program draw through it, so every backend shows them the same (the desktop's GS emulator too).
 *
 * The text is the game's font, DejaVu Sans Condensed, compiled in (UE's built-in fonts): `LeonCook -run=EmbedFont`
 * rasterizes it with the importer of the game's UFonts at 10 and 14 pixels into PSMT4 pages with a 16-level alpha
 * CLUT (GSDebugFontData.inl, ASCII and Latin-1, whole-pixel advances and kerning), so it draws when nothing can be
 * loaded. UploadFont puts the pages and the CLUT in the GS's local memory once; DrawString samples them there, one
 * textured SPRITE a glyph (nearest, MODULATE by the colour, alpha blended: what the canvas's glyphs are, the
 * TexturedCanvas conformance scene).
 *
 * Coordinates are pixels from the frame's top left (FGSDrawEnvironment::PixelVertex). Both draw without the depth test
 * (an overlay the scene cannot cover) and restore the environment's depth test after.
 */
class GSCORE_API FGSDebugDraw
{
public:
	/** The 64-word blocks the font takes in the GS's local memory (its pages and CLUT), from a page boundary. */
	[[nodiscard]] static uint32 GetFontBlocks();
	/**
	 * Records the font's upload to FontBlock (a multiple of 32: a page) and a TEXFLUSH: the images are held in place
	 * (static data). Once, before the first DrawString with that FontBlock; the memory must stay the font's.
	 */
	static void UploadFont(FGSCommandList& List, uint32 FontBlock);

	/**
	 * One line of UTF-8 text with its top-left corner at (X, Y), both rounded to whole pixels, laid out by the font's
	 * advances and kerning; a character the font lacks draws as '?'. Color as UnitColor makes it (0xff is full), the
	 * alpha 0x80 opaque; the glyphs' edges blend over the frame. The font must be resident at FontBlock (UploadFont).
	 */
	static void DrawString(FGSCommandList& List, const FGSDrawEnvironment& Environment, uint32 FontBlock, float X,
		float Y, const char* Text, const FGSRGBAQ& Color, EGSDebugFont Font = EGSDebugFont::Small);
	/** A filled rectangle; blended over the frame, (Cs - Cd) * As + Cd, when Color.A is below 0x80 (1.0). */
	static void DrawRect(FGSCommandList& List, const FGSDrawEnvironment& Environment, float X0, float Y0, float X1,
		float Y1, const FGSRGBAQ& Color);

	/**
	 * The width in pixels DrawString advances over Text (its first NumBytes bytes, or all of it with INDEX_NONE): the
	 * advances and the kerning, as the game's UFont::GetLineWidth measures.
	 */
	[[nodiscard]] static int32 MeasureString(
		const char* Text, EGSDebugFont Font = EGSDebugFont::Small, int32 NumBytes = INDEX_NONE);
	/**
	 * The bytes of Text's next line when it wraps at MaxWidth pixels (the error screen's paragraphs): up to its line
	 * break ('\n', not counted), else the most whole characters that fit, cut after the last space among them when a
	 * word would be split; at least one character, so a caller that skips the line break always moves on. 0 for an
	 * empty string or one starting with its line break.
	 */
	[[nodiscard]] static int32 FindLineBreak(const char* Text, int32 MaxWidth, EGSDebugFont Font = EGSDebugFont::Small);
	/** The pixels from one line's top to the next one's: the font's ascent, descent and leading. */
	[[nodiscard]] static int32 GetLineHeight(EGSDebugFont Font = EGSDebugFont::Small);

	/** A colour in [0, 1] as the GS's RGBAQ: 1.0 is 0xff for the colour and 0x80 for the alpha. */
	[[nodiscard]] static FGSRGBAQ UnitColor(float R, float G, float B, float Alpha = 1.0f);
};
