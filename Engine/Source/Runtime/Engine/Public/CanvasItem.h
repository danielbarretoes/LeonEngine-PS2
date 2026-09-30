#pragma once

#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"

class FCanvas;
class UFont;
class UTexture;

/** The base of the items a canvas draws (UE: FCanvasItem): where, and in what colour. */
class ENGINE_API FCanvasItem
{
public:
	explicit FCanvasItem(const FVector2D& InPosition)
		: Position(InPosition)
	{
	}
	virtual ~FCanvasItem() = default;

	/** Draws the item into Canvas (UE: Draw). */
	virtual void Draw(FCanvas* InCanvas) = 0;

	/** UE: SetColor. */
	void SetColor(const FLinearColor& InColor)
	{
		SetColorValue = InColor;
	}

	/** The top-left corner, pixels (UE: Position). */
	FVector2D Position;
	/** The tint and alpha (UE: SetColor's value). */
	FLinearColor SetColorValue = FLinearColor::White;
};

/**
 * A tile: a rectangle of a texture's texels, or of a colour, rotated about a pivot (UE: FCanvasTileItem). The minimap
 * draws the map's overview rotated with the view this way.
 */
class ENGINE_API FCanvasTileItem : public FCanvasItem
{
public:
	/** A tile of Texture's texels UV0 to UV1 (0 to 1, V from the top) in Color (UE's textured constructor). */
	FCanvasTileItem(const FVector2D& InPosition, const UTexture* InTexture, const FVector2D& InSize,
		const FVector2D& InUV0, const FVector2D& InUV1, const FLinearColor& InColor)
		: FCanvasItem(InPosition)
		, Texture(InTexture)
		, Size(InSize)
		, UV0(InUV0)
		, UV1(InUV1)
	{
		SetColor(InColor);
	}

	/** A filled tile (UE's untextured constructor). */
	FCanvasTileItem(const FVector2D& InPosition, const FVector2D& InSize, const FLinearColor& InColor)
		: FCanvasItem(InPosition)
		, Size(InSize)
	{
		SetColor(InColor);
	}

	void Draw(FCanvas* InCanvas) override;

	/** Null for a filled tile. */
	const UTexture* Texture = nullptr;
	FVector2D Size;
	FVector2D UV0 = FVector2D(0.0f, 0.0f);
	FVector2D UV1 = FVector2D(1.0f, 1.0f);
	/** Its Yaw turns the tile clockwise on the screen, degrees (UE: Rotation). */
	FRotator Rotation = FRotator::ZeroRotator;
	/** The point it turns about, a fraction of Size from the top-left corner (UE: PivotPoint). */
	FVector2D PivotPoint = FVector2D(0.0f, 0.0f);
};

/**
 * Text in a font, with an optional drop shadow and outline (UE: FCanvasTextItem). Text without a font is drawn in
 * UEngine::GetSmallFont.
 */
class ENGINE_API FCanvasTextItem : public FCanvasItem
{
public:
	FCanvasTextItem(const FVector2D& InPosition, const FText& InText, const UFont* InFont, const FLinearColor& InColor)
		: FCanvasItem(InPosition)
		, Text(InText)
		, Font(InFont)
	{
		SetColor(InColor);
	}

	void Draw(FCanvas* InCanvas) override;

	/** A shadow Offset pixels away (UE: EnableShadow). */
	void EnableShadow(const FLinearColor& InColor, const FVector2D& InOffset = FVector2D(1.0f, 1.0f))
	{
		ShadowColor = InColor;
		ShadowOffset = InOffset;
	}
	void DisableShadow()
	{
		ShadowOffset = FVector2D::ZeroVector;
	}

	FText Text;
	const UFont* Font = nullptr;
	/** Scales the glyphs (UE: Scale; only X is used). Sharp at 1; bilinear otherwise. */
	FVector2D Scale = FVector2D(1.0f, 1.0f);
	/** Each line's X is its centre rather than its left end (UE: bCentreX). */
	bool bCentreX = false;
	/** Y is the text's vertical centre (UE: bCentreY). */
	bool bCentreY = false;
	/** Leon: each line's X is its right end (UE's text items only centre). */
	bool bRightJustify = false;
	/** Four copies a pixel away diagonally in OutlineColor under the text (UE: bOutlined, OutlineColor). */
	bool bOutlined = false;
	FLinearColor OutlineColor = FLinearColor::Black;
	/** UE: ShadowOffset (zero: no shadow) and ShadowColor. */
	FVector2D ShadowOffset = FVector2D::ZeroVector;
	FLinearColor ShadowColor = FLinearColor::Black;
};
