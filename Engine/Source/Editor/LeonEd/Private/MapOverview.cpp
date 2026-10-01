#include "MapOverview.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/VisibilityCellVolume.h"
#include "Engine/World.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSReferenceRasterizer.h"
#include "GSSceneCapture.h"
#include "GameFramework/WorldSettings.h"
#include "LeonEdLog.h"
#include "Math/OrthoMatrix.h"
#include "PalettedTexture.h"
#include "UObject/Package.h"
#include "ViewMatrices.h"

namespace
{

	/** The GS's local memory: 512 pages of 32 blocks (4 MB). */
	constexpr uint32 GSPages = 512;
	constexpr uint32 BlocksPerPage = 32;
	/** A PSMCT32 or PSMZ24 page: 64 x 32 pixels. */
	constexpr uint32 PagePixels = 64 * 32;
	/** How far the eyes are above or below the map: outside every visibility cell, which would cull by portals. */
	constexpr float EyeDistance = 10000.0f;
	/** A pixel's height where the view reaches nothing. */
	constexpr float NoHeight = -1.0e30f;

	/** What the view does not reach (outside the map). */
	const FColor OutsideColor(20, 24, 20, 255);
	/** What a player cannot stand in: walls, houses, containers, crates. */
	const FColor SolidColor(62, 64, 58, 255);
	/** How much of its colour an edge line keeps. */
	constexpr float EdgeShade = 0.4f;
	/**
	 * A floor's look: its colour moves toward its grey by Desaturation, and its brightness halfway (Flattening) toward
	 * FloorLuminance, keeping its hue: the baked shadows and the occlusion soften, the materials stay apart.
	 */
	constexpr float Desaturation = 0.3f;
	constexpr float Flattening = 0.5f;
	constexpr float FloorLuminance = 175.0f;

	/** What a pixel of the overview shows. */
	enum class EOverviewPixel : uint8
	{
		Outside,
		Solid,
		Floor,
	};

	/** Whether a component is part of the static world the overview shows (FGSSceneCapture's choice). */
	bool IsStaticWorld(const UStaticMeshComponent& Mesh)
	{
		const AActor* Owner = Mesh.GetOwner();
		return Mesh.Mobility == EComponentMobility::Static && Mesh.IsVisible() && Mesh.GetStaticMesh() != nullptr &&
			(Owner == nullptr || (!Owner->IsHidden() && !Owner->IsPendingKillPending()));
	}

	uint8 ToByte(float Value)
	{
		return uint8(FMath::Clamp(FMath::RoundToInt(Value), 0, 255));
	}

