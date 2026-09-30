#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSConformanceScenes.h"
#include "GSDebugDraw.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "PS2RHI.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSConformance, Log, All);

namespace
{

	constexpr int32 ScreenWidth = 640;
	constexpr int32 ScreenHeight = 448;
	/**
	 * Each scene's pixel as Scale x Scale screen pixels, in a grid of Columns (4 x 6 cells on the screen: the labels'
	 * line of the debug font's 10 pixels over each cell, the rows touching, so 24 scenes fit the 448 lines).
	 */
	constexpr int32 Scale = 2;
	constexpr int32 Columns = 4;
	constexpr float CellWidth = float(GSConformance::FrameWidth * Scale);
	constexpr float CellHeight = float(GSConformance::FrameHeight * Scale);
	constexpr float Gap = 16.0f;
	constexpr EGSDebugFont LabelFont = EGSDebugFont::Tiny;
	constexpr float LabelHeight = 10.0f;
	constexpr float RowPitch = LabelHeight + CellHeight;
	constexpr float GridTop = 2.0f;
	static_assert(GridTop + (6.0f * RowPitch) <= float(ScreenHeight), "six rows fit the screen");

	/**
	 * Shows the scene's frame buffer (FBP 0) at (Left, Top), pixels from the screen's top left: a sprite that samples
	 * it as a texture, nearest, one texel per Scale x Scale pixels.
	 */
	void AppendShowScene(
		FGSCommandList& List, const FGSDrawEnvironment& Environment, EGSPixelFormat Format, float Left, float Top)
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
		List.AddVertex(Environment.PixelVertex(Left, Top));
		FGSUV Corner;
		Corner.U = GSToFixed4(float(GSConformance::FrameWidth), 14);
		Corner.V = GSToFixed4(float(GSConformance::FrameHeight), 14);
		List.SetUV(Corner);
		List.AddVertex(Environment.PixelVertex(Left + CellWidth, Top + CellHeight));
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
	// The labels' font above the display (the scenes keep to the VRAM below it), uploaded with the first frame.
	uint32 FontBlock = 0;
	uint32 FreeBlocks = 0;
	if (!FPS2RHI::AllocateTextureArena(FontBlock, FreeBlocks) || FreeBlocks < FGSDebugDraw::GetFontBlocks())
	{
		UE_LOG(LogGSConformance, Error, TEXT("GSConformance: no VRAM for the debug font"));
		return 1;
	}
	FGSCommandList FontUpload;
	FGSDebugDraw::UploadFont(FontUpload, FontBlock);
	FPS2RHI::Submit(FontUpload);

	const TArrayView<const FGSConformanceScene> Scenes = GSConformance::GetScenes();
	UE_LOG(LogGSConformance, Display, TEXT("GSConformance: drawing %d scenes"), Scenes.Num());

	const float GridLeft = (float(ScreenWidth) - ((CellWidth * Columns) + (Gap * (Columns - 1)))) * 0.5f;
	const FGSRGBAQ LabelColor = FGSDebugDraw::UnitColor(0.9f, 0.9f, 0.8f);
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
			const FGSDrawEnvironment Environment = FPS2RHI::GetDrawEnvironment();
			FGSCommandList Show;
			AppendShowScene(Show, Environment, Scene.FrameFormat, Left, Top + LabelHeight);
			FGSDebugDraw::DrawString(Show, Environment, FontBlock, Left, Top, Scene.Name, LabelColor, LabelFont);
			FPS2RHI::Submit(Show);
		}
		FPS2RHI::WaitVSync();
	}
}
