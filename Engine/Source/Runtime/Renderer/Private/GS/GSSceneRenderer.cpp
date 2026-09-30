#include "GSSceneRenderer.h"

#include "CanvasTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Debug/DebugDraw.h"
#include "Effects/WorldEffects.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Frustum.h"
#include "GLClipSpace.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/LowLevelMemTracker.h"
#include "LPS2Mesh.h"
#include "Level/Light.h"
#include "LightSceneProxy.h"
#include "MaterialShared.h"
#include "Math/ScaleMatrix.h"
#include "Math/TranslationMatrix.h"
#include "Misc/MemStack.h"
#include "Misc/Scratchpad.h"
#include "SceneManagement.h"
#include "ScenePrivate.h"
#include "SceneView.h"
#include "SkeletalMeshSceneProxy.h"
#include "SkyBoxGeometry.h"
#include "StaticMeshResources.h"
#include "StaticMeshSceneProxy.h"
#include "Stats/Stats.h"
#include "WorldEffectsGeometry.h"

DECLARE_CYCLE_STAT(TEXT("GS Scene Render"), STAT_GSSceneRender, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Canvas"), STAT_GSDrawCanvas, STATGROUP_SceneRendering);
// The scene's parts (N29): the profile's breakdown of GS Scene Render.
DECLARE_CYCLE_STAT(TEXT("GS Visibility"), STAT_GSVisibility, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Sky"), STAT_GSSky, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Opaque"), STAT_GSOpaque, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Skinned"), STAT_GSSkinned, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Translucent and Effects"), STAT_GSTranslucent, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS View Model"), STAT_GSViewModel, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Clipped Batches"), STAT_GSClippedBatches, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("GS Emitted Batches"), STAT_GSEmittedBatches, STATGROUP_SceneRendering);

const FLinearColor FGSSceneRenderer::ClearColor(0.08f, 0.09f, 0.11f, 1.0f);

namespace
{

	/** The world effects' mask as the GS samples it: white, the spot in the alpha. */
	uint8 GEffectsMaskKey = 0;

	/** A full-frame sprite of Color at depth Z (the environment's test is the caller's). */
	void AddFrameSprite(FGSCommandList& List, const FGSDrawEnvironment& Environment, const FGSRGBAQ& Color, uint32 Z)
	{
		FGSPrim Sprite;
		Sprite.Type = EGSPrimitive::Sprite;
		List.SetPrim(Sprite);
		List.SetRGBAQ(Color);
		List.AddVertex(Environment.PixelVertex(0.0f, 0.0f, Z));
		List.AddVertex(Environment.PixelVertex(float(Environment.Width), float(Environment.Height), Z));
	}

	[[nodiscard]] uint8 UnitByte(float Value)
	{
		return uint8(FMath::Clamp(FMath::RoundToInt(Value * 255.0f), 0, 255));
	}

	/** TEX1 for the scene's textures: bilinear, level 0 (the PS2 cook brings the mips, E3). */
	[[nodiscard]] FGSTex1 BilinearSampling()
	{
		FGSTex1 Tex1;
		Tex1.bFixedLOD = true;
		Tex1.MMAG = EGSFilter::Linear;
		Tex1.MMIN = EGSFilter::Linear;
		return Tex1;
	}

	/**
	 * The frame's pixels per centimetre at a depth of 1 cm through Projection (the geometric mean of the two axes'):
	 * a surface facing the view at depth w shows GetPixelsPerCm / w pixels per centimetre.
	 */
	[[nodiscard]] float GetPixelsPerCm(const FMatrix& Projection, const FGSDrawEnvironment& Environment)
	{
		const float AcrossX = FMath::Abs(Projection.M[0][0]) * float(Environment.Width) * 0.5f;
		const float AcrossY = FMath::Abs(Projection.M[1][1]) * float(Environment.Height) * 0.5f;
		return FMath::Max(FMath::Sqrt(AcrossX * AcrossY), SMALL_NUMBER);
	}

	/**
	 * A normal's transform to the world: the inverse transpose of LocalToWorld (UE: TransposeAdjoint), negated when it
	 * mirrors; the transformed normal is then normalized.
	 */
	[[nodiscard]] FMatrix MakeNormalToWorld(const FMatrix& LocalToWorld)
	{
		const FMatrix Adjoint = LocalToWorld.TransposeAdjoint();
		return LocalToWorld.Determinant() < 0.0f ? Adjoint * -1.0f : Adjoint;
	}

	/** The triangles a vertex batch's strips close (VU1 culls some of them). */
	[[nodiscard]] int32 CountStripTriangles(const FGSVertexBatch& Batch)
	{
		int32 Count = 0;
		for (uint32 Index = 2; Index < Batch.NumVertices; ++Index)
		{
			Count += (uint8(Batch.Normals[(Index * 4) + 3]) & FGSVertexBatch::FlagNoKick) == 0 ? 1 : 0;
		}
		return Count;
	}

	/** Where a LPS2 v2 batch's sphere is against the view (plan D8). */
	enum class EBatchPlacement : uint8
	{
		/** Wholly outside a plane of the view: nothing to draw. */
		Outside,
		/** Inside the guard band and the near and far planes: its strips need no clipping. */
		Inside,
		/** Across a clip plane: its triangles go through the clipper. */
		Crossing,
	};

	/**
	 * The planes of clip space (x, y in [-w, w], z in [0, w]) and of the guard band, in a mesh's space, normalized so
	 * that a point's distance to them is in the mesh's units: what a batch's sphere is placed against.
	 */
	class FBatchPlacer
	{
	public:
		FBatchPlacer(const FMatrix& LocalToClip, float GuardX, float GuardY)
		{
			// UE's row vectors: clip component C is the dot product of (P, 1) with the matrix's column C.
			const auto Column = [&LocalToClip](int32 C)
			{ return FVector4(LocalToClip.M[0][C], LocalToClip.M[1][C], LocalToClip.M[2][C], LocalToClip.M[3][C]); };
			const FVector4 X = Column(0);
			const FVector4 Y = Column(1);
			const FVector4 Z = Column(2);
			const FVector4 W = Column(3);
			// The view: near, far and the four sides; then the guard band's four sides.
			Planes[0] = Normalized(Z);
			Planes[1] = Normalized(W - Z);
			Planes[2] = Normalized(W - X);
			Planes[3] = Normalized(W + X);
			Planes[4] = Normalized(W - Y);
			Planes[5] = Normalized(W + Y);
			Planes[6] = Normalized((W * GuardX) - X);
			Planes[7] = Normalized((W * GuardX) + X);
			Planes[8] = Normalized((W * GuardY) - Y);
			Planes[9] = Normalized((W * GuardY) + Y);
		}

		[[nodiscard]] EBatchPlacement Place(const FLPS2Batch& Batch) const
		{
			return Place(
				FVector(Batch.BoundsCenter[0], Batch.BoundsCenter[1], Batch.BoundsCenter[2]), Batch.BoundsRadius);
		}

		/** A sphere in the mesh's space (a skinned batch's, around its posed vertices). */
		[[nodiscard]] EBatchPlacement Place(const FVector& Center, float Radius) const
		{
			bool bInside = true;
			for (int32 Plane = 0; Plane < NumPlanes; ++Plane)
			{
				const FVector4& P = Planes[Plane];
				const float Distance = (P.X * Center.X) + (P.Y * Center.Y) + (P.Z * Center.Z) + P.W;
				const bool bViewPlane = Plane < 6;
				if (bViewPlane && Distance < -Radius)
				{
					return EBatchPlacement::Outside;
				}
				// The clipper's planes: near, far and the guard band.
				if ((Plane < 2 || !bViewPlane) && Distance < Radius)
				{
					bInside = false;
				}
			}
			return bInside ? EBatchPlacement::Inside : EBatchPlacement::Crossing;
		}

