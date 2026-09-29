#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSTypes.h"

/**
 * Debug text and rectangles recorded into an FGSCommandList (Leon): the PS2's stats overlay and error screen and the
 * GS conformance program draw through it, so every backend shows them the same (the desktop's GS emulator too).
 *
 * Coordinates are pixels from the frame's top left (FGSDrawEnvironment::PixelVertex). Both draw without the depth test
 * (an overlay the scene cannot cover) and restore the environment's depth test after.
 */
class GSCORE_API FGSDebugDraw
{
public:
	/**
	 * 5x7 glyphs (ASCII A-Z, 0-9, "ms" and the punctuation of paths and log lines; other lowercase letters draw
	 * uppercase), one opaque sprite per run of set pixels. Scale 1 is 2-pixel cells: a 12-pixel advance, 14 pixels
	 * tall.
	 */
	static void DrawString(FGSCommandList& List, const FGSDrawEnvironment& Environment, float X, float Y,
		const char* Text, const FGSRGBAQ& Color, float Scale = 1.0f);
	/** A filled rectangle; blended over the frame, (Cs - Cd) * As + Cd, when Color.A is below 0x80 (1.0). */
	static void DrawRect(FGSCommandList& List, const FGSDrawEnvironment& Environment, float X0, float Y0, float X1,
		float Y1, const FGSRGBAQ& Color);

	/** The width and height DrawString covers, in pixels. */
	[[nodiscard]] static float GetTextWidth(const char* Text, float Scale = 1.0f);
	[[nodiscard]] static float GetTextHeight(float Scale = 1.0f);

	/** A colour in [0, 1] as the GS's RGBAQ: 1.0 is 0xff for the colour and 0x80 for the alpha. */
	[[nodiscard]] static FGSRGBAQ UnitColor(float R, float G, float B, float Alpha = 1.0f);
};
