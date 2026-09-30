#include "Animation/BlendSpace1D.h"
#include "Camera/CameraComponent.h"
#include "CanvasTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GS/GSSceneRenderer.h"
#include "GS/GSTextureCache.h"
#include "GSPrimitiveEmitter.h"
#include "GSReferenceRasterizer.h"
#include "GSTexelDecoder.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "LPS2Mesh.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"
#include "SceneView.h"
#include "StaticMeshSceneProxy.h"
#include "Tests/ScopedTestWorld.h"
#include "Tests/SkinnedTestMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

// The GS scene renderer (Docs/PLANS/ps2-gs-parity.md P5): the emitter's clipping and mapping, the texture cache, and
// frames recorded by FGSSceneRenderer and drawn by the reference rasterizer, the GS's oracle.

namespace
{

	constexpr int32 FrameWidth = 640;
	constexpr int32 FrameHeight = 448;

	/** A 640 x 448 PSMCT32 frame at FBP 0, its PSMZ24 Z buffer after it, as the PS2 lays out one frame. */
	FGSDrawEnvironment MakeEnvironment()
	{
		FGSDrawEnvironment Environment;
		Environment.Frame.FBP = 0;
		Environment.Frame.FBW = FrameWidth / 64;
		Environment.Frame.PSM = EGSPixelFormat::PSMCT32;
		Environment.ZBuf.ZBP = 140;
		Environment.ZBuf.PSM = EGSPixelFormat::PSMZ24;
		Environment.Width = FrameWidth;
		Environment.Height = FrameHeight;
		return Environment;
	}

	/** The texture arena after the frame and the Z buffer (pages 280 to 511). */
	constexpr uint32 ArenaFirstBlock = 280 * 32;
	constexpr uint32 ArenaBlocks = (512 - 280) * 32;

	TArray<FColor> Rasterize(const FGSCommandList& List, const FGSDrawEnvironment& Environment)
	{
		FGSReferenceRasterizer Rasterizer;
		Rasterizer.Execute(List);
		return Rasterizer.ReadFrame(Environment.Frame, FrameWidth, FrameHeight);
	}

	const FColor& PixelAt(const TArray<FColor>& Pixels, int32 X, int32 Y)
	{
		return Pixels[(Y * FrameWidth) + X];
	}