	private:
		static constexpr int32 NumPlanes = 10;

		[[nodiscard]] static FVector4 Normalized(const FVector4& Plane)
		{
			const float Length = FMath::Sqrt((Plane.X * Plane.X) + (Plane.Y * Plane.Y) + (Plane.Z * Plane.Z));
			return Length > 0.0f ? Plane * (1.0f / Length) : FVector4(0.0f, 0.0f, 0.0f, Plane.W);
		}

		FVector4 Planes[NumPlanes];
	};

} // namespace

FGSSceneRenderer::FSectionState FGSSceneRenderer::BindMaterial(
	const FMaterial& Material, float UvPerCm, FGSCommandList& List)
{
	FSectionState State;
	State.bLit = Material.Shading == EMaterialLightingModel::Lit;
	State.bTranslucent = Material.IsTransparent();
	State.Albedo = FLinearColor(Material.Albedo.X, Material.Albedo.Y, Material.Albedo.Z, 1.0f);
	State.Alpha = FMath::Clamp(Material.Alpha, 0.0f, 1.0f);
	State.UvScale = Material.UvScale;
	if (Material.AlbedoMap == nullptr)
	{
		return State;
	}
	if (TextureState.Texture != Material.AlbedoMap)
	{
		FGSTextureBinding Binding;
		if (!TextureCache.BindTexture(*Material.AlbedoMap, List, Binding))
		{
			return State;
		}
		if (Binding.bFlat)
		{
			// Not resident this frame: the texels' average stands in (MODULATE with a texel of that colour).
			constexpr float InverseByte = 1.0f / 255.0f;
			State.Albedo = FLinearColor(State.Albedo.R * float(Binding.FlatColor.R) * InverseByte,
				State.Albedo.G * float(Binding.FlatColor.G) * InverseByte,
				State.Albedo.B * float(Binding.FlatColor.B) * InverseByte, 1.0f);
			State.Alpha *= float(Binding.FlatColor.A) * InverseByte;
			return State;
		}
		if (Binding.NumLevels > 1)
		{
			List.SetMipTbp1(0, Binding.MipTbp1);
			if (Binding.NumLevels > 4)
			{
				List.SetMipTbp2(0, Binding.MipTbp2);
			}
		}
		List.SetTex0(0, Binding.Tex0);
		++FrameStats.Tex0Writes;
		TextureState.Texture = Material.AlbedoMap;
		TextureState.Binding = Binding;
	}
	const FGSTextureBinding& Binding = TextureState.Binding;
	FGSTex1 Tex1 = BilinearSampling();
	const float TexelsPerCm = UvPerCm * FMath::Sqrt(FMath::Abs(State.UvScale.X * State.UvScale.Y)) *
		FMath::Sqrt(float(1 << Binding.Tex0.TW) * float(1 << Binding.Tex0.TH));
	if (Material.bMipmaps && Binding.NumLevels > 1 && TexelsPerCm > 0.0f)
	{
		// LOD = log2(1/Q) + K: log2 of the texels a pixel covers at the depth 1/Q (the class comment).
		Tex1.bFixedLOD = false;
		Tex1.MXL = uint8(Binding.NumLevels - 1);
		Tex1.MMIN = EGSFilter::LinearMipmapLinear;
		Tex1.L = 0;
		const float K = FMath::Log2(TexelsPerCm / PixelsPerCm) + Material.LodBias;
		Tex1.K = int16(FMath::Clamp(FMath::RoundToInt(K * 16.0f), -2048, 2047));
	}
	// The texture's address modes (UE: AddressX, AddressY): repeat, or clamp at its edges (a sky's faces).
	FGSClamp Clamp;
	Clamp.WMS = Material.AlbedoMap->AddressX == ETextureAddress::Clamp ? EGSWrapMode::Clamp : EGSWrapMode::Repeat;
	Clamp.WMT = Material.AlbedoMap->AddressY == ETextureAddress::Clamp ? EGSWrapMode::Clamp : EGSWrapMode::Repeat;
	SetSampler(List, Tex1, Clamp);
	State.bTextured = true;
	return State;
}

void FGSSceneRenderer::SetSampler(FGSCommandList& List, const FGSTex1& Tex1, const FGSClamp& Clamp)
{
	const uint64 Tex1Value = Tex1.Encode();
	const uint64 ClampValue = Clamp.Encode();
	if (!TextureState.bHasSampler || TextureState.Tex1 != Tex1Value)
	{
		List.SetTex1(0, Tex1);
	}
	if (!TextureState.bHasSampler || TextureState.Clamp != ClampValue)
	{
		List.SetClamp(0, Clamp);
	}
	TextureState.bHasSampler = true;
	TextureState.Tex1 = Tex1Value;
	TextureState.Clamp = ClampValue;
}

void FGSSceneRenderer::DrawStaticSection(FGSPrimitiveEmitter& Emitter, const FStaticMeshSceneProxy& Proxy,
	int32 SectionIndex, int32 LODIndex, const FMatrix& ViewProjection, FGSCommandList& List,
	const FGSDrawEnvironment& Environment)
{
	DrawMeshSection(Emitter, Proxy.GetStaticMesh().GetLODResources(LODIndex).RenderData, SectionIndex,
		Proxy.GetLocalToWorld(), Proxy.GetWorldBounds(), Proxy.GetSectionMaterial(SectionIndex),
		Proxy.HasStaticLighting(), LODIndex == 0 ? Proxy.GetBakedVertexColors() : nullptr, nullptr, ViewProjection,
		List, Environment);
}

int32 FGSSceneRenderer::GetStaticMeshLOD(
	const FStaticMeshSceneProxy& Proxy, const FBox& Bounds, const FSceneView& View) const
{
	const UStaticMesh& Mesh = Proxy.GetStaticMesh();
	if (Mesh.GetNumLODs() <= 1 || Proxy.GetBakedVertexColors() != nullptr)
	{
		return 0;
	}
	// The world bounds' sphere (UE: the proxy's bounds' origin and sphere radius).
	return ComputeStaticMeshLOD(Mesh, Bounds.GetCenter(), Bounds.GetExtent().Size(), View.ViewLocation,
		View.ProjectionMatrix, LODDistanceScale);
}

