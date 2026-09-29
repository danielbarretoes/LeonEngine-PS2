#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GS/GSSceneRenderer.h"
#include "GS/GSTextureCache.h"
#include "GSReferenceRasterizer.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "PalettedTexture.h"
#include "Primitives.h"
#include "SceneView.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// The GS texture cache's residency (Docs/PLANS/ps2-shipping.md N13): textures by blocks with the least recently used
// evicted, the upload budget, the CLUT loads by CLD 2 to 5, and the scene renderer's draws grouped by texture.

namespace
{

	/** The texture arena after a 640 x 448 frame and its Z buffer (pages 280 on). */
	constexpr uint32 ArenaFirstBlock = 280 * 32;

	/**
	 * A texture as the PS2 cook makes it from Size x Size texels of Seed's colours (more than 16: PSMT8; Colors <= 16:
	 * PSMT4), with its mip chain.
	 */
	UTexture2D* MakeCookedTexture(int32 Size, int32 Seed, int32 Colors = 64)
	{
		TArray<uint8> Rgba;
		for (int32 Texel = 0; Texel < Size * Size; ++Texel)
		{
			const int32 Color = ((Texel * 7) + Seed) % Colors;
			Rgba.Append({uint8((Color * 37) + Seed), uint8(255 - (Color * 11)), uint8(Seed * 50), 255});
		}
		FPalettedTexture Paletted;
		if (!FPalettedTextureBuilder::Build(Rgba.GetData(), Size, Size, true, Paletted))
		{
			return nullptr;
		}
		UTexture2D* Texture = NewObject<UTexture2D>();
		(void)Texture->SetPlatformData(Paletted.SizeX, Paletted.SizeY, Paletted.Format, Paletted.Data.GetData());
		for (const TArray<uint8>& Mip : Paletted.Mips)
		{
			(void)Texture->AddMip(Mip.GetData());
		}
		return Texture;
	}

