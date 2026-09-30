#pragma once

#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "Misc/MemStack.h"

class FCanvasItem;
class FCanvasTextItem;
class FCanvasTileItem;
class UFont;
class UTexture;
class UTexture2D;

/**
 * A vertex of the canvas's primitives: pixel position (top-left origin, z = 0), a linear RGBA colour and, for a
 * textured primitive, its texture coordinates (0 to 1 across the texture, V from its top, as UE's).
 */
struct FCanvasVertex
{
	float X, Y, Z;
	float R, G, B, A;
	float U, V;
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

/** A run of the canvas's vertices of one primitive type and one texture (FCanvas::GetPrimitives). */
struct FCanvasPrimitiveRun
{
	ECanvasPrimitive Type = ECanvasPrimitive::Rectangle;
	/** The texture its vertices sample (MODULATE by their colour), null for flat colour. */
	const UTexture2D* Texture = nullptr;
	/**
	 * The texels map to pixels one to one (glyphs, unscaled and unrotated tiles): sampled nearest, so they stay
	 * sharp; bilinear otherwise.
	 */
	bool bNearest = false;
	int32 FirstVertex = 0;
	int32 NumVertices = 0;
};

/**
 * Screen-space 2D drawing for one frame (UE: FCanvas): tiles (flat or textured, rotated or not), thick lines and text
 * in a UFont. The engine makes one per frame, the HUD's widgets (UMG's FPaintContext) and the debug overlay draw into
 * it, and Flush_GameThread hands it to the renderer (IRendererModule::DrawCanvas), which draws its rectangles as the
 * GS's sprites and its triangles in one alpha-blended pass over the frame (GetPrimitives). Every item blends by its
 * colour's alpha (a textured one by the texel's alpha times it).
 *
 * Text is laid out by its font's metrics (advances and kerning pairs) and drawn a glyph a textured SPRITE, sampled
 * nearest from the font's PSMT4 pages; a text item may add a drop shadow or an outline (UE: FCanvasTextItem). Text
 * without a font is drawn in UEngine::GetSmallFont. Text is UTF-8 (Leon's TCHAR is a byte): ASCII and Latin-1.
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

	/** A filled rectangle blended by the colour's alpha (Leon: UE's DrawTile without a texture). */
	void DrawTile(float X, float Y, float SizeX, float SizeY, const FLinearColor& Color);
	/**
	 * A rectangle of Texture's texels [U, U + SizeU] x [V, V + SizeV] (0 to 1, V from the top) tinted by Color (UE:
	 * DrawTile). A null texture fills it with Color; bAlphaBlend false draws it opaque.
	 */
	void DrawTile(float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV,
		const FLinearColor& Color, const UTexture* Texture = nullptr, bool bAlphaBlend = true);
	/** A line Thickness pixels wide (UE: FCanvasLineItem), blended by the colour's alpha. */
	void DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness = 2.0f);
	/**
	 * Multiline text in Font (null: UEngine::GetSmallFont) whose every line is justified at X: its left end, centre or
	 * right end (Leon; UE: UCanvas::DrawText). Lines are the font's GetLineHeight apart. A drop shadow ShadowOffset
	 * away when ShadowColor's alpha is above 0, a one-pixel outline when OutlineColor's is (both drawn under the text).
	 */
	void DrawText(const UFont* Font, const FString& Text, float X, float Y, const FLinearColor& Color,
		ETextJustify Justify = ETextJustify::Left, const FVector2D& ShadowOffset = FVector2D(1.0f, 1.0f),
		const FLinearColor& ShadowColor = FLinearColor::Transparent,
		const FLinearColor& OutlineColor = FLinearColor::Transparent);
	/** DrawText with an 8-bit colour taken as it is (UE: FColor::ReinterpretAsLinear), not through the sRGB curve. */
	void DrawText(const UFont* Font, const FString& Text, float X, float Y, const FColor& Color,
		ETextJustify Justify = ETextJustify::Left);
	/** Text with a drop shadow one pixel down and right (UE: DrawShadowedString); returns the text's height. */
	int32 DrawShadowedString(float StartX, float StartY, const TCHAR* Text, const UFont* Font,
		const FLinearColor& Color, const FLinearColor& ShadowColor = FLinearColor::Black);
	/** A tile or a text item, with what the calls above do not set (a rotation, a scale, an outline) (UE: DrawItem). */
	void DrawItem(FCanvasItem& Item);

	/**
	 * The size of multiline text in Font (null: UEngine::GetSmallFont): the longest line's width and the font's
	 * GetLineHeight per line (UE: UCanvas::TextSize); zero without a font.
	 */
	static void MeasureText(const UFont* Font, const FString& Text, float& OutWidth, float& OutHeight);

	/** Font, or the engine's small font when it is null (Leon). */
	[[nodiscard]] static const UFont* GetFontOrDefault(const UFont* Font);

	[[nodiscard]] bool IsEmpty() const;
	/**
	 * Every item, batch by batch (higher depth sort keys first), as runs of primitives (N15): a tile, a line along an
	 * axis and every glyph of a text a rectangle; a rotated tile and a slanted line two triangles. A run holds one
	 * primitive type and one texture; the runs are in the items' order (a label's glyphs page by page).
	 */
	void GetPrimitives(TArray<FCanvasVertex>& OutVertices, TArray<FCanvasPrimitiveRun>& OutRuns) const;

	/** Draws the canvas now through the renderer (UE: Flush_GameThread); nothing without a Renderer module. */
	void Flush_GameThread();

private:
	friend class FCanvasTextItem;
	friend class FCanvasTileItem;

	struct FTileItem
	{
		float X = 0.0f;
		float Y = 0.0f;
		float SizeX = 0.0f;
		float SizeY = 0.0f;
		float U0 = 0.0f;
		float V0 = 0.0f;
		float U1 = 1.0f;
		float V1 = 1.0f;
		const UTexture2D* Texture = nullptr;
		/** Degrees clockwise on the screen, about the pivot (0: an axis-aligned rectangle). */
		float Rotation = 0.0f;
		/** The pivot, a fraction of the size from the top-left corner. */
		float PivotX = 0.0f;
		float PivotY = 0.0f;
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
		const UFont* Font = nullptr;
		float X = 0.0f;
		float Y = 0.0f;
		float Scale = 1.0f;
		ETextJustify Justify = ETextJustify::Left;
		FLinearColor Color = FLinearColor::White;
		/** A copy ShadowOffset away in ShadowColor, under the text (its alpha 0: none). */
		float ShadowOffsetX = 0.0f;
		float ShadowOffsetY = 0.0f;
		FLinearColor ShadowColor = FLinearColor::Transparent;
		/** Four copies a pixel away diagonally in OutlineColor, under the text (its alpha 0: none). */
		FLinearColor OutlineColor = FLinearColor::Transparent;
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
	/** Len characters of Text copied onto the frame's stack, null-terminated. */
	static const TCHAR* CopyText(const TCHAR* Text, int32 Len);
	/** Adds a text item for Len characters of Text with the item's other fields (nothing for no text or font). */
	void AddText(const TCHAR* Text, int32 Len, const FTextItem& Item);

	/** Gives the canvas's items back when it goes: declared first, so it goes last. */
	FMemMark MemMark;
	int32 SizeX = 0;
	int32 SizeY = 0;
	TArray<FBatch, TMemStackAllocator<>> Batches;
	TArray<int32, TInlineAllocator<8>> DepthSortKeyStack;
};
