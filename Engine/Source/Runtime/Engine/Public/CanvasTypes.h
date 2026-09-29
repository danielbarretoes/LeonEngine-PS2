#pragma once

#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "Misc/MemStack.h"

/** A vertex of the canvas's primitives: pixel position (top-left origin, z = 0) and a linear RGBA colour. */
struct FCanvasVertex
{
	float X, Y, Z;
	float R, G, B, A;
};

/**
 * What a run of the canvas's vertices draws (Leon, Docs/PLANS/ps2-shipping.md N15): axis-aligned rectangles, two
 * vertices each (the top-left and bottom-right corners, one colour: the GS's SPRITE), or triangles, three each.
 */
enum class ECanvasPrimitive : uint8
{
	Rectangle,
	Triangle,
};

/** A run of the canvas's vertices of one primitive type (FCanvas::GetPrimitives). */
struct FCanvasPrimitiveRun
{
	ECanvasPrimitive Type = ECanvasPrimitive::Rectangle;
	int32 FirstVertex = 0;
	int32 NumVertices = 0;
};

/**
 * Screen-space 2D drawing for one frame (UE: FCanvas): tiles (filled rectangles), thick lines and text in Leon's HUD
 * bitmap font (stb_easy_font). The engine makes one per frame, the HUD's widgets (UMG's FPaintContext) and the debug
 * overlay draw into it, and Flush_GameThread hands it to the renderer (IRendererModule::DrawCanvas), which draws its
 * rectangles as the GS's sprites and its triangles in one alpha-blended pass over the frame (GetPrimitives).
 *
 * Items are batched by depth sort key (PushDepthSortKey): batches with a higher key are drawn first, behind the lower
 * ones, as in UE. Inside a batch the tiles come first, then the lines, then the texts, each in the order they were
 * drawn (so a label is never hidden under a panel drawn after it).
 *
 * The items and a copy of each text live on the frame's stack (FMemStack) under the canvas's own mark: drawing a
 * frame's HUD allocates nothing from the heap (Docs/PLANS/ps2-shipping.md N17).
 */
class ENGINE_API FCanvas
{
public:
	FCanvas(int32 InSizeX, int32 InSizeY);

	/** The render target size in pixels (UE: GetRenderTarget()->GetSizeXY()). */
	[[nodiscard]] int32 GetSizeX() const
	{
		return SizeX;
	}
	[[nodiscard]] int32 GetSizeY() const
	{
		return SizeY;
	}

	/** Items drawn from now on go to the batch of Key (UE: PushDepthSortKey / PopDepthSortKey). */
	void PushDepthSortKey(int32 Key);
	void PopDepthSortKey();
	[[nodiscard]] int32 GetDepthSortKey() const;

	/** A filled rectangle (UE: DrawTile, untextured); the colour's alpha is not used. */
	void DrawTile(float X, float Y, float SizeX, float SizeY, const FLinearColor& Color);
	/** A line Thickness pixels wide (UE: FCanvasLineItem); the colour's alpha is not used. */
	void DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness = 2.0f);
	/**
	 * Multiline text whose every line is justified at X: its left end, centre or right end (UE: FCanvasTextItem).
	 * Lines are 14 * Scale pixels apart.
	 */
	void DrawText(const FString& Text, float X, float Y, const FColor& Color, float Scale = HudFontScale,
		ETextJustify Justify = ETextJustify::Left);
	/** DrawText with a linear colour, opaque (each channel clamped and stored in 8 bits). */
	void DrawText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = HudFontScale,
		ETextJustify Justify = ETextJustify::Left);
	/** Text laid out by the font itself from X, Y (its own line spacing): the debug overlay's text blocks. */
	void DrawTextBlock(const FString& Text, float X, float Y, const FColor& Color, float Scale = HudFontScale);

	/**
	 * The size of multiline text at Scale: the longest line's width and 14 * Scale per line (UE: StrLen /
	 * TextSize).
	 */
	static void MeasureText(const FString& Text, float Scale, float& OutWidth, float& OutHeight);

	[[nodiscard]] bool IsEmpty() const;
	/**
	 * Every item, batch by batch (higher depth sort keys first), as runs of primitives (N15): a tile, a line along an
	 * axis and every quad of a text's glyphs (stb_easy_font's bars are axis aligned) a rectangle, a slanted line two
	 * triangles. The runs are in the items' order.
	 */
	void GetPrimitives(TArray<FCanvasVertex>& OutVertices, TArray<FCanvasPrimitiveRun>& OutRuns) const;

	/** Draws the canvas now through the renderer (UE: Flush_GameThread); nothing without a Renderer module. */
	void Flush_GameThread();

private:
	struct FTileItem
	{
		float X = 0.0f;
		float Y = 0.0f;
		float SizeX = 0.0f;
		float SizeY = 0.0f;
		FLinearColor Color = FLinearColor::White;
	};

	struct FLineItem
	{
		float X0 = 0.0f;
		float Y0 = 0.0f;
		float X1 = 0.0f;
		float Y1 = 0.0f;
		float Thickness = 2.0f;
		FLinearColor Color = FLinearColor::White;
	};

	struct FTextItem
	{
		/** The text's copy on the frame's stack, null-terminated. */
		const TCHAR* Text = nullptr;
		int32 Len = 0;
		float X = 0.0f;
		float Y = 0.0f;
		float Scale = HudFontScale;
		ETextJustify Justify = ETextJustify::Left;
		FColor Color;
		/** Laid out by the font as one block instead of line by line (DrawTextBlock). */
		bool bBlock = false;
	};

	struct FBatch
	{
		int32 DepthSortKey = 0;
		TArray<FTileItem, TMemStackAllocator<>> Tiles;
		TArray<FLineItem, TMemStackAllocator<>> Lines;
		TArray<FTextItem, TMemStackAllocator<>> Texts;
	};

	/** The batch of the current depth sort key. */
	FBatch& GetBatch();
	/** Text's characters copied onto the frame's stack. */
	static const TCHAR* CopyText(const FString& Text);

	/** Gives the canvas's items back when it goes: declared first, so it goes last. */
	FMemMark MemMark;
	int32 SizeX = 0;
	int32 SizeY = 0;
	TArray<FBatch, TMemStackAllocator<>> Batches;
	TArray<int32, TInlineAllocator<8>> DepthSortKeyStack;
};