	/**
	 * One orthographic view of the square along Z, Side x Side pixels, the top row north and the left column west:
	 * from EyeZ, looking down or up, the geometry between the planes at NearZ and FarZ. bMirrored turns the view's
	 * handedness, so the renderer culls the faces it would draw and draws the ones it would cull. OutColors (when
	 * given) gets the frame, OutHeights each pixel's nearest surface's height (NoHeight where nothing is).
	 */
	bool RenderView(UWorld& World, int32 Side, const FVector2D& Center, float Size, float EyeZ, bool bLookUp,
		bool bMirrored, float NearZ, float FarZ, TArray<FColor>* OutColors, TArray<float>& OutHeights)
	{
		const uint32 FramePages = uint32(Side * Side) / PagePixels;
		FGSDrawEnvironment Environment;
		Environment.Frame.FBP = 0;
		Environment.Frame.FBW = uint8(Side / 64);
		Environment.Frame.PSM = EGSPixelFormat::PSMCT32;
		Environment.ZBuf.ZBP = uint16(FramePages);
		Environment.ZBuf.PSM = EGSPixelFormat::PSMZ24;
		Environment.Width = uint16(Side);
		Environment.Height = uint16(Side);

		// North up on the screen; east to the right from above, and from below in the mirrored view (the renderer's
		// own view from below has east to the left: its columns are turned back below).
		const FVector Eye(Center.X, Center.Y, EyeZ);
		const FVector Forward(0.0f, 0.0f, bLookUp ? 1.0f : -1.0f);
		const bool bEastLeft = bLookUp && !bMirrored;
		const FVector Right(0.0f, bEastLeft ? -1.0f : 1.0f, 0.0f);
		const FVector Up(1.0f, 0.0f, 0.0f);
		const float Near = FMath::Abs(NearZ - EyeZ);
		const float Far = FMath::Max(Near + 1.0f, FMath::Abs(FarZ - EyeZ));
		const FMatrix ViewMatrix = MakeViewMatrix(Eye, Forward, Right, Up);
		const FMatrix Projection(FOrthoMatrix(Size * 0.5f, Size * 0.5f, 1.0f / (Far - Near), -Near));
		FGSCommandList List;
		Environment.Append(List);
		// The capture keeps the textures the list points into until the list is rasterized.
		FGSSceneCapture Capture;
		const int32 Drawn = Capture.RecordStaticWorld(World, Eye, ViewMatrix, Projection, Environment,
			2 * FramePages * BlocksPerPage, (GSPages - (2 * FramePages)) * BlocksPerPage, List);
		if (Drawn == 0)
		{
			return false;
		}
		FGSReferenceRasterizer Rasterizer;
		Rasterizer.Execute(List);
		if (OutColors != nullptr)
		{
			*OutColors = Rasterizer.ReadFrame(Environment.Frame, uint32(Side), uint32(Side));
		}
		// The Z buffer: the far plane at 0, the near one at MaxDepth24.
		OutHeights.SetNumUninitialized(Side * Side);
		const double DepthRange = double(Far - Near);
		const double Direction = bLookUp ? 1.0 : -1.0;
		for (int32 Y = 0; Y < Side; ++Y)
		{
			for (int32 X = 0; X < Side; ++X)
			{
				const uint32 Z = Rasterizer.ReadZ(Environment.ZBuf, uint32(Side), uint32(X), uint32(Y));
				const double Depth =
					double(Near) + ((1.0 - (double(Z) / double(FGSDrawEnvironment::MaxDepth24))) * DepthRange);
				const int32 Column = bEastLeft ? (Side - 1 - X) : X;
				OutHeights[(Y * Side) + Column] = Z == 0 ? NoHeight : float(double(EyeZ) + (Direction * Depth));
			}
		}
		return true;
	}

} // namespace

bool FMapOverview::GetWorldSquare(
	const UWorld& World, float Margin, FVector2D& OutCenter, float& OutSize, float& OutMinZ, float& OutMaxZ)
{
	FBox Bounds(ForceInit);
	FBox Cells(ForceInit);
	if (World.PersistentLevel != nullptr)
	{
		for (const AActor* Actor : World.PersistentLevel->Actors)
		{
			if (Actor == nullptr)
			{
				continue;
			}
			if (const AVisibilityCellVolume* Cell = Cast<AVisibilityCellVolume>(Actor))
			{
				Cells += Cell->GetBrushBounds();
			}
			for (const UActorComponent* Component : Actor->GetComponents())
			{
				const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
				if (Mesh != nullptr && IsStaticWorld(*Mesh))
				{
					Bounds += Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform());
				}
			}
		}
	}
	if (!Bounds.IsValid)
	{
		return false;
	}
	// The square: the cells' when the map has some (they hold where the players go, not the scenery around it).
	const FBox& Square = Cells.IsValid ? Cells : Bounds;
	OutCenter = FVector2D((Square.Min.X + Square.Max.X) * 0.5f, (Square.Min.Y + Square.Max.Y) * 0.5f);
	OutSize = FMath::Max(Square.Max.X - Square.Min.X, Square.Max.Y - Square.Min.Y) + (2.0f * Margin);
	OutMinZ = Bounds.Min.Z;
	OutMaxZ = Bounds.Max.Z;
	return OutSize > 0.0f;
}

