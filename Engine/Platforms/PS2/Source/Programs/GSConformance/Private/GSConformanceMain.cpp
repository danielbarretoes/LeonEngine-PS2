#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSConformanceScenes.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "PS2RHI.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSConformance, Log, All);

namespace
{

	constexpr int32 ScreenWidth = 640;
	constexpr int32 ScreenHeight = 448;
	/** Each scene's pixel as Scale x Scale screen pixels, in a grid of Columns. */
	constexpr int32 Scale = 3;
	constexpr int32 Columns = 3;
	constexpr float CellWidth = float(GSConformance::FrameWidth * Scale);
	constexpr float CellHeight = float(GSConformance::FrameHeight * Scale);
	constexpr float Gap = 16.0f;
	constexpr float LabelHeight = 10.0f;
	constexpr float RowPitch = LabelHeight + CellHeight + 14.0f;

	/**
	 * Shows the scene's frame buffer (FBP 0) at (Left, Top): a sprite that samples it as a texture, nearest, one texel
	 * per Scale x Scale pixels.
	 */
	void AppendShowScene(FGSCommandList& List, EGSPixelFormat Format, float Left, float Top)
	{
		FGSTest NoDepth;
		List.SetTest(0, NoDepth);
		FGSTex0 Tex0;
		Tex0.TBP0 = 0;
		Tex0.TBW = 1;
		Tex0.PSM = Format;
		Tex0.TW = 6;
		Tex0.TH = 5;
		Tex0.TFX = EGSTextureFunction::Decal;
		List.SetTex1(0, FGSTex1());
		List.SetTex0(0, Tex0);
		FGSClamp Clamp;
		Clamp.WMS = EGSWrapMode::Clamp;
		Clamp.WMT = EGSWrapMode::Clamp;
		List.SetClamp(0, Clamp);
		FGSPrim Sprite;
		Sprite.Type = EGSPrimitive::Sprite;
		Sprite.bTextured = true;
		Sprite.bUseUV = true;
		List.SetPrim(Sprite);
		List.SetUV(FGSUV());
		List.AddVertex(FPS2RHI::ScreenVertex(Left, Top));
		FGSUV Corner;
		Corner.U = GSToFixed4(float(GSConformance::FrameWidth), 14);
		Corner.V = GSToFixed4(float(GSConformance::FrameHeight), 14);
		List.SetUV(Corner);
		List.AddVertex(FPS2RHI::ScreenVertex(Left + CellWidth, Top + CellHeight));
	}

} // namespace

// Draws every conformance scene into its 64 x 32 frame buffer at FBP 0 and copies it to the screen, frame after frame
// (the GS keeps no state between the scenes: each one sets up and clears its own). The screen is 32-bit so the scenes'
// colors are shown unquantized; the VRAM below GSConformance::LocalMemoryBytes is left to the scenes.
int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));

	if (!FPS2RHI::InitDisplay(ScreenWidth, ScreenHeight, EGSPixelFormat::PSMCT32, GSConformance::LocalMemoryBytes))
	{
		UE_LOG(LogGSConformance, Error, TEXT("GSConformance: the display did not initialize"));
		return 1;
	}
	const TArrayView<const FGSConformanceScene> Scenes = GSConformance::GetScenes();
	UE_LOG(LogGSConformance, Display, TEXT("GSConformance: drawing %d scenes"), Scenes.Num());

	const float GridLeft = -((CellWidth * Columns) + (Gap * (Columns - 1))) * 0.5f;
	const float GridTop = -float(ScreenHeight) * 0.5f + 16.0f;
	for (;;)
	{
		FPS2RHI::ClearColor(0.1f, 0.1f, 0.12f);
		for (int32 Index = 0; Index < Scenes.Num(); ++Index)
		{
			const FGSConformanceScene& Scene = Scenes[Index];
			const float Left = GridLeft + (float(Index % Columns) * (CellWidth + Gap));
			const float Top = GridTop + (float(Index / Columns) * RowPitch);

			FGSCommandList List;
			Scene.Build(List);
			// The scene's frame buffer is read back as a texture.
			List.TexFlush();
			FPS2RHI::Submit(List);
			FGSCommandList Show;
			AppendShowScene(Show, Scene.FrameFormat, Left, Top + LabelHeight);
			FPS2RHI::Submit(Show);
			FPS2RHI::DrawDebugText(Left, Top, Scene.Name, 0.9f, 0.9f, 0.8f, 0.5f);
		}
		FPS2RHI::WaitVSync();
	}
}
