#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlateFontInfo.generated.h"

class UFont;

/** An outline around text's glyphs (UE: FFontOutlineSettings): Leon draws a one-pixel outline, any size above 0. */
struct FFontOutlineSettings
{
	/** 0: none (UE: OutlineSize). */
	int32 OutlineSize = 0;
	/** UE: OutlineColor. */
	FLinearColor OutlineColor = FLinearColor::Black;
};

/**
 * The font a widget's text is drawn in (UE: FSlateFontInfo, SlateCore there): a UFont (FontObject), or, without one,
 * the engine's font nearest Size (UEngine's Tiny 10, Small 14, Medium 20, Large 32 pixels), and an outline.
 */
USTRUCT()
struct UMG_API FSlateFontInfo
{
	GENERATED_BODY()

	FSlateFontInfo() = default;
	FSlateFontInfo(UObject* InFontObject, int32 InSize)
		: Size(InSize)
		, FontObject(InFontObject)
	{
	}

	/** The pixel height asked for: picks the engine's font when there is no FontObject (UE: Size, points there). */
	int32 Size = 14;

	/** UE: OutlineSettings. */
	FFontOutlineSettings OutlineSettings;

	/** UE: FontObject (a UFont). */
	[[nodiscard]] UObject* GetFontObject() const
	{
		return FontObject;
	}
	void SetFontObject(UObject* InFontObject)
	{
		FontObject = InFontObject;
	}

	/** The font to draw with: FontObject's, else the engine's for Size; null when neither is loaded (Leon). */
	[[nodiscard]] const UFont* GetFont() const;

	bool operator==(const FSlateFontInfo& Other) const
	{
		return FontObject == Other.FontObject && Size == Other.Size &&
			OutlineSettings.OutlineSize == Other.OutlineSettings.OutlineSize &&
			OutlineSettings.OutlineColor == Other.OutlineSettings.OutlineColor;
	}

private:
	UPROPERTY()
	UObject* FontObject = nullptr;
};