bool FMapOverview::Render(UWorld& World, const FMapOverviewSettings& Settings, const FVector2D& Center, float Size,
	float MinZ, float MaxZ, TArray<FColor>& OutPixels)
{
	const int32 Resolution = Settings.Resolution;
	const int32 Supersampling = FMath::Max(1, Settings.Supersampling);
	const int32 Side = Resolution * Supersampling;
	if (Resolution < FPalettedTextureBuilder::MinSize || Resolution > FPalettedTextureBuilder::MaxSize ||
		!FMath::IsPowerOfTwo(Resolution) || Side % 64 != 0 || Side > 512 || Size <= 0.0f)
	{
		UE_LOG(LogLeonEd, Error,
			"MapOverview: %dx%d texels at %d pixels each is no overview (8 to 256, 64 to 512 "
			"pixels)",
			Resolution, Resolution, Supersampling);
		return false;
	}
	const float Bottom = FMath::Min(MinZ, Settings.SolidHeight) - 100.0f;
	const float Top = FMath::Max(MaxZ, Settings.ClipHeight) + 100.0f;

	// From above, cut at the clip height: the floors' colours and heights.
	TArray<FColor> Colors;
	TArray<float> Floors;
	if (!RenderView(
			World, Side, Center, Size, Top + EyeDistance, false, false, Settings.ClipHeight, Bottom, &Colors, Floors))
	{
		return false;
	}
	// From below, from the solid height up: the first face looking down (a ceiling), and mirrored the first face
	// looking up (the top of whatever the point is inside).
	TArray<float> Ceilings;
	TArray<float> Tops;
	const float Below = Bottom - EyeDistance;
	(void)RenderView(World, Side, Center, Size, Below, true, false, Settings.SolidHeight, Top, nullptr, Ceilings);
	(void)RenderView(World, Side, Center, Size, Below, true, true, Settings.SolidHeight, Top, nullptr, Tops);
	if (Ceilings.Num() != Side * Side || Tops.Num() != Side * Side)
	{
		Ceilings.Init(NoHeight, Side * Side);
		Tops.Init(NoHeight, Side * Side);
	}

	TArray<EOverviewPixel> Kinds;
	Kinds.SetNumUninitialized(Side * Side);
	for (int32 Index = 0; Index < Side * Side; ++Index)
	{
		constexpr float CoplanarTolerance = 1.0f;
		const bool bInsideSolid = Tops[Index] > NoHeight &&
			(Ceilings[Index] <= NoHeight || Tops[Index] <= Ceilings[Index] + CoplanarTolerance);
		Kinds[Index] = bInsideSolid    ? EOverviewPixel::Solid
			: Floors[Index] > NoHeight ? EOverviewPixel::Floor
									   : EOverviewPixel::Outside;
	}

	// CS's style: the floors paler and flatter, a dark line along what is not floor and on the higher side of a step.
	const int32 EdgeReach = FMath::Max(1, Supersampling / 2);
	TArray<FColor> Styled;
	Styled.SetNumUninitialized(Side * Side);
	for (int32 Y = 0; Y < Side; ++Y)
	{
		for (int32 X = 0; X < Side; ++X)
		{
			const int32 Index = (Y * Side) + X;
			if (Kinds[Index] != EOverviewPixel::Floor)
			{
				Styled[Index] = Kinds[Index] == EOverviewPixel::Solid ? SolidColor : OutsideColor;
				continue;
			}
			const float Height = Floors[Index];
			bool bEdge = false;
			for (int32 Dy = -EdgeReach; Dy <= EdgeReach && !bEdge; ++Dy)
			{
				for (int32 Dx = -EdgeReach; Dx <= EdgeReach && !bEdge; ++Dx)
				{
					const int32 NX = X + Dx;
					const int32 NY = Y + Dy;
					if (FMath::Abs(Dx) + FMath::Abs(Dy) > EdgeReach || NX < 0 || NY < 0 || NX >= Side || NY >= Side)
					{
						continue;
					}
					const int32 Other = (NY * Side) + NX;
					bEdge = Kinds[Other] != EOverviewPixel::Floor || Height - Floors[Other] > StepHeight;
				}
			}
			const FColor& Source = Colors[Index];
			const float Grey = (0.3f * Source.R) + (0.59f * Source.G) + (0.11f * Source.B);
			const float Flat = Grey > 1.0f ? FMath::Lerp(Grey, FloorLuminance, Flattening) / Grey : 1.0f;
			const float Shade = Flat * (bEdge ? EdgeShade : 1.0f);
			auto Channel = [Grey, Shade](uint8 Value)
			{ return ToByte(FMath::Lerp(float(Value), Grey, Desaturation) * Shade); };
			Styled[Index] = FColor(Channel(Source.R), Channel(Source.G), Channel(Source.B), 255);
		}
	}

	// Averaged down to the texture's side.
	OutPixels.SetNumUninitialized(Resolution * Resolution);
	const int32 Samples = Supersampling * Supersampling;
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			int32 Sum[3] = {0, 0, 0};
			for (int32 Sy = 0; Sy < Supersampling; ++Sy)
			{
				for (int32 Sx = 0; Sx < Supersampling; ++Sx)
				{
					const FColor& Pixel = Styled[(((Y * Supersampling) + Sy) * Side) + (X * Supersampling) + Sx];
					Sum[0] += Pixel.R;
					Sum[1] += Pixel.G;
					Sum[2] += Pixel.B;
				}
			}
			OutPixels[(Y * Resolution) + X] = FColor(uint8((Sum[0] + (Samples / 2)) / Samples),
				uint8((Sum[1] + (Samples / 2)) / Samples), uint8((Sum[2] + (Samples / 2)) / Samples), 255);
		}
	}
	return true;
}