void FGSSceneRenderer::MakeSkinPalette(const FLPS2Mesh& Mesh, const FLPS2Batch& Batch,
	const TArray<FMatrix>& SkinMatrices, FVector& OutCenter, float& OutRadius)
{
	const FLPS2SkinPalette& Palette = Mesh.GetPalette(Batch);
	const int32 NumBones = int32(Palette.NumBones);
	check(NumBones > 0 && NumBones <= int32(FGSVertexBatch::MaxBones));
	BatchPalette.SetNum(NumBones, false);
	const FVector BindCenter(Batch.BoundsCenter[0], Batch.BoundsCenter[1], Batch.BoundsCenter[2]);
	FVector Min(TNumericLimits<float>::Max());
	FVector Max(TNumericLimits<float>::Lowest());
	float MaxScale = 0.0f;
	// Where each bone moves the bind-pose sphere's centre.
	FVector Centers[FGSVertexBatch::MaxBones];
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const int32 Bone = Palette.Bones[Index];
		BatchPalette[Index] =
			SkinMatrices.IsValidIndex(Bone) ? FGSSkinMatrix::FromMatrix(SkinMatrices[Bone]) : FGSSkinMatrix();
		const FGSSkinMatrix& Skin = BatchPalette[Index];
		Centers[Index] = Skin.TransformPosition(BindCenter);
		Min = Min.ComponentMin(Centers[Index]);
		Max = Max.ComponentMax(Centers[Index]);
		for (int32 Row = 0; Row < 3; ++Row)
		{
			const float RowLengthSquared =
				FMath::Square(Skin.Rows[Row][0]) + FMath::Square(Skin.Rows[Row][1]) + FMath::Square(Skin.Rows[Row][2]);
			MaxScale = FMath::Max(MaxScale, RowLengthSquared);
		}
	}
	OutCenter = (Min + Max) * 0.5f;
	float CentersRadius = 0.0f;
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		CentersRadius = FMath::Max(CentersRadius, FVector::Dist(Centers[Index], OutCenter));
	}
	OutRadius = ((CentersRadius + (Batch.BoundsRadius * FMath::Sqrt(MaxScale))) * 1.0001f) + 0.01f;
}

void FGSSceneRenderer::DrawMeshSection(FGSPrimitiveEmitter& Emitter, const FLPS2Mesh& Mesh, int32 SectionIndex,
	const FMatrix& LocalToWorld, const FBox& WorldBounds, const FMaterial& Material, bool bStaticLighting,
	const FLPS2ColorStreams* BakedColors, const TArray<FMatrix>* SkinMatrices, const FMatrix& ViewProjection,
	FGSCommandList& List, const FGSDrawEnvironment& Environment)
{
	if (SectionIndex >= Mesh.GetNumSections())
	{
		return;
	}
	const bool bSkinned = SkinMatrices != nullptr && Mesh.IsSkinned();
	const FLPS2Section& Section = Mesh.GetSection(SectionIndex);
	const float MeshScale = LocalToWorld.GetMaximumAxisScale();
	const float UvPerCm = MeshScale > 0.0f ? Mesh.GetSectionUvDensity(SectionIndex) / MeshScale : 0.0f;
	const FSectionState State = BindMaterial(Material, UvPerCm, List);
	if (State.bTranslucent)
	{
		SetDepthWrite(List, Environment, false);
	}
	// The draw every batch of the section shares: the mesh's quantization, its transforms, the material, the lights.
	const FLPS2MeshHeader& Header = Mesh.GetHeader();
	FGSVertexDraw Draw;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Draw.PositionScale[Axis] = Header.PositionScale[Axis];
		Draw.PositionBias[Axis] = Header.PositionBias[Axis];
	}
	Draw.LocalToWorld = LocalToWorld;
	Draw.LocalToClip = LocalToWorld * ViewProjection;
	Draw.Color = FLinearColor(State.Albedo.R, State.Albedo.G, State.Albedo.B, State.Alpha);
	Draw.UvScale = State.UvScale;
	// A lit section of a Static component takes its light from its baked colours (none: the mesh's own colours, as if
	// unlit); a Movable one is lit per vertex by the frame's lights (N22): only that one is a lit draw.
	const FLPS2ColorStreams* BakedLighting = State.bLit && bStaticLighting ? BakedColors : nullptr;
	Draw.bLit = State.bLit && !bStaticLighting;
	Draw.bTextured = State.bTextured;
	Draw.bBlend = State.bTranslucent;
	Draw.Fog = FrameFog;
	if (Draw.bLit)
	{
		Draw.NormalToWorld = MakeNormalToWorld(LocalToWorld);
		// Only the point lights whose range reaches the mesh's bounds (N29): the others light none of its vertices,
		// and without them a microprogram does the draw's light (VU1 has the ambient and one sun).
		Draw.Lights = FrameLights;
		Draw.Lights.NumPoint = 0;
		for (int32 Index = 0; Index < FrameLights.NumPoint; ++Index)
		{
			const FGSVertexLights::FPoint& Point = FrameLights.Point[Index];
			if (WorldBounds.ComputeSquaredDistanceToPoint(Point.Position) < FMath::Square(Point.Radius))
			{
				Draw.Lights.Point[Draw.Lights.NumPoint++] = Point;
			}
		}
	}
	// VU1 draws the batches inside the guard band when a microprogram does the draw's lighting (plan N14); a skinned
	// batch's palette goes with it (N14b).
	Draw.bSkinned = bSkinned;
	EGSVertexProgram Program = EGSVertexProgram::StaticUnlit;
	const bool bRecordBatches = bVertexBatches && Draw.GetProgram(Program);
	int32 DrawIndex = INDEX_NONE;
	// The palette the section's last recorded skinned batch took, in the list's memory.
	const FGSSkinMatrix* RecordedPalette = nullptr;
	uint32 RecordedBones = 0;
	const FBatchPlacer Placer(Draw.LocalToClip, Emitter.GetGuardX(), Emitter.GetGuardY());
	// The primitive the emitter was last started with in this section: a strip or independent triangles.
	EBatchPlacement Started = EBatchPlacement::Outside;
	for (uint32 BatchIndex = Section.FirstBatch; BatchIndex < Section.FirstBatch + Section.NumBatches; ++BatchIndex)
	{
		const FLPS2Batch& LPS2Batch = Mesh.GetBatch(int32(BatchIndex));
		EBatchPlacement Placement = EBatchPlacement::Outside;
		if (bSkinned)
		{
			// The palette of the pose, and the sphere it keeps the batch in.
			FVector PoseCenter = FVector::ZeroVector;
			float PoseRadius = 0.0f;
			MakeSkinPalette(Mesh, LPS2Batch, *SkinMatrices, PoseCenter, PoseRadius);
			Placement = Placer.Place(PoseCenter, PoseRadius);
		}
		else
		{
			Placement = Placer.Place(LPS2Batch);
		}
		if (Placement == EBatchPlacement::Outside)
		{
			continue;
		}
		FGSVertexBatch Batch;
		Batch.NumVertices = LPS2Batch.NumVertices;
		Batch.Positions = Mesh.GetPositions(LPS2Batch);
		Batch.Normals = Mesh.GetNormals(LPS2Batch);
		Batch.Colors = BakedLighting != nullptr ? BakedLighting->GetBatchColors(Mesh, int32(BatchIndex))
												: Mesh.GetColors(LPS2Batch);
		Batch.TexCoords = Mesh.GetTexCoords(LPS2Batch);
		Batch.TexCoordOffset[0] = LPS2Batch.TexCoordOffset[0];
		Batch.TexCoordOffset[1] = LPS2Batch.TexCoordOffset[1];
		if (bSkinned)
		{
			Batch.Skin = Mesh.GetSkin(LPS2Batch);
			Batch.Palette = BatchPalette.GetData();
			Batch.NumBones = uint32(BatchPalette.Num());
		}
		if (Placement == EBatchPlacement::Inside)
		{
			if (bRecordBatches)
			{
				if (DrawIndex == INDEX_NONE)
				{
					DrawIndex = List.AddVertexDraw(Draw);
				}
				Batch.Draw = DrawIndex;
				if (bSkinned)
				{
					// The palette where the list keeps it until its chain has been sent, once for batches that share
					// it.
					if (RecordedPalette == nullptr || RecordedBones != Batch.NumBones ||
						FMemory::Memcmp(RecordedPalette, Batch.Palette, Batch.NumBones * sizeof(FGSSkinMatrix)) != 0)
					{
						FGSSkinMatrix* Copy = List.AllocateSkinPalette(Batch.NumBones);
						FMemory::Memcpy(Copy, Batch.Palette, Batch.NumBones * sizeof(FGSSkinMatrix));
						RecordedPalette = Copy;
						RecordedBones = Batch.NumBones;
					}
					Batch.Palette = RecordedPalette;
				}
				List.DrawVertexBatch(Batch);
				NumBatchTriangles += CountStripTriangles(Batch);
				++FrameStats.BatchesOnVU1;
				// The batch sets PRIM itself: a run of the emitter after it starts again.
				Started = EBatchPlacement::Outside;
				continue;
			}
			SCOPE_CYCLE_COUNTER(STAT_GSEmittedBatches);
			if (Started != EBatchPlacement::Inside)
			{
				Emitter.BeginStrip(State.bTextured, State.bTranslucent, true);
				Started = EBatchPlacement::Inside;
			}
			Emitter.AddVertexBatch(Draw, Batch);
			++FrameStats.BatchesOnEmitter;
			continue;
		}
		// Across a clip plane: each triangle of the strips on its own, in the source's winding.
		SCOPE_CYCLE_COUNTER(STAT_GSClippedBatches);
		++FrameStats.BatchesClipped;
		const int32 NumVertices = int32(Batch.NumVertices);
		BatchVertices.SetNumUninitialized(NumVertices, false);
		BatchTriangles.SetNumUninitialized(NumVertices, false);
		FGSPrimitiveEmitter::TransformVertexBatch(Draw, Batch, BatchVertices.GetData(), BatchTriangles.GetData());
		if (Started != EBatchPlacement::Crossing)
		{
			Emitter.BeginTriangles(State.bTextured, State.bTranslucent, true);
			Started = EBatchPlacement::Crossing;
		}
		for (int32 Index = 2; Index < NumVertices; ++Index)
		{
			if (BatchTriangles[Index] == EGSStripTriangle::None)
			{
				continue;
			}
			const bool bReversed = BatchTriangles[Index] == EGSStripTriangle::Reversed;
			++FrameStats.TrianglesClipped;
			Emitter.AddTriangle(BatchVertices[bReversed ? Index - 1 : Index - 2],
				BatchVertices[bReversed ? Index - 2 : Index - 1], BatchVertices[Index]);
		}
	}
	if (State.bTranslucent)
	{
		SetDepthWrite(List, Environment, true);
	}
	++FrameStats.DrawsSubmitted;
}