	/** The vertices the list kicks, in order. */
	TArray<FGSXYZ> KickedVertices(const FGSCommandList& List)
	{
		TArray<FGSXYZ> Vertices;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			if (Write.Register == EGSRegister::XYZ2)
			{
				Vertices.Add(FGSXYZ::Decode(Write.Value));
			}
		}
		return Vertices;
	}

	FGSClipVertex ClipVertex(float X, float Y, float Z = 0.5f, float W = 1.0f)
	{
		FGSClipVertex Vertex;
		Vertex.Clip = FVector4(X, Y, Z, W);
		return Vertex;
	}

	/** A camera at (-Distance, 0, 0) looking down +X, 60 degrees vertically, near 10 cm. */
	UCameraComponent& MakeCamera(float Distance)
	{
		UCameraComponent& Camera = *NewObject<UCameraComponent>();
		Camera.SetPerspective(60.0f, float(FrameWidth) / float(FrameHeight), 10.0f, 10000.0f);
		Camera.SetMode(ECameraMode::Orbit);
		Camera.SetTarget(FVector::ZeroVector);
		Camera.SetViewRotation(FRotator(0.0f, 0.0f, 0.0f));
		Camera.SetDistance(Distance);
		return Camera;
	}

	/** Writes a palette (RGBA8 entries) at the start of a paletted texture's data as the cook does: the GS's CLUT. */
	void MakeClutPrefix(const TArray<uint32>& Palette, TArray<uint8>& Data)
	{
		TArray<uint8> Clut;
		uint16 Width = 0;
		uint16 Height = 0;
		FGSTextureLayout::MakeClutImage(Palette, Clut, Width, Height);
		FMemory::Memcpy(Data.GetData(), Clut.GetData(), SIZE_T(Clut.Num()));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmitterMappingTest, "System.Renderer.GS.Emitter.MappingAndCulling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSEmitterMappingTest::RunTest(const FString& Parameters)
{
	// NDC (0, 0) is the frame's center, on the pixel corner OpenGL samples around (GS pixel centers are integers, so
	// half a pixel less); depth 0 (the near plane) is the largest Z. Counter-clockwise triangles (Y up) are front
	// faces; a triangle wholly outside is rejected.
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList List;
	FGSPrimitiveEmitter Emitter(Environment, List);
	Emitter.BeginTriangles(false, false, true);
	Emitter.AddTriangle(ClipVertex(0.0f, 0.0f, 0.0f), ClipVertex(0.5f, 0.0f), ClipVertex(0.0f, 0.5f));
	const TArray<FGSXYZ> Front = KickedVertices(List);
	TestEqual("Three vertices", Front.Num(), 3);
	if (Front.Num() != 3)
	{
		return false;
	}
	TestEqual("Center X", int32(Front[0].X), int32(GSToFixed4(Environment.PrimitiveX(319.5f), 16)));
	TestEqual("Center Y", int32(Front[0].Y), int32(GSToFixed4(Environment.PrimitiveY(223.5f), 16)));
	TestEqual("Near plane: the largest Z", Front[0].Z, FGSDrawEnvironment::MaxDepth24);
	TestTrue("NDC +Y is up on screen", Front[2].Y < Front[0].Y);

	Emitter.AddTriangle(ClipVertex(0.0f, 0.0f), ClipVertex(0.0f, 0.5f), ClipVertex(0.5f, 0.0f));
	TestEqual("A clockwise triangle is culled", KickedVertices(List).Num(), 3);
	Emitter.AddTriangle(ClipVertex(2.0f, 0.0f), ClipVertex(3.0f, 0.0f), ClipVertex(2.0f, 0.5f));
	TestEqual("Outside the view: rejected", KickedVertices(List).Num(), 3);
	TestEqual("Two rejected", Emitter.GetNumRejected(), 2);

	// One vertex behind the near plane: the triangle becomes a quad (two triangles) with no vertex behind it.
	FGSCommandList Clipped;
	FGSPrimitiveEmitter NearEmitter(Environment, Clipped);
	NearEmitter.BeginTriangles(false, false, false);
	NearEmitter.AddTriangle(
		ClipVertex(-0.5f, -0.5f, 0.5f), ClipVertex(0.5f, -0.5f, 0.5f), ClipVertex(0.0f, 0.5f, -0.5f));
	TestEqual("Clipped into two triangles", NearEmitter.GetNumTriangles(), 2);
	bool bAllInFront = true;
	for (const FGSXYZ& Vertex : KickedVertices(Clipped))
	{
		bAllInFront &= Vertex.Z <= FGSDrawEnvironment::MaxDepth24;
	}
	TestTrue("Z in range", bAllInFront);

	// Far off to the side but in front: clipped to the guard band, within the GS's primitive space.
	FGSCommandList Guard;
	FGSPrimitiveEmitter GuardEmitter(Environment, Guard);
	GuardEmitter.BeginTriangles(false, false, false);
	GuardEmitter.AddTriangle(ClipVertex(-0.5f, -0.5f), ClipVertex(100.0f, -0.5f), ClipVertex(-0.5f, 0.5f));
	bool bInSpace = KickedVertices(Guard).Num() >= 3;
	for (const FGSXYZ& Vertex : KickedVertices(Guard))
	{
		bInSpace &= Vertex.X < GSToFixed4(4095.0f, 16);
	}
	TestTrue("Guard band keeps X in 0..4095", bInSpace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheTest, "System.Renderer.GS.TextureCache",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheTest::RunTest(const FString& Parameters)
{
	// A texture uploads the first time it is bound (a power of two at least 8 texels a side) and not again. It takes
	// the blocks the GS's layout gives it (an 8 x 8 PSMCT32 one block, from any block; a 64 x 64 two pages, from a
	// page), the first that are free; when none are, the least recently bound textures of earlier frames go, oldest
	// first, until they are (ps2-shipping N13), and a frame's own textures stay.
	FGSTextureCache Cache;
	FGSCommandList List;
	FGSTex0 Tex0;
	TArray<uint8> Texels;
	Texels.Init(200, 5 * 3 * 4);
	TArray<uint8> Large;
	Large.Init(100, 64 * 64 * 4);
	int32 KeyA = 0;
	int32 KeyB = 0;
	int32 KeyC = 0;
	int32 KeyD = 0;
	int32 KeyE = 0;
	TestFalse("No arena, no texture", Cache.BindTexels(&KeyA, 5, 3, Texels, false, List, Tex0));
	Cache.SetArena(ArenaFirstBlock, 128);
	TestTrue("Bound", Cache.BindTexels(&KeyA, 5, 3, Texels, false, List, Tex0));
	TestEqual("Uploaded once", Cache.GetFrameCounters().Uploads, 1);
	TestEqual("8 texels wide", int32(Tex0.TW), 3);
	TestEqual("8 texels high", int32(Tex0.TH), 3);
	TestEqual("At the arena", int32(Tex0.TBP0), int32(ArenaFirstBlock));
	TestTrue("Bound again", Cache.BindTexels(&KeyA, 5, 3, Texels, false, List, Tex0));
	TestEqual("Not uploaded again", Cache.GetFrameCounters().Uploads, 1);
	TestTrue("A second one", Cache.BindTexels(&KeyB, 5, 3, Texels, false, List, Tex0));
	TestEqual("The next block", int32(Tex0.TBP0), int32(ArenaFirstBlock + 1));
	TestTrue("A texture of two pages", Cache.BindTexels(&KeyD, 64, 64, Large, false, List, Tex0));
	TestEqual("On the next page", int32(Tex0.TBP0), int32(ArenaFirstBlock + 32));
	TestTrue("A third small one", Cache.BindTexels(&KeyC, 5, 3, Texels, false, List, Tex0));
	TestEqual("In the first free block", int32(Tex0.TBP0), int32(ArenaFirstBlock + 2));
	TestFalse("Two more pages do not fit while the frame draws the others",
		Cache.BindTexels(&KeyE, 64, 64, Large, false, List, Tex0));
	TestEqual("Nothing evicted", Cache.GetFrameCounters().Evictions, 0);
	Cache.BeginFrame();
	TestTrue("Next frame: bound C only", Cache.BindTexels(&KeyC, 5, 3, Texels, false, List, Tex0));
	TestTrue("Two more pages evict the oldest", Cache.BindTexels(&KeyE, 64, 64, Large, false, List, Tex0));
	TestEqual("A, B and D evicted (A and B did not make room)", Cache.GetFrameCounters().Evictions, 3);
	TestEqual("In D's pages", int32(Tex0.TBP0), int32(ArenaFirstBlock + 32));
	TestTrue("C stays", Cache.IsResident(&KeyC) && !Cache.IsResident(&KeyA) && !Cache.IsResident(&KeyD));
	TestEqual("Two resident", Cache.GetNumResident(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCachePalettedTest, "System.Renderer.GS.TextureCache.Paletted",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCachePalettedTest::RunTest(const FString& Parameters)
{
	// The PS2 cook's formats upload as they are: the indices and a CLUT at the arena's end, which TEX0 loads. The GS
	// reads every texel back as its palette colour (alpha to the GS's 0..0x80), and TCC takes the palette's alpha: a
	// cut-out entry (alpha 0) and a half-transparent one reach the blend.
	for (const EPixelFormat Format : {PF_P8, PF_P4})
	{
		const bool bIndex4 = Format == PF_P4;
		constexpr int32 Size = 16;
		const int32 PaletteSize = GetPixelFormatPaletteSize(Format);
		TArray<uint8> Data;
		Data.SetNumZeroed(int32(GetPixelFormatDataSize(Format, Size, Size)));
		// The cooked layout (N23): the CLUT image as the GS reads it, then the indices.
		TArray<uint32> Palette;
		for (int32 Entry = 0; Entry < PaletteSize; ++Entry)
		{
			const uint32 Alpha = Entry == 1 ? 0 : (Entry == 2 ? 128 : 255);
			Palette.Add(uint32(uint8(Entry)) | (uint32(uint8(255 - Entry)) << 8) | (uint32(uint8(Entry * 5)) << 16) |
				(Alpha << 24));
		}
		MakeClutPrefix(Palette, Data);
		const auto AlphaOf = [](int32 Entry) { return uint8(Entry == 1 ? 0 : (Entry == 2 ? 0x40 : 0x80)); };
		const auto IndexOf = [PaletteSize](int32 X, int32 Y) { return ((X * 3) + (Y * 7)) % PaletteSize; };
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Texel = (Y * Size) + X;
				uint8& Byte = Data[(PaletteSize * 4) + (bIndex4 ? Texel / 2 : Texel)];
				Byte |= uint8(bIndex4 && (Texel % 2) != 0 ? IndexOf(X, Y) << 4 : IndexOf(X, Y));
			}
		}
		UTexture2D* Texture = NewObject<UTexture2D>();
		if (!TestTrue("A paletted texture", Texture->SetPlatformData(Size, Size, Format, Data.GetData())))
		{
			return false;
		}

		FGSTextureCache Cache;
		Cache.SetArena(ArenaFirstBlock, ArenaBlocks);
		FGSCommandList List;
		FGSTextureBinding Binding;
		if (!TestTrue("Bound", Cache.BindTexture(*Texture, List, Binding)))
		{
			return false;
		}
		const FGSTex0& Tex0 = Binding.Tex0;
		TestEqual("Its format", int32(Tex0.PSM), int32(bIndex4 ? EGSPixelFormat::PSMT4 : EGSPixelFormat::PSMT8));
		// The CLUT follows the texels and loads (CLD 2 / 3: the buffer's CLUT is not known yet), a PSMT8 one with CBP0
		// at CSA 0, a PSMT4 one with CBP1 at CSA 1.
		TestTrue("The CLUT after the texels, loaded",
			Tex0.CBP > Tex0.TBP0 && Tex0.CLD == (bIndex4 ? 3 : 2) && Tex0.CSA == (bIndex4 ? 1 : 0));
		TestTrue("TCC: the texels' alpha counts", Tex0.bRGBA);
		// Load-in-place (N23): the level and the CLUT are uploaded from the texture's own data, not copied: the level
		// after the CLUT image at the start of the 128-byte aligned blob.
		const FByteBulkData& Blob = Texture->GetPlatformData().Mips[0].BulkData;
		const uint8* BlobData = static_cast<const uint8*>(Blob.LockReadOnly());
		Blob.Unlock();
		TestTrue("The level and the CLUT in place",
			List.GetNumImages() == 2 && List.IsImageInPlace(0) && List.IsImageInPlace(1) &&
				List.GetImage(0).GetData() == BlobData + (PaletteSize * 4) && List.GetImage(1).GetData() == BlobData &&
				(UPTRINT(BlobData) & 127) == 0);
		FGSReferenceRasterizer Rasterizer;
		Rasterizer.Execute(List);
		FGSClutBuffer Clut;
		Clut.Load(Rasterizer.GetMemory(), Tex0);
		bool bMatches = true;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const FColor Color = FGSTexelDecoder::Decode(
					Rasterizer.GetMemory(), Tex0, Tex0.TBP0, Tex0.TBW, FGSTexA(), Clut, uint32(X), uint32(Y));
				const int32 Entry = IndexOf(X, Y);
				bMatches &= Color.R == uint8(Entry) && Color.G == uint8(255 - Entry) && Color.B == uint8(Entry * 5) &&
					Color.A == AlphaOf(Entry);
			}
		}
		TestTrue(bIndex4 ? TEXT("PSMT4 texels") : TEXT("PSMT8 texels"), bMatches);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheTwoPalettesTest, "System.Renderer.GS.TextureCache.TwoPalettes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheTwoPalettesTest::RunTest(const FString& Parameters)
{
	// Two PSMT8 textures bound one after the other: each takes its block of texels and, after it, its CLUT's 4 blocks
	// (what a 16 x 16 PSMCT32 CLUT takes on the GS). Both palettes must read back whole (the linear memory model let
	// the second CLUT overwrite the first).
	constexpr int32 Size = 16;
	const int32 PaletteSize = GetPixelFormatPaletteSize(PF_P8);
	TArray<UTexture2D*> Textures;
	for (int32 Palette = 0; Palette < 2; ++Palette)
	{
		TArray<uint8> Data;
		Data.SetNumZeroed(int32(GetPixelFormatDataSize(PF_P8, Size, Size)));
		TArray<uint32> Colors;
		for (int32 Entry = 0; Entry < PaletteSize; ++Entry)
		{
			Colors.Add(uint32(uint8(Palette == 0 ? Entry : 0x20)) | (uint32(uint8(Palette == 0 ? 0x40 : Entry)) << 8) |
				(uint32(uint8(Palette == 0 ? 255 - Entry : 0x60)) << 16) | (255u << 24));
		}
		MakeClutPrefix(Colors, Data);
		for (int32 Texel = 0; Texel < Size * Size; ++Texel)
		{
			Data[(PaletteSize * 4) + Texel] = uint8(Texel);
		}
		UTexture2D* Texture = NewObject<UTexture2D>();
		if (!TestTrue("A PSMT8 texture", Texture->SetPlatformData(Size, Size, PF_P8, Data.GetData())))
		{
			return false;
		}
		Textures.Add(Texture);
	}

	FGSTextureCache Cache;
	Cache.SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList List;
	FGSTextureBinding FirstBinding;
	FGSTextureBinding SecondBinding;
	const bool bBound =
		Cache.BindTexture(*Textures[0], List, FirstBinding) && Cache.BindTexture(*Textures[1], List, SecondBinding);
	if (!TestTrue("Both bound", bBound))
	{
		return false;
	}
	const FGSTex0& First = FirstBinding.Tex0;
	const FGSTex0& Second = SecondBinding.Tex0;
	// Each texture's run: its block of texels, then its CLUT's 4 blocks.
	TestEqual("The first CLUT after its texels", int32(First.CBP) - int32(First.TBP0), 1);
	TestEqual("The second texture after the first CLUT", int32(Second.TBP0) - int32(First.CBP), 4);
	TestEqual("The CLUTs 5 blocks apart", int32(Second.CBP) - int32(First.CBP), 5);
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	for (int32 Palette = 0; Palette < 2; ++Palette)
	{
		const FGSTex0& Tex0 = Palette == 0 ? First : Second;
		FGSClutBuffer Clut;
		Clut.Load(Rasterizer.GetMemory(), Tex0);
		bool bMatches = true;
		for (int32 Texel = 0; Texel < Size * Size; ++Texel)
		{
			const FColor Color = FGSTexelDecoder::Decode(Rasterizer.GetMemory(), Tex0, Tex0.TBP0, Tex0.TBW, FGSTexA(),
				Clut, uint32(Texel % Size), uint32(Texel / Size));
			const FColor Expected = Palette == 0 ? FColor(uint8(Texel), 0x40, uint8(255 - Texel), 0x80)
												 : FColor(0x20, uint8(Texel), 0x60, 0x80);
			bMatches &= Color == Expected;
		}
		TestTrue(Palette == 0 ? TEXT("The first palette intact") : TEXT("The second palette intact"), bMatches);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererLitCubeTest, "System.Renderer.GS.Scene.LitCube",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererLitCubeTest::RunTest(const FString& Parameters)
{
	// A Movable cube at the origin seen from 3 m down +X, lit head-on by a white sun travelling +X: its face is the
	// albedo times the environment's light (the default world settings' sky) + N.L with an untextured material, the
	// corners are the clear colour, and the face wrote its depth.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	(void)Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
	UMaterial* Flat = NewObject<UMaterial>();
	Flat->BaseColor = FLinearColor(0.5f, 0.25f, 0.75f, 1.0f);
	Actor->GetStaticMeshComponent()->SetMaterial(0, Flat);
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));

	UCameraComponent& Camera = MakeCamera(300.0f);
	FSceneViewFamily Family(
		FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);

	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList List;
	Environment.Append(List);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	Renderer.Render(Family, Environment, List);
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const TArray<FColor> Pixels = Rasterizer.ReadFrame(Environment.Frame, FrameWidth, FrameHeight);

	const FStaticMeshSceneProxy* Proxy =
		static_cast<const FStaticMeshSceneProxy*>(Actor->GetStaticMeshComponent()->SceneProxy);
	if (!TestNotNull("A proxy", Proxy))
	{
		return false;
	}
	const FVector Albedo = Proxy->GetSectionMaterial(0).Albedo;
	const FVector Light = FLightmassWorldInfoSettings().GetEnvironmentLight() + FVector(1.0f, 1.0f, 1.0f);
	const auto Expected = [](float Value) { return FMath::Clamp(FMath::RoundToInt(Value * 255.0f), 0, 255); };
	const FColor& Center = PixelAt(Pixels, FrameWidth / 2, FrameHeight / 2);

	TestTrue("Lit face, red", FMath::Abs(int32(Center.R) - Expected(Albedo.X * Light.X)) <= 1);
	TestTrue("Lit face, green", FMath::Abs(int32(Center.G) - Expected(Albedo.Y * Light.Y)) <= 1);
	TestTrue("Lit face, blue", FMath::Abs(int32(Center.B) - Expected(Albedo.Z * Light.Z)) <= 1);
	const FColor& Corner = PixelAt(Pixels, 2, 2);
	TestEqual("Clear colour", int32(Corner.R), FMath::RoundToInt(FGSSceneRenderer::ClearColor.R * 255.0f));
	TestTrue("The face wrote its depth",
		Rasterizer.ReadZ(Environment.ZBuf, FrameWidth, FrameWidth / 2, FrameHeight / 2) > 0);
	TestEqual("Background Z", Rasterizer.ReadZ(Environment.ZBuf, FrameWidth, 2, 2), 0u);
	TestEqual("One object", Renderer.GetFrameStats().ObjectsVisible, 1);
	TestTrue("The near faces only (back faces culled)", Renderer.GetFrameStats().TrianglesSubmitted <= 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererStaticLightingTest, "System.Renderer.GS.Scene.StaticLighting",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererStaticLightingTest::RunTest(const FString& Parameters)
{
	// N22: a Static cube draws its baked vertex colours times the albedo, whatever the lights (a sun shines on it
	// head-on); colours baked for another build of its mesh are ignored (the mesh's own colours: the albedo as it is);
	// a Movable cube is lit per frame instead.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	UStaticMeshComponent& Component = *Actor->GetStaticMeshComponent();
	(void)Component.SetStaticMesh(Cube);
	UMaterial* Flat = NewObject<UMaterial>();
	Flat->BaseColor = FLinearColor(0.5f, 0.25f, 0.75f, 1.0f);
	Component.SetMaterial(0, Flat);
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	const FLPS2Mesh& Mesh = Cube->GetLODResources().RenderData;
	const auto Bake = [&Component, &Mesh](uint32 MeshCrc)
	{
		FLPS2ColorStreams Colors;
		Colors.Init(Mesh);
		Colors.MeshCrc = MeshCrc;
		for (int32 Offset = 0; Offset + 3 < Colors.Data.Num(); Offset += 4)
		{
			Colors.Data[Offset] = 64;
			Colors.Data[Offset + 1] = 128;
			Colors.Data[Offset + 2] = 255;
		}
		Component.SetBakedVertexColors(MoveTemp(Colors));
	};
	const auto CenterPixel = [&World]()
	{
		UCameraComponent& Camera = MakeCamera(300.0f);
		FSceneViewFamily Family(
			FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
		const FSceneView View(FSceneView::FromCamera(Family, Camera));
		Family.Views.Add(&View);
		const FGSDrawEnvironment Environment = MakeEnvironment();
		FGSCommandList List;
		Environment.Append(List);
		FGSSceneRenderer Renderer;
		Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
		Renderer.Render(Family, Environment, List);
		return PixelAt(Rasterize(List, Environment), FrameWidth / 2, FrameHeight / 2);
	};
	const auto Near = [](int32 Value, float Expected)
	{ return FMath::Abs(Value - FMath::Clamp(FMath::RoundToInt(Expected * 255.0f), 0, 255)) <= 1; };

	Bake(Mesh.GetDataCrc());
	TestTrue("Baked for its mesh", Component.HasValidBakedVertexColors());
	const FColor Baked = CenterPixel();
	TestTrue("Baked: the albedo times the colours, red", Near(Baked.R, 0.5f * 64.0f / 255.0f));
	TestTrue("Baked: green", Near(Baked.G, 0.25f * 128.0f / 255.0f));
	TestTrue("Baked: blue (no light added)", Near(Baked.B, 0.75f));

	Bake(Mesh.GetDataCrc() + 1);
	TestFalse("Baked for another mesh", Component.HasValidBakedVertexColors());
	const FColor Stale = CenterPixel();
	TestTrue("Not baked: the albedo, red", Near(Stale.R, 0.5f));
	TestTrue("Not baked: green", Near(Stale.G, 0.25f));
	TestTrue("Not baked: blue", Near(Stale.B, 0.75f));

	Bake(Mesh.GetDataCrc());
	Component.SetMobility(EComponentMobility::Movable);
	Component.MarkRenderStateDirty();
	const FColor Dynamic = CenterPixel();
	const FVector Light = FLightmassWorldInfoSettings().GetEnvironmentLight() + FVector(1.0f, 1.0f, 1.0f);
	TestTrue("Movable: lit per frame, red", Near(Dynamic.R, 0.5f * Light.X));
	TestTrue("Movable: green", Near(Dynamic.G, 0.25f * Light.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererStripsTest, "System.Renderer.GS.Scene.StripsDrawTheSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererStripsTest::RunTest(const FString& Parameters)
{
	// The renderer draws a static mesh from its LPS2 v2 render data: the batches inside the guard band as triangle
	// strips (XYZ3 where no triangle is drawn), the floor across the near plane through the clipper. The frame must be
	// the one the source's triangles give when each is sent on its own (ps2-shipping N12): only the quantization of
	// the positions (a thousandth of a centimetre here) may move a pixel on an edge.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	struct FShape
	{
		FMeshData Source;
		UStaticMesh* Mesh = nullptr;
		AStaticMeshActor* Actor = nullptr;
	};
	TArray<FShape> Shapes;
	const auto Spawn = [&World, &Shapes](const FMeshData& Source, const FVector& Location, const FVector& Scale,
						   const FLinearColor& Color)
	{
		FShape& Shape = Shapes.AddDefaulted_GetRef();
		Shape.Source = Source;
		Shape.Mesh = NewObject<UStaticMesh>();
		(void)Shape.Mesh->BuildFromMeshData(Source);
		Shape.Actor = World.SpawnActor<AStaticMeshActor>(Location, FRotator(0.0f, 30.0f, 0.0f));
		(void)Shape.Actor->GetStaticMeshComponent()->SetStaticMesh(Shape.Mesh);
		UMaterial* Material = NewObject<UMaterial>();
		Material->ShadingModel = MSM_Unlit;
		Material->BaseColor = Color;
		Shape.Actor->GetStaticMeshComponent()->SetMaterial(0, Material);
		Shape.Actor->SetActorScale3D(Scale);
	};
	Spawn(MakeCube(), FVector(0.0f, 0.0f, -100.0f), FVector(20.0f, 20.0f, 0.2f), FLinearColor(0.3f, 0.6f, 0.3f));
	Spawn(MakeCube(), FVector(200.0f, -120.0f, 0.0f), FVector(1.0f), FLinearColor(0.9f, 0.4f, 0.2f));
	Spawn(MakeSphere(24, 16), FVector(300.0f, 150.0f, 20.0f), FVector(1.5f), FLinearColor(0.2f, 0.4f, 0.9f));
	World.SendAllEndOfFrameUpdates();

	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	Camera.SetPerspective(60.0f, float(FrameWidth) / float(FrameHeight), 10.0f, 10000.0f);
	Camera.SetMode(ECameraMode::FreeLook);
	Camera.SetEyeLocation(FVector(-250.0f, 0.0f, 60.0f));
	Camera.SetViewRotation(FRotator(-15.0f, 5.0f, 0.0f));
	FSceneViewFamily Family(
		FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);

	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList Strips;
	Environment.Append(Strips);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	Renderer.Render(Family, Environment, Strips);

	// The same frame from the source's triangles, each on its own through the emitter.
	FGSCommandList Triangles;
	Environment.Append(Triangles);
	Triangles.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	Triangles.SetPrim(Sprite);
	FGSRGBAQ Background;
	Background.R = uint8(FMath::RoundToInt(FGSSceneRenderer::ClearColor.R * 255.0f));
	Background.G = uint8(FMath::RoundToInt(FGSSceneRenderer::ClearColor.G * 255.0f));
	Background.B = uint8(FMath::RoundToInt(FGSSceneRenderer::ClearColor.B * 255.0f));
	Triangles.SetRGBAQ(Background);
	Triangles.AddVertex(Environment.PixelVertex(0.0f, 0.0f, 0));
	Triangles.AddVertex(Environment.PixelVertex(float(FrameWidth), float(FrameHeight), 0));
	Triangles.SetTest(0, FGSDrawEnvironment::DepthTest(true));
	FGSPrimitiveEmitter Emitter(Environment, Triangles);
	for (const FShape& Shape : Shapes)
	{
		const FStaticMeshSceneProxy* Proxy =
			static_cast<const FStaticMeshSceneProxy*>(Shape.Actor->GetStaticMeshComponent()->SceneProxy);
		if (!TestNotNull("A proxy", Proxy))
		{
			return false;
		}
		const FMatrix ToClip = Proxy->GetLocalToWorld() * View.ViewMatrix * View.ProjectionMatrix;
		const FVector Albedo = Proxy->GetSectionMaterial(0).Albedo;
		Emitter.BeginTriangles(false, false, true);
		for (int32 Index = 0; Index + 2 < Shape.Source.Indices.Num(); Index += 3)
		{
			FGSClipVertex Corners[3];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVertex& Vertex = Shape.Source.Vertices[int32(Shape.Source.Indices[Index + Corner])];
				Corners[Corner].Clip = ToClip.TransformPosition(Vertex.Position);
				Corners[Corner].Color = FLinearColor(Albedo.X, Albedo.Y, Albedo.Z, 1.0f);
			}
			Emitter.AddTriangle(Corners[0], Corners[1], Corners[2]);
		}
	}

	int32 NumNoKick = 0;
	for (const FGSRegisterWrite& Write : Strips.GetWrites())
	{
		NumNoKick += Write.Register == EGSRegister::XYZ3 ? 1 : 0;
	}
	TestTrue("Strips: some vertices draw nothing (XYZ3)", NumNoKick > 0);
	// The sphere's poles have triangles of no area (two corners on the pole), which neither list draws; whether one
	// counts as front facing depends on the corner the area is measured from, which a strip may rotate.
	TestTrue("The same triangles drawn",
		FMath::Abs(Renderer.GetFrameStats().TrianglesSubmitted - Emitter.GetNumTriangles()) <= 2);
	TestTrue("Fewer GS writes than independent triangles", Strips.GetWrites().Num() < Triangles.GetWrites().Num());
	const TArray<FColor> Drawn = Rasterize(Strips, Environment);
	const TArray<FColor> Expected = Rasterize(Triangles, Environment);
	int32 NumDifferent = 0;
	for (int32 Index = 0; Index < Drawn.Num() && Index < Expected.Num(); ++Index)
	{
		NumDifferent += Drawn[Index] == Expected[Index] ? 0 : 1;
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("LPS2 strips: %d triangles, %d GS writes (%d as independent triangles), %d pixel(s) differ",
			Renderer.GetFrameStats().TrianglesSubmitted, Strips.GetWrites().Num(), Triangles.GetWrites().Num(),
			NumDifferent));
	TestTrue("The frame is the source's", NumDifferent <= 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererVertexBatchesTest, "System.Renderer.GS.Scene.VertexBatches",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererVertexBatchesTest::RunTest(const FString& Parameters)
{
	// The batches as the list's vertex batch commands (what the PS2 draws on VU1, ps2-shipping N14): unlit meshes, a
	// Static lit one (its light baked: none here, the mesh's colours) and a Movable lit one under a sun (lit per
	// frame), one across the near plane (to be clipped, ps2-polish P8b). Expanded by the C++ emitter, the list draws
	// the frame the emitter draws when it sends the batches itself, pixel for pixel.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const auto Spawn = [&World](const FMeshData& Source, const FVector& Location, const FVector& Scale,
						   const FLinearColor& Color, bool bLit, bool bMovable = false)
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>();
		(void)Mesh->BuildFromMeshData(Source);
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(Location, FRotator(0.0f, 30.0f, 0.0f));
		Actor->GetStaticMeshComponent()->SetMobility(
			bMovable ? EComponentMobility::Movable : EComponentMobility::Static);
		(void)Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		UMaterial* Material = NewObject<UMaterial>();
		Material->ShadingModel = bLit ? MSM_DefaultLit : MSM_Unlit;
		Material->BaseColor = Color;
		Actor->GetStaticMeshComponent()->SetMaterial(0, Material);
		Actor->SetActorScale3D(Scale);
	};
	Spawn(MakeCube(), FVector(0.0f, 0.0f, -100.0f), FVector(20.0f, 20.0f, 0.2f), FLinearColor(0.3f, 0.6f, 0.3f), true);
	Spawn(MakeCube(), FVector(200.0f, -120.0f, 0.0f), FVector(1.0f), FLinearColor(0.9f, 0.4f, 0.2f), false);
	Spawn(
		MakeSphere(24, 16), FVector(300.0f, 150.0f, 20.0f), FVector(1.5f), FLinearColor(0.2f, 0.4f, 0.9f), true, true);
	Spawn(MakeCube(), FVector(250.0f, 40.0f, -40.0f), FVector(0.6f), FLinearColor(0.8f, 0.8f, 0.3f), true);
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.0f, 20.0f, 0.0f));
	World.SendAllEndOfFrameUpdates();

	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	Camera.SetPerspective(60.0f, float(FrameWidth) / float(FrameHeight), 10.0f, 10000.0f);
	Camera.SetMode(ECameraMode::FreeLook);
	Camera.SetEyeLocation(FVector(-250.0f, 0.0f, 60.0f));
	Camera.SetViewRotation(FRotator(-15.0f, 5.0f, 0.0f));
	FSceneViewFamily Family(
		FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);

	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList Emitted;
	Environment.Append(Emitted);
	Renderer.Render(Family, Environment, Emitted);
	const int32 EmittedTriangles = Renderer.GetFrameStats().TrianglesSubmitted;

	Renderer.SetVertexBatches(true);
	FGSCommandList Recorded;
	Environment.Append(Recorded);
	Renderer.Render(Family, Environment, Recorded);
	TestEqual("None emitted as a batch", Emitted.GetVertexBatches().Num(), 0);
	TestTrue("Batches recorded", Recorded.GetVertexBatches().Num() > 0);
	int32 NumLit = 0;
	for (const FGSVertexDraw& Draw : Recorded.GetVertexDraws())
	{
		EGSVertexProgram Program = EGSVertexProgram::StaticUnlit;
		NumLit += Draw.GetProgram(Program) && Program == EGSVertexProgram::StaticLit ? 1 : 0;
	}
	TestTrue("Lit and unlit draws", NumLit > 0 && NumLit < Recorded.GetVertexDraws().Num());
	// The floor's batches across the near plane are recorded to be clipped (on VU1, ps2-polish P8b): the EE's clipper
	// takes none.
	int32 NumClipped = 0;
	for (const FGSVertexBatch& Batch : Recorded.GetVertexBatches())
	{
		NumClipped += Batch.bClip ? 1 : 0;
	}
	TestTrue("The floor recorded to be clipped", NumClipped > 0 && NumClipped < Recorded.GetVertexBatches().Num());
	TestEqual("Clipped batches counted", Renderer.GetFrameStats().BatchesClipped, NumClipped);
	TestEqual("None clipped on the EE", Renderer.GetFrameStats().TrianglesClipped, 0);
	TestTrue("Counted before VU1 culls", Renderer.GetFrameStats().TrianglesSubmitted >= EmittedTriangles);

	FGSCommandList Expanded;
	Expanded.AppendExpanded(Recorded, Environment);
	const TArray<FColor> Drawn = Rasterize(Expanded, Environment);
	const TArray<FColor> Expected = Rasterize(Emitted, Environment);
	int32 NumDifferent = 0;
	for (int32 Index = 0; Index < Drawn.Num() && Index < Expected.Num(); ++Index)
	{
		NumDifferent += Drawn[Index] == Expected[Index] ? 0 : 1;
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Vertex batches: %d batches of %d draws (%d lit), %d pixel(s) differ",
			Recorded.GetVertexBatches().Num(), Recorded.GetVertexDraws().Num(), NumLit, NumDifferent));
	TestEqual("The same frame", NumDifferent, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererPointLightsPerDrawTest, "System.Renderer.GS.Scene.PointLightsPerDraw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGSSceneRendererPointLightsPerDrawTest::RunTest(const FString& Parameters)
{
	// Four point lights reach a Movable lit sphere (a lamp and three muzzle flashes, say): its draw takes the two that
	// light it most (ps2-polish P8b), so VU1's lit program draws it instead of the EE's emitter, and the recorded frame
	// is still the emitter's.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	(void)Mesh->BuildFromMeshData(MakeSphere(24, 16));
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	(void)Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	UMaterial* Material = NewObject<UMaterial>();
	Material->ShadingModel = MSM_DefaultLit;
	Material->BaseColor = FLinearColor(0.8f, 0.8f, 0.8f);
	Actor->GetStaticMeshComponent()->SetMaterial(0, Material);
	Actor->SetActorScale3D(FVector(1.5f));
	// Weak and far, strong and near, weak and near, strong and far: the second and the third light it most.
	const FVector Positions[4] = {FVector(-150.0f, 0.0f, 0.0f), FVector(0.0f, -120.0f, 40.0f),
		FVector(0.0f, 120.0f, 40.0f), FVector(0.0f, 0.0f, 300.0f)};
	const float Intensities[4] = {0.3f, 2.0f, 1.0f, 2.0f};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		APointLight* Light = World.SpawnActor<APointLight>(Positions[Index], FRotator::ZeroRotator);
		Light->GetPointLightComponent()->SetIntensity(Intensities[Index]);
		Light->GetPointLightComponent()->SetAttenuationRadius(500.0f);
	}
	World.SendAllEndOfFrameUpdates();

	UCameraComponent& Camera = MakeCamera(450.0f);
	FSceneViewFamily Family(
		FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList Emitted;
	Environment.Append(Emitted);
	Renderer.Render(Family, Environment, Emitted);
	Renderer.SetVertexBatches(true);
	FGSCommandList Recorded;
	Environment.Append(Recorded);
	Renderer.Render(Family, Environment, Recorded);

	TestEqual("None on the EE", Renderer.GetFrameStats().BatchesOnEmitter, 0);
	if (!TestEqual("One draw recorded", Recorded.GetVertexDraws().Num(), 1))
	{
		return false;
	}
	const FGSVertexLights& Lights = Recorded.GetVertexDraws()[0].Lights;
	TestEqual("Two point lights", Lights.NumPoint, FGSVertexDraw::MaxVU1PointLights);
	TestTrue("The strong near one",
		Lights.Point[0].Position.Equals(Positions[1]) || Lights.Point[1].Position.Equals(Positions[1]));
	TestTrue("The weak near one",
		Lights.Point[0].Position.Equals(Positions[2]) || Lights.Point[1].Position.Equals(Positions[2]));

	FGSCommandList Expanded;
	Expanded.AppendExpanded(Recorded, Environment);
	const TArray<FColor> Drawn = Rasterize(Expanded, Environment);
	const TArray<FColor> Expected = Rasterize(Emitted, Environment);
	int32 NumDifferent = 0;
	for (int32 Index = 0; Index < Drawn.Num() && Index < Expected.Num(); ++Index)
	{
		NumDifferent += Drawn[Index] == Expected[Index] ? 0 : 1;
	}
	TestEqual("The same frame", NumDifferent, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererSkinnedVertexBatchesTest,
	"System.Renderer.GS.Scene.SkinnedVertexBatches",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGSSceneRendererSkinnedVertexBatchesTest::RunTest(const FString& Parameters)
{
	// A posed character's batches as vertex batch commands (ps2-shipping N14b): each with its skin stream and its
	// palette in the list's memory, for the Skinned lit program (a sun). Expanded by the C++ emitter, the list draws
	// what the emitter draws itself, pixel for pixel; a list the batches were appended to keeps its own copy of their
	// palettes.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	USkeletalMesh* Mesh = FSkinnedTestCharacter::MakeMesh();
	if (!TestNotNull("The test character", Mesh))
	{
		return false;
	}
	AActor* Actor = World.SpawnActor<AActor>(FVector(0.0f, 0.0f, -70.0f), FRotator(0.0f, 20.0f, 0.0f));
	USkeletalMeshComponent* Skinned = NewObject<USkeletalMeshComponent>(Actor);
	(void)Skinned->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Skinned->RegisterComponent();
	Skinned->SetSkeletalMesh(Mesh);
	UBlendSpace1D* Pose = NewObject<UBlendSpace1D>();
	Pose->AddSample(
		FSkinnedTestCharacter::MakeRotatedClip(FSkinnedTestCharacter::Spine01, FQuat(FVector(1.0f, 0.0f, 0.0f), 0.6f)),
		0.0f);
	Skinned->GetAnimInstance().SetBlendSpace(Pose);
	Skinned->GetAnimInstance().SetLocomotionBlendInterpSpeed(0.0f);
	Skinned->TickComponent(1.0f / 30.0f);
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.0f, 20.0f, 0.0f));
	World.SendAllEndOfFrameUpdates();

	// From afar, every batch inside the guard band; close up, the batches across the near plane recorded to be clipped
	// (ps2-polish P8b), their palettes too.
	for (const float Distance : {450.0f, 45.0f})
	{
		const bool bCloseUp = Distance < 100.0f;
		UCameraComponent& Camera = MakeCamera(Distance);
		FSceneViewFamily Family(
			FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
		const FSceneView View(FSceneView::FromCamera(Family, Camera));
		Family.Views.Add(&View);
		// Two renderers: each list uploads the character's texture (a texture cache sends it once).
		const FGSDrawEnvironment Environment = MakeEnvironment();
		FGSSceneRenderer Renderer;
		Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
		FGSCommandList Emitted;
		Environment.Append(Emitted);
		Renderer.Render(Family, Environment, Emitted);

		FGSSceneRenderer BatchRenderer;
		BatchRenderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
		BatchRenderer.SetVertexBatches(true);
		FGSCommandList Recorded;
		Environment.Append(Recorded);
		BatchRenderer.Render(Family, Environment, Recorded);
		int32 NumSkinned = 0;
		int32 NumPalettes = 0;
		int32 NumClipped = 0;
		const FGSSkinMatrix* LastPalette = nullptr;
		for (const FGSVertexBatch& Batch : Recorded.GetVertexBatches())
		{
			NumSkinned += Batch.IsSkinned() && Batch.Palette != nullptr && Batch.NumBones > 0 ? 1 : 0;
			NumPalettes += Batch.Palette != LastPalette ? 1 : 0;
			NumClipped += Batch.bClip ? 1 : 0;
			LastPalette = Batch.Palette;
			TestTrue("A palette on a quadword", (UPTRINT(Batch.Palette) & 15) == 0);
		}
		TestTrue("Skinned batches recorded", NumSkinned > 0 && NumSkinned == Recorded.GetVertexBatches().Num());
		TestTrue(bCloseUp ? "Close up: batches to clip" : "Afar: none to clip", bCloseUp == (NumClipped > 0));
		TestEqual("None clipped on the EE", BatchRenderer.GetFrameStats().TrianglesClipped, 0);
		EGSVertexProgram Program = EGSVertexProgram::StaticUnlit;
		TestTrue("Lit by the Skinned lit program",
			Recorded.GetVertexDraws().Num() > 0 && Recorded.GetVertexDraws()[0].GetProgram(Program) &&
				Program == EGSVertexProgram::SkinnedLit);

		// Appended, then the source list's memory reused: the appended batches keep their palettes.
		FGSCommandList Appended;
		Appended.Append(Recorded);
		Recorded.Reset();
		for (const FGSVertexBatch& Batch : Appended.GetVertexBatches())
		{
			FGSSkinMatrix* Reused = Recorded.AllocateSkinPalette(Batch.NumBones);
			FMemory::Memset(Reused, 0x55, Batch.NumBones * sizeof(FGSSkinMatrix));
			TestTrue("Palettes of their own", Reused != Batch.Palette);
		}

		FGSCommandList Expanded;
		Expanded.AppendExpanded(Appended, Environment);
		const TArray<FColor> Drawn = Rasterize(Expanded, Environment);
		const TArray<FColor> Expected = Rasterize(Emitted, Environment);
		int32 NumDifferent = 0;
		int32 NumCovered = 0;
		for (int32 Index = 0; Index < Drawn.Num() && Index < Expected.Num(); ++Index)
		{
			NumDifferent += Drawn[Index] == Expected[Index] ? 0 : 1;
			NumCovered += Expected[Index] == Expected[0] ? 0 : 1;
		}
		UE_LOG(LogTemp, Display, "%s",
			*FString::Printf(
				"Skinned vertex batches at %.0f cm: %d batch(es) (%d to clip) of %d palette(s), %d pixel(s) drawn, "
				"%d differ",
				double(Distance), NumSkinned, NumClipped, NumPalettes, NumCovered, NumDifferent));
		TestTrue("The character drawn", NumCovered > 1000);
		TestEqual("The same frame", NumDifferent, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererCanvasTest, "System.Renderer.GS.Canvas",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererCanvasTest::RunTest(const FString& Parameters)
{
	// A tile covers [X, X + Size) x [Y, Y + Size) of the frame in its colour, over whatever is there.
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList List;
	Environment.Append(List);
	FCanvas Canvas(FrameWidth, FrameHeight);
	Canvas.DrawTile(10.0f, 20.0f, 30.0f, 5.0f, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
	FGSSceneRenderer Renderer;
	Renderer.DrawCanvas(Canvas, Environment, List);
	const TArray<FColor> Pixels = Rasterize(List, Environment);
	TestEqual("Its first pixel", int32(PixelAt(Pixels, 10, 20).R), 255);
	TestEqual("Its last pixel", int32(PixelAt(Pixels, 39, 24).R), 255);
	TestEqual("Right of it", int32(PixelAt(Pixels, 40, 22).R), 0);
	TestEqual("Below it", int32(PixelAt(Pixels, 20, 25).R), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererSkeletalCullingTest,
	"System.Renderer.GS.Scene.SkeletalMeshCulledByPose",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGSSceneRendererSkeletalCullingTest::RunTest(const FString& Parameters)
{
	// A skinned mesh in front of the camera is skinned from its LPS2 v2 batches and drawn; one behind the camera is
	// culled whole, and so is one whose pose moved its vertices out of the view while the component stayed in it: the
	// renderer culls by the pose's bounds, which the component sends with the skin matrices.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	USkeletalMesh* Mesh = MakeSkinnedTestMesh();
	if (!TestNotNull("The test mesh", Mesh))
	{
		return false;
	}
	AActor* Actor = World.SpawnActor<AActor>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	USkeletalMeshComponent* Skinned = NewObject<USkeletalMeshComponent>(Actor);
	(void)Skinned->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Skinned->RegisterComponent();
	Skinned->SetSkeletalMesh(Mesh);

	UCameraComponent& Camera = MakeCamera(300.0f);
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	const auto RenderFrame = [&]()
	{
		World.SendAllEndOfFrameUpdates();
		FSceneViewFamily Family(
			FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
		const FSceneView View(FSceneView::FromCamera(Family, Camera));
		Family.Views.Add(&View);
		FGSCommandList List;
		Environment.Append(List);
		Renderer.Render(Family, Environment, List);
		return Renderer.GetFrameStats();
	};

	FFrameStats Stats = RenderFrame();
	TestEqual("In view: drawn", Stats.ObjectsVisible, 1);
	TestEqual("In view: not culled", Stats.ObjectsCulled, 0);
	TestTrue("Its front faces", Stats.TrianglesSubmitted > 0 && Stats.TrianglesSubmitted <= 6);

	// Behind the camera (at -300 cm on X, looking down +X).
	Actor->SetActorLocation(FVector(-1000.0f, 0.0f, 0.0f));
	Stats = RenderFrame();
	TestEqual("Behind: culled", Stats.ObjectsCulled, 1);
	TestEqual("Behind: nothing drawn", Stats.TrianglesSubmitted, 0);

	// Back in view, but posed 50 m to the side: the pose's bounds leave the view.
	Actor->SetActorLocation(FVector::ZeroVector);
	UBlendSpace1D* Away = NewObject<UBlendSpace1D>();
	Away->AddSample(MakeSkinnedTestClip(FVector(0.0f, 5000.0f, 0.0f)), 0.0f);
	Skinned->GetAnimInstance().SetBlendSpace(Away);
	Skinned->GetAnimInstance().SetLocomotionBlendInterpSpeed(0.0f);
	Skinned->TickComponent(1.0f / 30.0f);
	Stats = RenderFrame();
	TestEqual("Posed out of the view: culled", Stats.ObjectsCulled, 1);
	TestEqual("Posed out of the view: nothing drawn", Stats.TrianglesSubmitted, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererSkinnedViewModelTest,
	"System.Renderer.GS.Scene.SkinnedViewModelOwnerOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGSSceneRendererSkinnedViewModelTest::RunTest(const FString& Parameters)
{
	// First-person arms (Docs/PLANS/ps2-shipping.md N25): a skinned mesh flagged as a view model and owner-only is
	// drawn in the view model pass of its owner's view and in no other view; the skinned body the owner may not see
	// is drawn for the others only. The renderer stamps what it draws (LastRenderTime, the pose's throttling) and
	// records the view's location.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	USkeletalMesh* Mesh = MakeSkinnedTestMesh();
	if (!TestNotNull("The test mesh", Mesh))
	{
		return false;
	}
	APawn* Player = World.SpawnActor<APawn>();
	const auto SpawnSkinned = [&](const FVector& Location)
	{
		AActor* Actor = World.SpawnActor<AActor>(Location, FRotator(0.0f, 0.0f, 0.0f));
		Actor->SetOwner(Player);
		USkeletalMeshComponent* Skinned = NewObject<USkeletalMeshComponent>(Actor);
		(void)Skinned->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Skinned->RegisterComponent();
		Skinned->SetSkeletalMesh(Mesh);
		return Skinned;
	};
	USkeletalMeshComponent* Arms = SpawnSkinned(FVector::ZeroVector);
	Arms->SetRenderAsViewModel(true);
	Arms->SetOnlyOwnerSee(true);
	USkeletalMeshComponent* Body = SpawnSkinned(FVector(0.0f, 0.0f, 0.0f));
	Body->SetOwnerNoSee(true);

	UCameraComponent& Camera = MakeCamera(300.0f);
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	const auto RenderFrame = [&](const AActor* ViewActor)
	{
		Arms->LastRenderTime = -1000.0f;
		Body->LastRenderTime = -1000.0f;
		World.SendAllEndOfFrameUpdates();
		FSceneViewFamily Family(
			FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
		FSceneViewInitOptions Options = FSceneView::FromCamera(Family, Camera);
		Options.ViewActor = ViewActor;
		const FSceneView View(Options);
		Family.Views.Add(&View);
		FGSCommandList List;
		Environment.Append(List);
		Renderer.Render(Family, Environment, List);
		return Renderer.GetFrameStats();
	};

	FFrameStats Stats = RenderFrame(Player);
	TestEqual("The owner's view: the arms only", Stats.ObjectsVisible, 1);
	TestTrue("The arms drawn", Stats.TrianglesSubmitted > 0);
	TestTrue("The arms stamped, not the body", Arms->WasRecentlyRendered() && !Body->WasRecentlyRendered());
	TestTrue("The view recorded",
		World.ViewLocationsRenderedLastFrame.Num() == 1 &&
			World.ViewLocationsRenderedLastFrame[0].Equals(FVector(-300.0f, 0.0f, 0.0f), 1.0f));

	Stats = RenderFrame(nullptr);
	TestEqual("Another view: the body only", Stats.ObjectsVisible, 1);
	TestTrue("The body stamped, not the arms", Body->WasRecentlyRendered() && !Arms->WasRecentlyRendered());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