	/** The blocks a cooked Size x Size PSMT8 texture with its mips takes. */
	uint32 GetTextureBlocks(int32 Size)
	{
		FGSTextureLayout::FFootprint Footprint;
		FGSTextureLayout::GetFootprint(EGSPixelFormat::PSMT8, uint32(Size), uint32(Size),
			FPalettedTextureBuilder::GetNumMips(Size, Size), Footprint);
		return Footprint.NumBlocks;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheOversubscribedTest, "System.Renderer.GS.TextureCache.Oversubscribed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheOversubscribedTest::RunTest(const FString& Parameters)
{
	// Twelve 64 x 64 PSMT8 textures with their mips in an arena for four (3x oversubscribed), four a frame, the window
	// moving by two each frame: every bind is resident and textured, nothing the frame binds is ever evicted (the
	// arena never starts over), and the textures of earlier frames make the room, least recently used first.
	TArray<UTexture2D*> Textures;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		Textures.Add(MakeCookedTexture(64, Index));
		if (!TestNotNull("A cooked texture", Textures.Last()) ||
			!TestEqual("Its mips", Textures.Last()->GetNumMips(), 4))
		{
			return false;
		}
	}
	const uint32 Blocks = GetTextureBlocks(64);
	FGSTextureCache Cache;
	Cache.SetArena(ArenaFirstBlock, Blocks * 4);
	FGSCommandList List;
	bool bAllBound = true;
	bool bFrameKept = true;
	int32 Evictions = 0;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Cache.BeginFrame();
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			const UTexture2D* Texture = Textures[((Frame * 2) + Slot) % Textures.Num()];
			FGSTextureBinding Binding;
			bAllBound &= Cache.BindTexture(*Texture, List, Binding) && !Binding.bFlat && Binding.NumLevels == 4;
			for (int32 Earlier = 0; Earlier <= Slot; ++Earlier)
			{
				bFrameKept &= Cache.IsResident(Textures[((Frame * 2) + Earlier) % Textures.Num()]);
			}
		}
		Evictions += Cache.GetFrameCounters().Evictions;
		TestTrue(
			"Never more than two uploads a frame after the first", Frame == 0 || Cache.GetFrameCounters().Uploads <= 2);
	}
	TestTrue("Every bind resident, with its 4 levels", bAllBound);
	TestTrue("The frame's textures are never evicted (no reset)", bFrameKept);
	TestEqual("Two evictions a frame after the first", Evictions, 29 * 2);
	TestTrue("The arena full, not over", Cache.GetResidentBlocks() == Blocks * 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheEvictionOrderTest, "System.Renderer.GS.TextureCache.EvictionOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheEvictionOrderTest::RunTest(const FString& Parameters)
{
	// An arena for three: A, B and C in frames 1 to 3, A again in frame 4; D in frame 5 evicts B (the least recently
	// bound), E in frame 6 evicts C. In a frame that binds two of three residents and then two new ones, the second
	// new one finds no room it may take and draws flat, while the frame's own stay.
	TArray<UTexture2D*> Textures;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Textures.Add(MakeCookedTexture(32, Index));
	}
	FGSTextureCache Cache;
	Cache.SetArena(ArenaFirstBlock, GetTextureBlocks(32) * 3);
	FGSCommandList List;
	FGSTextureBinding Binding;
	const auto BindInFrame = [&](int32 Index)
	{
		Cache.BeginFrame();
		return Cache.BindTexture(*Textures[Index], List, Binding) && !Binding.bFlat;
	};
	TestTrue("A", BindInFrame(0));
	TestTrue("B", BindInFrame(1));
	TestTrue("C", BindInFrame(2));
	TestTrue("A again", BindInFrame(0));
	TestTrue("D", BindInFrame(3));
	TestTrue("D evicted B",
		!Cache.IsResident(Textures[1]) && Cache.IsResident(Textures[0]) && Cache.IsResident(Textures[2]));
	TestTrue("E", BindInFrame(4));
	TestTrue("E evicted C",
		!Cache.IsResident(Textures[2]) && Cache.IsResident(Textures[0]) && Cache.IsResident(Textures[3]));

	Cache.BeginFrame();
	TestTrue("A and D this frame",
		Cache.BindTexture(*Textures[0], List, Binding) && Cache.BindTexture(*Textures[3], List, Binding));
	TestTrue("B takes E's room", Cache.BindTexture(*Textures[1], List, Binding) && !Binding.bFlat);
	TestTrue("C has none it may take: flat", Cache.BindTexture(*Textures[2], List, Binding) && Binding.bFlat);
	TestTrue("A, D and B stay",
		Cache.IsResident(Textures[0]) && Cache.IsResident(Textures[3]) && Cache.IsResident(Textures[1]));
	TestEqual("One deferred", Cache.GetFrameCounters().Deferred, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheUploadBudgetTest, "System.Renderer.GS.TextureCache.UploadBudget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheUploadBudgetTest::RunTest(const FString& Parameters)
{
	// Six 64 x 64 PSMT8 textures (6.3 KB each with their mips and CLUT) under an 8 KB budget: a frame uploads one
	// whole and what else fits of the others' smallest levels and CLUTs (drawn from that level alone); the rest draw
	// flat with their average colour. Frame after frame the uploads spread until every texture is whole.
	TArray<UTexture2D*> Textures;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Textures.Add(MakeCookedTexture(64, Index));
	}
	FGSTextureCache Cache;
	Cache.SetArena(ArenaFirstBlock, 1024);
	Cache.SetUploadBudgetKB(8);
	FGSCommandList List;
	int32 Frames = 0;
	bool bWithinBudget = true;
	bool bSawTail = false;
	bool bSawFlat = false;
	for (; Frames < 20; ++Frames)
	{
		Cache.BeginFrame();
		bool bAllWhole = true;
		for (const UTexture2D* Texture : Textures)
		{
			FGSTextureBinding Binding;
			if (!Cache.BindTexture(*Texture, List, Binding))
			{
				return TestTrue("Bound", false);
			}
			bSawFlat |= Binding.bFlat;
			bSawTail |= !Binding.bFlat && Binding.NumLevels == 1 && Binding.Tex0.TW == 3;
			bAllWhole &= !Binding.bFlat && Binding.NumLevels == 4 && Binding.Tex0.TW == 6;
		}
		// The first upload of a frame always goes; the others keep it within the budget.
		bWithinBudget &= Cache.GetFrameCounters().UploadBytes <= 8 * 1024;
		if (bAllWhole)
		{
			break;
		}
	}
	TestTrue("Every frame within 8 KB", bWithinBudget);
	TestTrue("A texture over the budget draws from its smallest level", bSawTail);
	TestTrue("One with no room in the budget draws flat", bSawFlat);
	TestTrue(*FString::Printf("All whole after %d frames", Frames + 1), Frames >= 4 && Frames < 12);

	// The flat stand-in is the texels' average: a one-colour texture's colour.
	TArray<uint8> Rgba;
	for (int32 Texel = 0; Texel < 16 * 16; ++Texel)
	{
		Rgba.Append({200, 100, 50, 255});
	}
	UTexture2D* Plain = NewObject<UTexture2D>();
	(void)Plain->SetPlatformData(16, 16, PF_R8G8B8A8, Rgba.GetData());
	FGSTextureCache Budgeted;
	Budgeted.SetArena(ArenaFirstBlock, 1024);
	Budgeted.SetUploadBudgetKB(1);
	Budgeted.BeginFrame();
	FGSTextureBinding Binding;
	(void)Budgeted.BindTexture(*Textures[0], List, Binding);
	TestTrue("Over the budget: flat, its average",
		Budgeted.BindTexture(*Plain, List, Binding) && Binding.bFlat && Binding.FlatColor == FColor(200, 100, 50, 255));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheClutLoadsTest, "System.Renderer.GS.TextureCache.ClutLoads",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheClutLoadsTest::RunTest(const FString& Parameters)
{
	// TEX0 asks for CLD 4 / 5, so the GS loads a CLUT only when CBP0 / CBP1 changes: two PSMT8 textures (CBP0) and
	// two PSMT4 ones (CSA 1 with CBP1 and CSA 0 with CBP0) bound in turn load 7 times in 10 binds. A texture that
	// takes an evicted one's blocks has its CLUT at the same CBP: the cache asks for CLD 2 there, and the reference
	// draws the new texture's colours (CLD 4 would keep the old CLUT).
	UTexture2D* A = MakeCookedTexture(16, 1);
	UTexture2D* B = MakeCookedTexture(16, 2);
	UTexture2D* C = MakeCookedTexture(16, 3, 8);
	UTexture2D* D = MakeCookedTexture(16, 4, 8);
	FGSTextureCache Cache;
	Cache.SetArena(ArenaFirstBlock, 1024);
	Cache.BeginFrame();
	FGSCommandList List;
	TArray<uint8> Clds;
	TArray<uint8> Csas;
	for (UTexture2D* Texture : {A, A, B, A, C, D, C, D, A, C})
	{
		FGSTextureBinding Binding;
		if (!Cache.BindTexture(*Texture, List, Binding))
		{
			return TestTrue("Bound", false);
		}
		List.SetTex0(0, Binding.Tex0);
		Clds.Add(Binding.Tex0.CLD);
		Csas.Add(Binding.Tex0.CSA);
	}
	// A: loads (unknown buffer: CLD 2); A: kept; B: CBP0 differs; A: differs; C: CSA 1 (CLD 3, CBP1 unknown); D: CSA 0
	// (the slot not bound last: CLD 4, differs from A's CBP0); C, D: kept at their CSA; A: loads (D took CBP0); C: A's
	// 256 entries overwrote its 16, and CBP1 still names it, so CLD 3.
	TestEqual("CLUT loads", Cache.GetFrameCounters().ClutLoads, 7);
	TestTrue("CLD", Clds == TArray<uint8>({2, 4, 4, 4, 3, 4, 5, 4, 4, 3}));
	TestTrue("CSA", Csas == TArray<uint8>({0, 0, 0, 0, 1, 0, 1, 0, 0, 1}));

	// E takes A's blocks, CLUT included; A's CLUT is what CBP0 names.
	Cache.Release(A);
	UTexture2D* E = MakeCookedTexture(16, 9);
	FGSTextureBinding BindingE;
	if (!TestTrue("E bound", Cache.BindTexture(*E, List, BindingE)))
	{
		return false;
	}
	TestEqual("E's CLUT load forced (CLD 2)", int32(BindingE.Tex0.CLD), 2);
	List.SetTex1(0, FGSTex1());
	List.SetTex0(0, BindingE.Tex0);
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	Sprite.bTextured = true;
	Sprite.bUseUV = true;
	FGSDrawEnvironment Environment;
	Environment.Frame.FBW = 10;
	Environment.Width = 640;
	Environment.Height = 448;
	Environment.ZBuf.ZBP = 140;
	FGSCommandList Frame;
	Environment.Append(Frame);
	Frame.Append(List);
	Frame.SetPrim(Sprite);
	Frame.SetRGBAQ(FGSRGBAQ{0x80, 0x80, 0x80, 0x80});
	Frame.SetUV(FGSUV{GSToFixed4(0.5f, 14), GSToFixed4(0.5f, 14)});
	Frame.AddVertex(Environment.PixelVertex(0.0f, 0.0f));
	Frame.SetUV(FGSUV{GSToFixed4(1.5f, 14), GSToFixed4(0.5f, 14)});
	Frame.AddVertex(Environment.PixelVertex(1.0f, 1.0f));
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(Frame);
	const FColor Drawn = Rasterizer.ReadFrame(Environment.Frame, 640, 448)[0];
	// E's texel (0, 0) through E's palette (MODULATE by 0x80: the texel itself).
	const FTexturePlatformData& Data = E->GetPlatformData();
	const uint8* Texels = static_cast<const uint8*>(Data.Mips[0].BulkData.LockReadOnly());
	const uint8* Entry = &Texels[Texels[256 * 4] * 4];
	const FColor Expected(Entry[0], Entry[1], Entry[2]);
	Data.Mips[0].BulkData.Unlock();
	TestTrue(*FString::Printf("E's colour (%d %d %d, expected %d %d %d)", Drawn.R, Drawn.G, Drawn.B, Expected.R,
				 Expected.G, Expected.B),
		Drawn.R == Expected.R && Drawn.G == Expected.G && Drawn.B == Expected.B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererTextureSortTest, "System.Renderer.GS.Scene.DrawsGroupedByTexture",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererTextureSortTest::RunTest(const FString& Parameters)
{
	// Six cubes whose materials alternate between two textures in the scene's order: the opaque pass draws them
	// grouped by texture, so the frame writes TEX0 twice and loads two CLUTs instead of six; a second frame uploads
	// nothing and loads nothing it does not need.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	UMaterial* Materials[2] = {NewObject<UMaterial>(), NewObject<UMaterial>()};
	Materials[0]->BaseColorMap = MakeCookedTexture(64, 1);
	Materials[1]->BaseColorMap = MakeCookedTexture(64, 2);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(
			FVector(0.0f, float(Index - 3) * 60.0f, 0.0f), FRotator(0.0f, 20.0f, 0.0f));
		(void)Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->GetStaticMeshComponent()->SetMaterial(0, Materials[Index % 2]);
	}
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.0f, 0.0f, 0.0f));
	World.SendAllEndOfFrameUpdates();

	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	Camera.SetPerspective(60.0f, 640.0f / 448.0f, 10.0f, 10000.0f);
	Camera.SetMode(ECameraMode::FreeLook);
	Camera.SetEyeLocation(FVector(-400.0f, 0.0f, 50.0f));
	Camera.SetViewRotation(FRotator(-5.0f, 0.0f, 0.0f));
	FSceneViewFamily Family(FSceneViewFamily::ConstructionValues(640, 448, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);

	FGSDrawEnvironment Environment;
	Environment.Frame.FBW = 10;
	Environment.Width = 640;
	Environment.Height = 448;
	Environment.ZBuf.ZBP = 140;
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, (512 - 280) * 32);
	for (int32 Frame = 0; Frame < 2; ++Frame)
	{
		FGSCommandList List;
		Environment.Append(List);
		Renderer.Render(Family, Environment, List);
		int32 Tex0Writes = 0;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			Tex0Writes += Write.Register == EGSRegister::TEX0_1 ? 1 : 0;
		}
		const FFrameStats& Stats = Renderer.GetFrameStats();
		TestEqual("Six cubes drawn", Stats.ObjectsVisible, 6);
		TestEqual("TEX0 written once a texture", Tex0Writes, 2);
		TestEqual("Counted", Stats.Tex0Writes, 2);
		TestEqual("Two CLUT loads", Stats.ClutLoads, 2);
		TestEqual(
			Frame == 0 ? TEXT("Both uploaded") : TEXT("Nothing uploaded"), Stats.TextureUploads, Frame == 0 ? 2 : 0);
		TestTrue("Resident", Stats.TextureResidentBytes > 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