void FGSSceneRenderer::SetDepthWrite(FGSCommandList& List, const FGSDrawEnvironment& Environment, bool bWrite)
{
	FGSZBuf ZBuf = Environment.ZBuf;
	ZBuf.bMask = !bWrite;
	List.SetZBuf(0, ZBuf);
}

void FGSSceneRenderer::Render(
	const FSceneViewFamily& ViewFamily, const FGSDrawEnvironment& Environment, FGSCommandList& List)
{
	SCOPE_CYCLE_COUNTER(STAT_GSSceneRender);
	LLM_SCOPE(ELLMTag::SceneRender);
	FrameStats = FFrameStats();
	TextureCache.BeginFrame();
	// Whatever wrote the list before (another frame, the canvas) may have changed the texture registers.
	TextureState = FTextureState();

	// The clear: the background colour, and Z 0 (the farthest) everywhere.
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	FGSRGBAQ Background;
	Background.R = UnitByte(ClearColor.R);
	Background.G = UnitByte(ClearColor.G);
	Background.B = UnitByte(ClearColor.B);
	AddFrameSprite(List, Environment, Background, 0);
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));

	if (ViewFamily.Views.Num() == 0 || ViewFamily.Views[0] == nullptr)
	{
		return;
	}
	const FSceneView& View = *ViewFamily.Views[0];
	FScene* Scene = ViewFamily.Scene != nullptr ? ViewFamily.Scene->GetRenderScene() : nullptr;

	// The ambient light of what is lit per frame: the map's environment, as its static lighting was baked with.
	const UWorld* SceneWorld = Scene != nullptr ? Scene->GetWorld() : nullptr;
	const AWorldSettings* WorldSettings = SceneWorld != nullptr ? SceneWorld->GetWorldSettings() : nullptr;
	FrameLights = FGSVertexLights();
	FrameLights.Ambient = WorldSettings != nullptr ? WorldSettings->LightmassSettings.GetEnvironmentLight()
												   : FLightmassWorldInfoSettings().GetEnvironmentLight();
	NumBatchTriangles = 0;
	UWorld* World = Scene != nullptr ? Scene->GetWorld() : nullptr;
	const float WorldTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (World != nullptr)
	{
		// The skeletal meshes' update rate goes by the distance to the views drawn (UE:
		// ViewLocationsRenderedLastFrame).
		World->ViewLocationsRenderedLastFrame.Reset();
		World->ViewLocationsRenderedLastFrame.Add(View.ViewLocation);
	}

	// The frame's lists and the emitter's scratch live on the scratchpad (the frame's stack what does not fit) until
	// the scene is recorded (N15).
	FMemMark EmitterMark(FMemStack::Get());
	FScratchpadMark ScratchpadMark;
	FSceneRenderList<const FStaticMeshSceneProxy*> WorldMeshes;
	FSceneRenderList<const FStaticMeshSceneProxy*> ViewModelMeshes;
	FSceneRenderList<const FPrimitiveSceneInfo*> WorldSkeletalMeshes;
	FSceneRenderList<const FPrimitiveSceneInfo*> ViewModelSkeletalMeshes;
	const FMatrix ViewProjection = View.ViewMatrix * View.ProjectionMatrix;
	// The cells the view sees through the map's portals (everything without cells, N15).
	FVisibilityCellGraph::FVisibleCells VisibleCells;
	if (Scene != nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_GSVisibility);
		const FVisibilityCellGraph& Cells = Scene->GetVisibilityCells();
		Cells.FindVisibleCells(ViewProjection, View.ViewLocation, VisibleCells);
		if (VisibleCells.EyeCell != INDEX_NONE)
		{
			for (int32 Cell = 0; Cell < Cells.GetCells().Num(); ++Cell)
			{
				FrameStats.CellsVisible += (VisibleCells.Mask >> uint32(Cell)) & 1u ? 1 : 0;
			}
		}
		FrameStats.ObjectsCulledByCells = Scene->GatherPrimitives(
			View, VisibleCells, WorldMeshes, ViewModelMeshes, WorldSkeletalMeshes, ViewModelSkeletalMeshes);
		static_assert(
			MaxDirectionalLights <= FGSVertexLights::MaxDirectional && MaxPointLights <= FGSVertexLights::MaxPoint,
			"The vertex lights hold the renderer's lights");
		for (const FLightSceneInfo& Info : Scene->GetLights())
		{
			const FLightSceneProxy* Light = Info.Proxy.Get();
			if (Light->GetLightType() == ELightSceneProxyType::Directional)
			{
				if (FrameLights.NumDirectional < MaxDirectionalLights)
				{
					FGSVertexLights::FDirectional& Directional = FrameLights.Directional[FrameLights.NumDirectional++];
					Directional.Direction = Light->GetDirection();
					Directional.Color = Light->GetColor();
				}
			}
			else if (FrameLights.NumPoint < MaxPointLights)
			{
				FGSVertexLights::FPoint& Point = FrameLights.Point[FrameLights.NumPoint++];
				Point.Position = Light->GetPosition();
				Point.Color = Light->GetColor();
				Point.Radius = Light->GetRadius();
			}
		}
	}

	// The world settings' distance fog (N15): FOGCOL for the frame, a coefficient per vertex of the world pass.
	FrameFog = FGSVertexFog();
	// The sky's cube map (ps2-polish P8), whose horizon the fog fades into.
	const UTextureCube* Sky = WorldSettings != nullptr ? WorldSettings->SkySettings.SkyCubemap : nullptr;
	if (Sky != nullptr && !Sky->HasValidFaces())
	{
		Sky = nullptr;
	}
	if (WorldSettings != nullptr && WorldSettings->FogSettings.bEnableFog)
	{
		const FWorldFogSettings& Fog = WorldSettings->FogSettings;
		FrameFog = FGSVertexFog::MakeLinear(Fog.StartDistance, Fog.EndDistance);
		const FLinearColor FogColor =
			Fog.bInscatteringColorFromSky && Sky != nullptr ? Sky->HorizonColor : Fog.FogInscatteringColor;
		FGSFogCol FogCol;
		FogCol.R = UnitByte(FogColor.R);
		FogCol.G = UnitByte(FogColor.G);
		FogCol.B = UnitByte(FogColor.B);
		List.SetFogCol(FogCol);
	}

	// The frustum's planes four to a quadword: VU0 tests a box against four at once on the PS2 (N15).
	FFrustum Frustum;
	Frustum.ExtractFromViewProjection(View.ViewMatrix * ToGLClipSpace(View.ProjectionMatrix));
	FGSPrimitiveEmitter Emitter(Environment, List);
	Emitter.SetFog(FrameFog);
	PixelsPerCm = GetPixelsPerCm(View.ProjectionMatrix, Environment);

	// The sky, over the clear and under everything else.
	if (Sky != nullptr)
	{
		DrawSky(*Sky, View, ViewProjection, Emitter, List, Environment);
	}

	// Opaque sections grouped by texture, the groups in the order their first section comes in the scene (the same
	// order every run: no addresses decide it) and the scene's order within a group; translucent ones afterwards,
	// back to front. Each at its mesh's LOD.
	struct FTranslucentSection
	{
		const FStaticMeshSceneProxy* Proxy = nullptr;
		int32 Section = 0;
		int32 LOD = 0;
		float DistanceSquared = 0.0f;
	};
	FSceneRenderList<FOpaqueSection> OpaqueSections;
	OpaqueSections.Reserve(WorldMeshes.Num() * 2);
	FSceneRenderList<FTranslucentSection> Translucent;
	// What casts a blob shadow among the drawn primitives.
	FSceneRenderList<const FPrimitiveSceneProxy*> BlobShadowCasters;
	TextureGroups.Reset();
	{
		SCOPE_CYCLE_COUNTER(STAT_GSOpaque);
		for (const FStaticMeshSceneProxy* Proxy : WorldMeshes)
		{
			if (!Proxy->IsShown() || !Proxy->GetStaticMesh().HasValidRenderData())
			{
				continue;
			}
			++FrameStats.ObjectsTotal;
			const FBox Bounds = Proxy->GetWorldBounds();
			if (!Frustum.IntersectsAabb(Bounds))
			{
				++FrameStats.ObjectsCulled;
				continue;
			}
			++FrameStats.ObjectsVisible;
			const int32 LOD = GetStaticMeshLOD(*Proxy, Bounds, View);
			FrameStats.ObjectsAtLowerLOD += LOD > 0 ? 1 : 0;
			if (Proxy->CastsBlobShadow())
			{
				BlobShadowCasters.Add(Proxy);
			}
			for (int32 Section = 0; Section < Proxy->GetNumSections(); ++Section)
			{
				const FMaterial& Material = Proxy->GetSectionMaterial(Section);
				if (Material.IsTransparent())
				{
					Translucent.Add({Proxy, Section, LOD, FVector::DistSquared(Bounds.GetCenter(), View.ViewLocation)});
					continue;
				}
				const int32* Group = TextureGroups.Find(Material.AlbedoMap);
				const int32 GroupIndex =
					Group != nullptr ? *Group : TextureGroups.Add(Material.AlbedoMap, TextureGroups.Num());
				OpaqueSections.Add({Proxy, Section, LOD, GroupIndex});
			}
		}
		OpaqueSections.StableSort([](const FOpaqueSection& A, const FOpaqueSection& B) { return A.Group < B.Group; });
		for (const FOpaqueSection& Item : OpaqueSections)
		{
			DrawStaticSection(Emitter, *Item.Proxy, Item.Section, Item.LOD, ViewProjection, List, Environment);
		}
	}

	// The skinned meshes, culled by their pose's bounds, skinned batch by batch from their LPS2 v2 render data; the
	// drawn ones are stamped for their pose's throttling.
	{
		SCOPE_CYCLE_COUNTER(STAT_GSSkinned);
		for (const FPrimitiveSceneInfo* Info : WorldSkeletalMeshes)
		{
			const FSkeletalMeshSceneProxy& Skeletal = *static_cast<const FSkeletalMeshSceneProxy*>(Info->Proxy.Get());
			if (!Skeletal.GetSkeletalMesh().HasValidRenderData())
			{
				continue;
			}
			++FrameStats.ObjectsTotal;
			if (!Frustum.IntersectsAabb(Skeletal.GetWorldBounds()))
			{
				++FrameStats.ObjectsCulled;
				continue;
			}
			++FrameStats.ObjectsVisible;
			Info->Component->LastRenderTime = WorldTime;
			if (Skeletal.CastsBlobShadow())
			{
				BlobShadowCasters.Add(&Skeletal);
			}
			DrawSkeletalMesh(Emitter, Skeletal, ViewProjection, List, Environment);
		}
	}

	// The blob shadows on the floors, under the marks and what is translucent.
	{
		SCOPE_CYCLE_COUNTER(STAT_GSTranslucent);
		DrawBlobShadows(BlobShadowCasters, ViewProjection, Emitter, List, Environment);

		DrawImpactMarks(ViewFamily, ViewProjection, Emitter, List, Environment);

		Translucent.StableSort([](const FTranslucentSection& A, const FTranslucentSection& B)
			{ return A.DistanceSquared > B.DistanceSquared; });
		// A mesh's sections share its distance, so the stable sort keeps them in the mesh's order.
		for (const FTranslucentSection& Item : Translucent)
		{
			DrawStaticSection(Emitter, *Item.Proxy, Item.Section, Item.LOD, ViewProjection, List, Environment);
		}

		// The effect sprites (smoke), blended over the translucent meshes, farthest first.
		DrawEffectSprites(ViewFamily, View, ViewProjection, Emitter, List, Environment);

		// The tracers glow over everything drawn so far.
		DrawTracers(ViewFamily, View, ViewProjection, Emitter, List, Environment);

		DrawWorldLines(ViewFamily, ViewProjection, Emitter, List);
		DrawShowFlags(ViewFamily, View, ViewProjection, WorldMeshes, Emitter, List);
	}

	// The view model pass: Z cleared (the frame buffer masked), then the view model meshes with their projection,
	// culled by its frustum (N15), not fogged: static (a weapon) and skinned (first-person arms, by their pose's
	// bounds).
	FrameFog = FGSVertexFog();
	Emitter.SetFog(FrameFog);
	if (ViewModelMeshes.Num() > 0 || ViewModelSkeletalMeshes.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_GSViewModel);
		FGSFrame Masked = Environment.Frame;
		Masked.FBMSK = 0xffffffffu;
		List.SetFrame(0, Masked);
		List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
		AddFrameSprite(List, Environment, FGSRGBAQ(), 0);
		List.SetFrame(0, Environment.Frame);
		List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
		const FMatrix ViewModelProjection = View.ViewMatrix * View.ViewModelProjectionMatrix;
		FFrustum ViewModelFrustum;
		ViewModelFrustum.ExtractFromViewProjection(View.ViewMatrix * ToGLClipSpace(View.ViewModelProjectionMatrix));
		PixelsPerCm = GetPixelsPerCm(View.ViewModelProjectionMatrix, Environment);
		for (const FStaticMeshSceneProxy* Proxy : ViewModelMeshes)
		{
			if (!Proxy->GetStaticMesh().HasValidRenderData())
			{
				continue;
			}
			++FrameStats.ObjectsTotal;
			if (!ViewModelFrustum.IntersectsAabb(Proxy->GetWorldBounds()))
			{
				++FrameStats.ObjectsCulled;
				continue;
			}
			++FrameStats.ObjectsVisible;
			for (int32 Section = 0; Section < Proxy->GetNumSections(); ++Section)
			{
				DrawStaticSection(Emitter, *Proxy, Section, 0, ViewModelProjection, List, Environment);
			}
		}
		for (const FPrimitiveSceneInfo* Info : ViewModelSkeletalMeshes)
		{
			const FSkeletalMeshSceneProxy& Skeletal = *static_cast<const FSkeletalMeshSceneProxy*>(Info->Proxy.Get());
			if (!Skeletal.GetSkeletalMesh().HasValidRenderData())
			{
				continue;
			}
			++FrameStats.ObjectsTotal;
			if (!ViewModelFrustum.IntersectsAabb(Skeletal.GetWorldBounds()))
			{
				++FrameStats.ObjectsCulled;
				continue;
			}
			++FrameStats.ObjectsVisible;
			Info->Component->LastRenderTime = WorldTime;
			DrawSkeletalMesh(Emitter, Skeletal, ViewModelProjection, List, Environment);
		}
	}
	FrameStats.TrianglesSubmitted = Emitter.GetNumTriangles() + NumBatchTriangles;
	// The EE's writes: a vertex batch's are VU1's.
	FrameStats.RegisterWrites = List.GetWrites().Num() - List.GetVertexBatches().Num();
	const FGSTextureCache::FFrameCounters& Counters = TextureCache.GetFrameCounters();
	FrameStats.TextureUploads = Counters.Uploads;
	FrameStats.TextureUploadBytes = Counters.UploadBytes;
	FrameStats.TextureEvictions = Counters.Evictions;
	FrameStats.ClutLoads = Counters.ClutLoads;
	FrameStats.TextureResidentBytes = int32(TextureCache.GetResidentBlocks() * FGSTextureLayout::BytesPerBlock);
}