FString FMapOverview::GetOverviewPackageName(const UWorld& World)
{
	return World.GetOutermost()->GetName() + TEXT("/T_") + World.GetMapName() + TEXT("_Overview");
}

bool FMapOverview::Build(UWorld& World, const FMapOverviewSettings& Settings, UTexture2D& Texture)
{
	FVector2D Center = FVector2D::ZeroVector;
	float Size = 0.0f;
	float MinZ = 0.0f;
	float MaxZ = 0.0f;
	TArray<FColor> Pixels;
	if (!GetWorldSquare(World, Settings.Margin, Center, Size, MinZ, MaxZ) ||
		!Render(World, Settings, Center, Size, MinZ, MaxZ, Pixels))
	{
		UE_LOG(LogLeonEd, Error, "MapOverview: %s has no static geometry to show", *World.GetPathName());
		return false;
	}
	// RGBA8, the bottom row (south) first as a texture's texels are, then paletted as the PS2 cook would, one level.
	const int32 Resolution = Settings.Resolution;
	TArray<uint8> Rgba;
	Rgba.SetNumUninitialized(Resolution * Resolution * 4);
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const FColor& Pixel = Pixels[(((Resolution - 1) - Y) * Resolution) + X];
			uint8* Texel = &Rgba[((Y * Resolution) + X) * 4];
			Texel[0] = Pixel.R;
			Texel[1] = Pixel.G;
			Texel[2] = Pixel.B;
			Texel[3] = 255;
		}
	}
	FPalettedTexture Paletted;
	if (!FPalettedTextureBuilder::Build(Rgba.GetData(), Resolution, Resolution, true, Paletted))
	{
		UE_LOG(LogLeonEd, Error, "MapOverview: %s: the overview cannot be paletted", *World.GetPathName());
		return false;
	}
	Texture.SRGB = 1;
	if (!Texture.SetPlatformData(Paletted.SizeX, Paletted.SizeY, Paletted.Format, Paletted.Data.GetData()))
	{
		UE_LOG(LogLeonEd, Error, "MapOverview: %s: its texels were refused", *Texture.GetPathName());
		return false;
	}
	Texture.UpdateResource();
	if (AWorldSettings* WorldSettings = World.GetWorldSettings())
	{
		WorldSettings->OverviewSettings.Texture = &Texture;
		WorldSettings->OverviewSettings.Center = Center;
		WorldSettings->OverviewSettings.Size = Size;
	}
	UE_LOG(LogLeonEd, Display, "MapOverview: %s: %dx%d %s (%d colours), a %.0f cm square centred at (%.0f, %.0f)",
		*Texture.GetPathName(), Paletted.SizeX, Paletted.SizeY,
		Paletted.Format == PF_P4 ? TEXT("PSMT4") : TEXT("PSMT8"), Paletted.NumSourceColors, double(Size),
		double(Center.X), double(Center.Y));
	return true;
}
