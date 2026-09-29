#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "GS/GSTextureCache.h"
#include "GSEmulator/PS2TexturePreview.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPS2TexturePreviewTest, "System.Renderer.PS2Preview.UncookedTexturesAsCooked",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPS2TexturePreviewTest::RunTest(const FString& Parameters)
{
	// With the desktop's converter, an uncooked RGBA8 texture (3 colours, 20 x 10) draws as the PS2 cook makes it: a
	// 16 x 8 PSMT4 with a CLUT (one level: the mip chain stops at 8 texels on the shorter side); it converts once, and
	// uploads again from the conversion after the cache is reset.
	TArray<uint8> Rgba;
	for (int32 Index = 0; Index < 20 * 10; ++Index)
	{
		const uint8 Shade = uint8((Index % 3) * 100);
		Rgba.Append({Shade, uint8(255 - Shade), 40, 255});
	}
	UTexture2D* Texture = NewObject<UTexture2D>();
	(void)Texture->SetPlatformData(20, 10, PF_R8G8B8A8, Rgba.GetData());

	FGSTextureCache Cache;
	Cache.SetArena(280 * 32, 64);
	Cache.SetTextureConverter(&ConvertTextureAsPS2Cook);
	FGSCommandList List;
	FGSTextureBinding Binding;
	const FGSTex0& Tex0 = Binding.Tex0;
	if (!TestTrue("Bound", Cache.BindTexture(*Texture, List, Binding)))
	{
		return false;
	}
	TestEqual("PSMT4", int32(Tex0.PSM), int32(EGSPixelFormat::PSMT4));
	TestTrue("16 x 8", Tex0.TW == 4 && Tex0.TH == 3);
	TestTrue("With its CLUT, loaded at CSA 1", Tex0.CLD == 3 && Tex0.CSA == 1);
	TestEqual("One level", Binding.NumLevels, 1);
	Cache.Reset();
	TestTrue("Again after a reset", Cache.BindTexture(*Texture, List, Binding) && Tex0.PSM == EGSPixelFormat::PSMT4);

	FGSTextureCache Plain;
	Plain.SetArena(280 * 32, 64);
	TestTrue("Without a converter: PSMCT32",
		Plain.BindTexture(*Texture, List, Binding) && Tex0.PSM == EGSPixelFormat::PSMCT32);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