void FGSSceneRenderer::DrawSky(const UTextureCube& Sky, const FSceneView& View, const FMatrix& ViewProjection,
	FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment)
{
	SCOPE_CYCLE_COUNTER(STAT_GSSky);
	if (SkyMesh.IsEmpty() && !FSkyBoxGeometry::BuildMesh(SkyMesh))
	{
		return;
	}
	// Behind everything: the depth test always passes and Z is not written; not fogged.
	const FGSVertexFog WorldFog = FrameFog;
	FrameFog = FGSVertexFog();
	Emitter.SetFog(FrameFog);
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	SetDepthWrite(List, Environment, false);
	// Around the eye, never moving with it.
	const float Radius = FSkyBoxGeometry::GetRadius(View.ProjectionMatrix);
	const FMatrix LocalToWorld = FScaleMatrix(FVector(Radius)) * FTranslationMatrix(View.ViewLocation);
	const FBox Bounds = FBox(FVector(-Radius), FVector(Radius)).ShiftBy(View.ViewLocation);
	for (int32 Section = 0; Section < SkyMesh.GetNumSections(); ++Section)
	{
		// The face unlit, its texels as they are (MODULATE by white), level 0 bilinear, clamped (its texture's address
		// modes).
		FMaterial Face;
		Face.Shading = EMaterialLightingModel::Unlit;
		Face.Albedo = FVector::OneVector;
		Face.AlbedoMap = Sky.GetFace(ECubeFace(SkyMesh.GetSection(Section).MaterialIndex));
		Face.bMipmaps = false;
		DrawMeshSection(Emitter, SkyMesh, Section, LocalToWorld, Bounds, Face, /*bStaticLighting =*/false, nullptr,
			nullptr, ViewProjection, List, Environment);
	}
	SetDepthWrite(List, Environment, true);
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
	FrameFog = WorldFog;
	Emitter.SetFog(FrameFog);
}

