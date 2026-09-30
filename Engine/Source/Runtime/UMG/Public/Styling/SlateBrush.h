#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlateBrush.generated.h"

/** How a brush is drawn (UE: ESlateBrushDrawType; Leon draws images only: no boxes, borders or rounded boxes). */
enum class ESlateBrushDrawType : uint8
{
	/** Nothing. */
	NoDrawType,
	/** The resource's texels stretched over the rectangle (a plain tint without a resource). */
	Image,
};

/**
 * What a widget paints a rectangle with (UE: FSlateBrush, SlateCore there): a texture (its ResourceObject, a
 * UTexture2D), the region of it to show and a tint; without a resource, the tint alone. Its size is what an image asks
 * for.
 */
USTRUCT()
struct UMG_API FSlateBrush
{
	GENERATED_BODY()

	FSlateBrush() = default;

	/** The size an image with this brush asks for (UE: ImageSize). */
	FVector2D ImageSize = FVector2D(32.0f, 32.0f);

	/** Multiplies the texels, or fills the rectangle without a resource; its alpha blends it (UE: TintColor). */
	FLinearColor TintColor = FLinearColor::White;

	/** UE: DrawAs. */
	ESlateBrushDrawType DrawAs = ESlateBrushDrawType::Image;

	/** The texels shown, 0 to 1 with V from the top (UE: GetUVRegion; the whole texture by default). */
	FBox2D UVRegion = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));

	/** The texture (UE: GetResourceObject / SetResourceObject); null: a plain tint. */
	[[nodiscard]] UObject* GetResourceObject() const
	{
		return ResourceObject;
	}
	void SetResourceObject(UObject* InResourceObject)
	{
		ResourceObject = InResourceObject;
	}

	/** UE: GetImageSize / SetImageSize. */
	[[nodiscard]] const FVector2D& GetImageSize() const
	{
		return ImageSize;
	}
	void SetImageSize(const FVector2D& InImageSize)
	{
		ImageSize = InImageSize;
	}

	/** UE: GetUVRegion / SetUVRegion. */
	[[nodiscard]] const FBox2D& GetUVRegion() const
	{
		return UVRegion;
	}
	void SetUVRegion(const FBox2D& InUVRegion)
	{
		UVRegion = InUVRegion;
	}

private:
	UPROPERTY()
	UObject* ResourceObject = nullptr;
};
