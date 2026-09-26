#include "Math/UnrealMathUtility.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"

bool FPS2RHI::DrawUnlitRectAlpha(float X0, float Y0, float X1, float Y1, float R, float G, float B, float Alpha)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady)
	{
		return false;
	}
	// GS alpha: 0x80 is 1.0.
	const uint8 A = uint8(FMath::Clamp(int32((Alpha * 128.0f) + 0.5f), 0, 0x80));

	// (Cs - Cd) * As + Cd, the drawing environment's blending; overlays ignore Z.
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	Sprite.bAlphaBlend = true;
	Leon::PS2::AppendDepthTest(Gs, false);
	Gs.FrameList.SetPrim(Sprite);
	Gs.FrameList.SetRGBAQ(Leon::PS2::UnitColor(R, G, B, A));
	Gs.FrameList.AddVertex(Leon::PS2::ScreenVertex(FMath::Min(X0, X1), FMath::Min(Y0, Y1)));
	Gs.FrameList.AddVertex(Leon::PS2::ScreenVertex(FMath::Max(X0, X1), FMath::Max(Y0, Y1)));
	Leon::PS2::AppendDepthTest(Gs, true);
	return true;
}