void FGSSceneRenderer::DrawSkeletalMesh(FGSPrimitiveEmitter& Emitter, const FSkeletalMeshSceneProxy& Skeletal,
	const FMatrix& ViewProjection, FGSCommandList& List, const FGSDrawEnvironment& Environment)
{
	const FLPS2Mesh& Mesh = Skeletal.GetSkeletalMesh().GetRenderData();
	for (int32 Section = 0; Section < Mesh.GetNumSections(); ++Section)
	{
		DrawMeshSection(Emitter, Mesh, Section, Skeletal.GetLocalToWorld(), Skeletal.GetWorldBounds(),
			Skeletal.GetSectionMaterial(Section),
			/*bStaticLighting =*/false, nullptr, &Skeletal.GetBoneMatrices(), ViewProjection, List, Environment);
	}
}

void FGSSceneRenderer::DrawShowFlags(const FSceneViewFamily& ViewFamily, const FSceneView& View,
	const FMatrix& ViewProjection, TArrayView<const FStaticMeshSceneProxy* const> WorldMeshes,
	FGSPrimitiveEmitter& Emitter, FGSCommandList& List)
{
	const FEngineShowFlags& ShowFlags = ViewFamily.EngineShowFlags;
	FDebugDraw Lines;
	// F1: the static meshes' world boxes, depth tested (hidden ones, as blocking volumes, in magenta).
	if (ShowFlags.Bounds)
	{
		constexpr FLinearColor BoxColor(0.2f, 0.95f, 0.35f);
		constexpr FLinearColor HiddenBoxColor(0.95f, 0.35f, 0.85f);
		for (const FStaticMeshSceneProxy* Proxy : WorldMeshes)
		{
			if (Proxy->GetStaticMesh().HasValidRenderData())
			{
				const FBox Box = Proxy->GetWorldBounds();
				Lines.AddAabb(Box.Min, Box.Max, Proxy->IsShown() ? BoxColor : HiddenBoxColor);
			}
		}
		DrawDebugLines(Lines, ViewProjection, Emitter);
	}
	if (!ShowFlags.AxesGizmo)
	{
		return;
	}
	// F6: 1 m world axes at the origin over everything, and the view's orientation in a square in the bottom-left
	// corner (the view's axes in its own normalized coordinates, moved into the square).
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	Lines.Clear();
	Lines.AddAxes(FVector::ZeroVector);
	DrawDebugLines(Lines, ViewProjection, Emitter);
	constexpr float GizmoSize = 48.0f;
	constexpr float GizmoMargin = 6.0f;
	const float Width = float(ViewFamily.RenderTargetSizeX > 0 ? ViewFamily.RenderTargetSizeX : 640);
	const float Height = float(ViewFamily.RenderTargetSizeY > 0 ? ViewFamily.RenderTargetSizeY : 448);
	const float CenterX = (((GizmoMargin + (GizmoSize * 0.5f)) / Width) * 2.0f) - 1.0f;
	const float CenterY = (((GizmoMargin + (GizmoSize * 0.5f)) / Height) * 2.0f) - 1.0f;
	const FMatrix ToGizmo(FPlane(GizmoSize / Width, 0.0f, 0.0f, 0.0f), FPlane(0.0f, GizmoSize / Height, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 1.0f, 0.0f), FPlane(CenterX, CenterY, 0.5f, 1.0f));
	Lines.Clear();
	Lines.AddViewAxes(View.ViewMatrix);
	DrawDebugLines(Lines, ToGizmo, Emitter);
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
}

