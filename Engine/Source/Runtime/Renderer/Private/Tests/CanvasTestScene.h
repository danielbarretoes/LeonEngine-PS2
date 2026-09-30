#pragma once

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

#if WITH_DEV_AUTOMATION_TESTS

// The canvas the text and textured-canvas tests draw (Docs/PLANS/ps2-polish.md P5): the four engine fonts with
// Spanish text, a shadow and an outline, a textured tile one to one and scaled, and a rotated textured quad, over a
// dark backdrop.

namespace CanvasTestScene
{

	/** A 16 x 16 RGBA texture: a checker of four colours whose alpha falls along x (bottom row first). */
	inline UTexture2D* MakeTexture()
	{
		TArray<uint8> Texels;
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				const int32 Cell = ((X / 4) + (Y / 4)) % 4;
				Texels.Add(Cell == 0 ? 255 : Cell == 1 ? 40 : Cell == 2 ? 200 : 60);
				Texels.Add(Cell == 0 ? 40 : Cell == 1 ? 220 : Cell == 2 ? 200 : 60);
				Texels.Add(Cell == 0 ? 40 : Cell == 1 ? 60 : Cell == 2 ? 40 : 240);
				Texels.Add(uint8(255 - (X * 8)));
			}
		}
		UTexture2D* Texture = UTexture2D::CreateTransient(16, 16);
		(void)Texture->SetPlatformData(16, 16, PF_R8G8B8A8, Texels.GetData());
		return Texture;
	}

	/** The backdrop: the text's pixels are those that differ from it. */
	inline const FLinearColor& GetBackdrop()
	{
		static const FLinearColor Backdrop(0.05f, 0.05f, 0.08f, 1.0f);
		return Backdrop;
	}

	/** Draws the scene into Canvas (640 x 448) with Texture. */
	inline void Draw(FCanvas& Canvas, UTexture2D* Texture)
	{
		Canvas.DrawTile(0.0f, 0.0f, 640.0f, 448.0f, GetBackdrop());
		Canvas.DrawShadowedString(
			20.0f, 10.0f, "Men\xc3\xba principal", UEngine::GetLargeFont(), FLinearColor::White, FLinearColor::Black);
		FCanvasTextItem Outlined(FVector2D(20.0f, 60.0f), FText::FromString("\xc2\xbfJugar? \xc2\xa1S\xc3\xad!"),
			UEngine::GetMediumFont(), FLinearColor(1.0f, 0.8f, 0.2f));
		Outlined.bOutlined = true;
		Canvas.DrawItem(Outlined);
		Canvas.DrawText(UEngine::GetSmallFont(),
			"\xc3\x81\xc3\x89\xc3\x8d\xc3\x93\xc3\x9a \xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba \xc3\xb1\xc3\x91 "
			"\xc3\xbc 0123456789 $:/",
			20.0f, 100.0f, FLinearColor(1.0f, 1.0f, 0.0f, 0.8f));
		Canvas.DrawText(UEngine::GetTinyFont(), TEXT("The quick brown fox jumps over the lazy dog"), 20.0f, 130.0f,
			FLinearColor::White);
		Canvas.DrawText(UEngine::GetSmallFont(), TEXT("CT 3 - T 6"), 620.0f, 160.0f, FLinearColor(0.4f, 0.6f, 1.0f),
			ETextJustify::Right);
		Canvas.DrawTile(20.0f, 200.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, Texture);
		Canvas.DrawTile(60.0f, 200.0f, 64.0f, 64.0f, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, Texture);
		FCanvasTileItem Turned(FVector2D(200.0f, 200.0f), Texture, FVector2D(64.0f, 64.0f), FVector2D(0.0f, 0.0f),
			FVector2D(1.0f, 1.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.7f));
		Turned.Rotation = FRotator(0.0f, 30.0f, 0.0f);
		Turned.PivotPoint = FVector2D(0.5f, 0.5f);
		Canvas.DrawItem(Turned);
	}

} // namespace CanvasTestScene

#endif // WITH_DEV_AUTOMATION_TESTS