void FGSSceneRenderer::DrawDebugLines(const FDebugDraw& Lines, const FMatrix& ToClip, FGSPrimitiveEmitter& Emitter)
{
	const TArray<FDebugDraw::FLineVertex>& Vertices = Lines.GetVertices();
	if (Vertices.Num() < 2)
	{
		return;
	}
	Emitter.BeginLines(false);
	for (int32 Index = 0; Index + 1 < Vertices.Num(); Index += 2)
	{
		FGSClipVertex Ends[2];
		for (int32 End = 0; End < 2; ++End)
		{
			const FDebugDraw::FLineVertex& Line = Vertices[Index + End];
			Ends[End].Clip = ToClip.TransformPosition(Line.Position);
			Ends[End].Color = FLinearColor(Line.Color.X, Line.Color.Y, Line.Color.Z, 1.0f);
		}
		Emitter.AddLine(Ends[0], Ends[1]);
	}
}

void FGSSceneRenderer::DrawBlobShadows(TArrayView<const FPrimitiveSceneProxy* const> Casters,
	const FMatrix& ViewProjection, FGSPrimitiveEmitter& Emitter, FGSCommandList& List,
	const FGSDrawEnvironment& Environment)
{
	if (Casters.Num() == 0)
	{
		return;
	}
	// Two triangles a shadow on the frame's stack, placed on the floor its component traced (N15).
	FMemMark Mark(FMemStack::Get());
	TArray<FWorldEffectVertex, TMemStackAllocator<>> Vertices;
	Vertices.Reserve(Casters.Num() * 6);
	for (const FPrimitiveSceneProxy* Proxy : Casters)
	{
		FVector FloorPoint;
		FVector FloorNormal;
		FWorldEffectsGeometry::FBlobShadow Shadow;
		if (Proxy->GetBlobShadowFloor(FloorPoint, FloorNormal) &&
			FWorldEffectsGeometry::PlaceBlobShadow(Proxy->GetWorldBounds(), FloorPoint, FloorNormal, Shadow))
		{
			FWorldEffectsGeometry::AddBlobShadowVertices(Shadow, Vertices);
		}
	}
	if (Vertices.Num() == 0 || !BindEffectsMask(List))
	{
		return;
	}
	// Black through the mask's round spot as strongly as the opacity (the default translucent blend), a little nearer
	// than the floor, not writing Z (as the impact marks).
	SetDepthWrite(List, Environment, false);
	Emitter.SetDepthBias(DecalDepthBias);
	DrawEffectVertices(Emitter, Vertices, ViewProjection, true);
	Emitter.SetDepthBias(0);
	SetDepthWrite(List, Environment, true);
	FrameStats.BlobShadows += Vertices.Num() / 6;
}

void FGSSceneRenderer::DrawImpactMarks(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection,
	FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment)
{
	UWorld* World = ViewFamily.Scene != nullptr ? ViewFamily.Scene->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}
	// The frame's vertices on the frame's stack.
	FMemMark Mark(FMemStack::Get());
	TArray<FWorldEffectVertex, TMemStackAllocator<>> Vertices;
	FWorldEffectsGeometry::BuildImpactMarkVertices(World->ImpactMarks, Vertices);
	if (Vertices.Num() == 0)
	{
		return;
	}
	// The mark's colour at the spot's centre, as strongly as its opacity, over the surface (a lerp: the GS cannot
	// multiply by the destination), a little nearer than the surface.
	if (!BindEffectsMask(List))
	{
		return;
	}
	SetDepthWrite(List, Environment, false);
	Emitter.SetDepthBias(DecalDepthBias);
	DrawEffectVertices(Emitter, Vertices, ViewProjection, true);
	Emitter.SetDepthBias(0);
	SetDepthWrite(List, Environment, true);
}

void FGSSceneRenderer::DrawTracers(const FSceneViewFamily& ViewFamily, const FSceneView& View,
	const FMatrix& ViewProjection, FGSPrimitiveEmitter& Emitter, FGSCommandList& List,
	const FGSDrawEnvironment& Environment)
{
	UWorld* World = ViewFamily.Scene != nullptr ? ViewFamily.Scene->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}
	FMemMark Mark(FMemStack::Get());
	TArray<FWorldEffectVertex, TMemStackAllocator<>> Vertices;
	FWorldEffectsGeometry::BuildTracerVertices(World->Tracers, View.ViewLocation, Vertices);
	if (Vertices.Num() == 0)
	{
		return;
	}
	// Added (Cs * As + Cd), fading across the ribbon through the mask; unmasked without a texture arena; never fogged
	// (the fog's colour would be added too).
	const bool bMasked = BindEffectsMask(List);
	List.SetAlpha(0, FGSAlpha::Additive());
	SetDepthWrite(List, Environment, false);
	Emitter.SetFog(FGSVertexFog());
	DrawEffectVertices(Emitter, Vertices, ViewProjection, bMasked);
	Emitter.SetFog(FrameFog);
	SetDepthWrite(List, Environment, true);
	List.SetAlpha(0, FGSAlpha::Translucent());
}

void FGSSceneRenderer::DrawEffectSprites(const FSceneViewFamily& ViewFamily, const FSceneView& View,
	const FMatrix& ViewProjection, FGSPrimitiveEmitter& Emitter, FGSCommandList& List,
	const FGSDrawEnvironment& Environment)
{
	UWorld* World = ViewFamily.Scene != nullptr ? ViewFamily.Scene->GetWorld() : nullptr;
	if (World == nullptr || World->EffectSprites.IsEmpty())
	{
		return;
	}
	FMemMark Mark(FMemStack::Get());
	TArray<FWorldEffectVertex, TMemStackAllocator<>> Vertices;
	FWorldEffectsGeometry::BuildEffectSpriteVertices(
		World->EffectSprites, View.ViewMatrix, View.ViewLocation, Vertices);
	// The sprite's colour through the mask's round spot, as strongly as its opacity, over the scene (the default
	// translucent blend, as the impact marks'), depth tested, not written.
	if (Vertices.Num() == 0 || !BindEffectsMask(List))
	{
		return;
	}
	SetDepthWrite(List, Environment, false);
	DrawEffectVertices(Emitter, Vertices, ViewProjection, true);
	SetDepthWrite(List, Environment, true);
}

bool FGSSceneRenderer::BindEffectsMask(FGSCommandList& List)
{
	// The effects' mask with the spot moved to the alpha (MODULATE takes the colour from the vertex), built once: it
	// never changes, and the cache uploads it again only after a reset.
	if (EffectsMaskTexels.Num() == 0)
	{
		FWorldEffectsGeometry::BuildMaskTexels(EffectsMaskTexels);
		for (int32 Index = 0; Index < EffectsMaskTexels.Num(); Index += 4)
		{
			EffectsMaskTexels[Index + 3] = EffectsMaskTexels[Index];
			EffectsMaskTexels[Index + 0] = 255;
			EffectsMaskTexels[Index + 1] = 255;
			EffectsMaskTexels[Index + 2] = 255;
		}
	}
	if (TextureState.Texture != &GEffectsMaskKey)
	{
		FGSTex0 Mask;
		if (!TextureCache.BindTexels(&GEffectsMaskKey, FWorldEffectsGeometry::MaskSize, FWorldEffectsGeometry::MaskSize,
				EffectsMaskTexels, true, List, Mask))
		{
			return false;
		}
		List.SetTex0(0, Mask);
		++FrameStats.Tex0Writes;
		TextureState.Texture = &GEffectsMaskKey;
		TextureState.Binding = FGSTextureBinding();
		TextureState.Binding.Tex0 = Mask;
	}
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	Clamp.WMT = EGSWrapMode::Clamp;
	SetSampler(List, BilinearSampling(), Clamp);
	return true;
}

void FGSSceneRenderer::DrawEffectVertices(FGSPrimitiveEmitter& Emitter, TArrayView<const FWorldEffectVertex> Vertices,
	const FMatrix& ViewProjection, bool bTextured)
{
	Emitter.BeginTriangles(bTextured, true, false);
	for (int32 Index = 0; Index + 2 < Vertices.Num(); Index += 3)
	{
		FGSClipVertex Corners[3];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const FWorldEffectVertex& Effect = Vertices[Index + Corner];
			Corners[Corner].Clip = ViewProjection.TransformPosition(Effect.Position);
			Corners[Corner].Color = Effect.Color;
			Corners[Corner].U = Effect.TexCoord.X;
			Corners[Corner].V = Effect.TexCoord.Y;
		}
		Emitter.AddTriangle(Corners[0], Corners[1], Corners[2]);
	}
}

void FGSSceneRenderer::DrawWorldLines(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection,
	FGSPrimitiveEmitter& Emitter, FGSCommandList& List)
{
	(void)List;
	UWorld* World = ViewFamily.Scene != nullptr ? ViewFamily.Scene->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}
	// The frame's debug lines, depth tested; the batch is emptied once drawn (UE: the line batcher's lines last a
	// frame).
	DrawDebugLines(World->LineBatcher, ViewProjection, Emitter);
	World->LineBatcher.Clear();
}

bool FGSSceneRenderer::BindCanvasTexture(const UTexture2D& Texture, FGSCommandList& List)
{
	if (TextureState.Texture != &Texture)
	{
		FGSTextureBinding Binding;
		// Not resident this frame (the upload budget, a full arena): the run waits for a later frame rather than
		// drawing its texels' average over its rectangles.
		if (!TextureCache.BindTexture(Texture, List, Binding) || Binding.bFlat)
		{
			return false;
		}
		List.SetTex0(0, Binding.Tex0);
		++FrameStats.Tex0Writes;
		TextureState.Texture = &Texture;
		TextureState.Binding = Binding;
	}
	return true;
}

void FGSSceneRenderer::DrawCanvas(const FCanvas& Canvas, const FGSDrawEnvironment& Environment, FGSCommandList& List)
{
	SCOPE_CYCLE_COUNTER(STAT_GSDrawCanvas);
	LLM_SCOPE(ELLMTag::SceneRender);
	if (Canvas.IsEmpty())
	{
		return;
	}
	// Kept between frames: the canvas's vertices reuse their capacity.
	TArray<FCanvasVertex>& Vertices = CanvasVertices;
	Canvas.GetPrimitives(Vertices, CanvasRuns);
	// Pixel coordinates (top-left origin) blended by their alpha over the frame, without the depth test or Z writes:
	// the rectangles as SPRITEs (two vertices, the colour flat), the rest as Gouraud triangles (N15). A textured run
	// samples its texture by UV (texels; the texture's rows are stored bottom first, so V turns), MODULATE by the
	// vertex colour, clamped, nearest when its texels map to pixels one to one (glyphs), bilinear otherwise.
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	SetDepthWrite(List, Environment, false);
	FGSRGBAQ Last;
	bool bHasLast = false;
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	Clamp.WMT = EGSWrapMode::Clamp;
	FGSTex1 Nearest;
	Nearest.bFixedLOD = true;
	for (const FCanvasPrimitiveRun& Run : CanvasRuns)
	{
		const bool bTextured = Run.Texture != nullptr;
		if (bTextured)
		{
			if (!BindCanvasTexture(*Run.Texture, List))
			{
				continue;
			}
			SetSampler(List, Run.bNearest ? Nearest : BilinearSampling(), Clamp);
		}
		FGSPrim Prim;
		Prim.Type = Run.Type == ECanvasPrimitive::Rectangle ? EGSPrimitive::Sprite : EGSPrimitive::Triangle;
		Prim.bGouraud = Run.Type == ECanvasPrimitive::Triangle;
		Prim.bAlphaBlend = true;
		Prim.bTextured = bTextured;
		Prim.bUseUV = bTextured;
		List.SetPrim(Prim);
		// MODULATE takes 0x80 as 1.0; a flat colour is written as it is.
		const float ColorScale = bTextured ? 128.0f : 255.0f;
		const float TexelsU = bTextured ? float(Run.Texture->GetSizeX()) : 0.0f;
		const float TexelsV = bTextured ? float(Run.Texture->GetSizeY()) : 0.0f;
		// A run of vertices shares its colour (a label, a panel): it converts once per run, not per vertex.
		float LastR = -1.0f;
		float LastG = -1.0f;
		float LastB = -1.0f;
		float LastA = -1.0f;
		for (int32 Index = Run.FirstVertex; Index < Run.FirstVertex + Run.NumVertices; ++Index)
		{
			const FCanvasVertex& Vertex = Vertices[Index];
			if (Vertex.R != LastR || Vertex.G != LastG || Vertex.B != LastB || Vertex.A != LastA)
			{
				LastR = Vertex.R;
				LastG = Vertex.G;
				LastB = Vertex.B;
				LastA = Vertex.A;
				FGSRGBAQ Color;
				Color.R = uint8(FMath::Clamp(FMath::RoundToInt(Vertex.R * ColorScale), 0, 255));
				Color.G = uint8(FMath::Clamp(FMath::RoundToInt(Vertex.G * ColorScale), 0, 255));
				Color.B = uint8(FMath::Clamp(FMath::RoundToInt(Vertex.B * ColorScale), 0, 255));
				Color.A = uint8(FMath::Clamp(FMath::RoundToInt(Vertex.A * 128.0f), 0, 0x80));
				if (!bHasLast || Color.Encode() != Last.Encode())
				{
					List.SetRGBAQ(Color);
					Last = Color;
					bHasLast = true;
				}
			}
			if (bTextured)
			{
				FGSUV UV;
				UV.U = uint16(FMath::Clamp(FMath::RoundToInt(Vertex.U * TexelsU * 16.0f), 0, 0x3fff));
				UV.V = uint16(FMath::Clamp(FMath::RoundToInt((1.0f - Vertex.V) * TexelsV * 16.0f), 0, 0x3fff));
				List.SetUV(UV);
			}
			// OpenGL covers pixel i when i + 0.5 is inside; the GS samples pixel i at i.
			List.AddVertex(Environment.PixelVertex(Vertex.X - 0.5f, Vertex.Y - 0.5f, 0));
		}
	}
	SetDepthWrite(List, Environment, true);
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
}
