#include "GSSceneRenderer.h"

#include "CanvasTypes.h"
#include "Debug/DebugDraw.h"
#include "Effects/WorldEffects.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Frustum.h"
#include "GLClipSpace.h"
#include "Level/Light.h"
#include "LightSceneProxy.h"
#include "MaterialShared.h"
#include "ScenePrivate.h"
#include "SceneView.h"
#include "SkeletalMeshSceneProxy.h"
#include "StaticMeshResources.h"
#include "StaticMeshSceneProxy.h"
#include "WorldEffectsGeometry.h"

const FLinearColor FGSSceneRenderer::ClearColor(0.08f, 0.09f, 0.11f, 1.0f);

namespace
{

	/** The ambient share of the albedo. */
	constexpr float AmbientShare = 0.10f;

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

	/** World normal through the inverse transpose of LocalToWorld (UE: TransposeAdjoint, flipped when mirrored). */
	struct FNormalTransform
	{
		FMatrix Matrix;
		float Sign = 1.0f;

		explicit FNormalTransform(const FMatrix& LocalToWorld)
			: Matrix(LocalToWorld.TransposeAdjoint())
			, Sign(LocalToWorld.Determinant() < 0.0f ? -1.0f : 1.0f)
		{
		}

		[[nodiscard]] FVector Transform(const FVector& Normal) const
		{
			const FVector4 World = Matrix.TransformVector(Normal);
			return (FVector(World.X, World.Y, World.Z) * Sign).GetSafeNormal();
		}
	};

} // namespace

FVector FGSSceneRenderer::Irradiance(const FVector& Position, const FVector& Normal) const
{
	FVector Light(AmbientShare, AmbientShare, AmbientShare);
	for (const FLightSceneProxy* Directional : Lights.Directional)
	{
		const float NdL = FVector::DotProduct(Normal, -Directional->GetDirection());
		if (NdL > 0.0f)
		{
			Light += Directional->GetColor() * NdL;
		}
	}
	for (const FLightSceneProxy* Point : Lights.Point)
	{
		const FVector ToLight = Point->GetPosition() - Position;
		const float Distance = ToLight.Size();
		const float Range = FMath::Max(Point->GetRadius(), 0.1f);
		if (Distance >= Range || Distance <= 0.0f)
		{
			continue;
		}
		const float NdL = FVector::DotProduct(Normal, ToLight / Distance);
		if (NdL > 0.0f)
		{
			const float Attenuation = FMath::Square(1.0f - (Distance / Range));
			Light += Point->GetColor() * (NdL * Attenuation);
		}
	}
	return Light;
}

FGSSceneRenderer::FSectionState FGSSceneRenderer::BindMaterial(const FMaterial& Material, FGSCommandList& List)
{
	FSectionState State;
	State.bLit = Material.Shading == EMaterialLightingModel::BlinnPhong;
	State.bTranslucent = Material.IsTransparent();
	State.Albedo = FLinearColor(Material.Albedo.X, Material.Albedo.Y, Material.Albedo.Z, 1.0f);
	State.Alpha = FMath::Clamp(Material.Alpha, 0.0f, 1.0f);
	State.UvScale = Material.UvScale;
	FGSTex0 Tex0;
	if (Material.AlbedoMap != nullptr && TextureCache.BindTexture(*Material.AlbedoMap, List, Tex0))
	{
		List.SetTex1(0, BilinearSampling());
		List.SetClamp(0, FGSClamp());
		List.SetTex0(0, Tex0);
		State.bTextured = true;
	}
	return State;
}

void FGSSceneRenderer::TransformStaticMesh(const FStaticMeshSceneProxy& Proxy, const FMatrix& ViewProjection)
{
	const FStaticMeshLODResources& Resources = Proxy.GetStaticMesh().GetLODResources();
	const FMatrix& LocalToWorld = Proxy.GetLocalToWorld();
	const FMatrix LocalToClip = LocalToWorld * ViewProjection;
	const FNormalTransform NormalTransform(LocalToWorld);
	const int32 NumVertices = Resources.Vertices.Num();
	ClipPositions.SetNumUninitialized(NumVertices);
	Irradiances.SetNumUninitialized(NumVertices);
	MeshTexCoords.SetNumUninitialized(NumVertices);
	for (int32 Index = 0; Index < NumVertices; ++Index)
	{
		const FVertex& Vertex = Resources.Vertices[Index];
		ClipPositions[Index] = LocalToClip.TransformPosition(Vertex.Position);
		const FVector4 World = LocalToWorld.TransformPosition(Vertex.Position);
		Irradiances[Index] = Irradiance(FVector(World.X, World.Y, World.Z), NormalTransform.Transform(Vertex.Normal));
		MeshTexCoords[Index] = Vertex.TexCoord;
	}
}

void FGSSceneRenderer::TransformSkeletalMesh(const FSkeletalMeshSceneProxy& Proxy, const FMatrix& ViewProjection)
{
	const TArray<FSkeletalVertex>& Vertices = Proxy.GetSkeletalMesh().GetVertices();
	const TArray<FMatrix>& Bones = Proxy.GetBoneMatrices();
	const FMatrix& LocalToWorld = Proxy.GetLocalToWorld();
	const FMatrix LocalToClip = LocalToWorld * ViewProjection;
	const FNormalTransform NormalTransform(LocalToWorld);
	const int32 NumVertices = Vertices.Num();
	ClipPositions.SetNumUninitialized(NumVertices);
	Irradiances.SetNumUninitialized(NumVertices);
	MeshTexCoords.SetNumUninitialized(NumVertices);
	for (int32 Index = 0; Index < NumVertices; ++Index)
	{
		const FSkeletalVertex& Vertex = Vertices[Index];
		// Linear blend skinning: the weighted sum of the bones' transforms.
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		float TotalWeight = 0.0f;
		for (int32 Influence = 0; Influence < 4; ++Influence)
		{
			const float Weight = Vertex.BoneWeights[Influence];
			const int32 Bone = Vertex.BoneIndices[Influence];
			if (Weight <= 0.0f || !Bones.IsValidIndex(Bone))
			{
				continue;
			}
			const FVector4 Skinned = Bones[Bone].TransformPosition(Vertex.Position);
			const FVector4 SkinnedNormal = Bones[Bone].TransformVector(Vertex.Normal);
			Position += FVector(Skinned.X, Skinned.Y, Skinned.Z) * Weight;
			Normal += FVector(SkinnedNormal.X, SkinnedNormal.Y, SkinnedNormal.Z) * Weight;
			TotalWeight += Weight;
		}
		if (TotalWeight <= 0.0f)
		{
			Position = Vertex.Position;
			Normal = Vertex.Normal;
		}
		ClipPositions[Index] = LocalToClip.TransformPosition(Position);
		const FVector4 World = LocalToWorld.TransformPosition(Position);
		Irradiances[Index] = Irradiance(FVector(World.X, World.Y, World.Z), NormalTransform.Transform(Normal));
		MeshTexCoords[Index] = Vertex.TexCoord;
	}
}

void FGSSceneRenderer::DrawTriangles(FGSPrimitiveEmitter& Emitter, const TArray<uint32>& Indices, int32 First,
	int32 Count, const FSectionState& State, const TArray<FVector2D>& TexCoords)
{
	Emitter.BeginTriangles(State.bTextured, State.bTranslucent, true);
	FGSClipVertex Corners[3];
	const int32 End = FMath::Min(First + Count, Indices.Num());
	for (int32 Index = First; Index + 2 < End; Index += 3)
	{
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 VertexIndex = int32(Indices[Index + Corner]);
			if (!ClipPositions.IsValidIndex(VertexIndex))
			{
				return;
			}
			FGSClipVertex& Vertex = Corners[Corner];
			Vertex.Clip = ClipPositions[VertexIndex];
			const FVector& Light = Irradiances[VertexIndex];
			Vertex.Color = State.bLit ? FLinearColor(State.Albedo.R * Light.X, State.Albedo.G * Light.Y,
											State.Albedo.B * Light.Z, State.Alpha)
									  : FLinearColor(State.Albedo.R, State.Albedo.G, State.Albedo.B, State.Alpha);
			Vertex.U = TexCoords[VertexIndex].X * State.UvScale.X;
			Vertex.V = TexCoords[VertexIndex].Y * State.UvScale.Y;
		}
		Emitter.AddTriangle(Corners[0], Corners[1], Corners[2]);
	}
}

void FGSSceneRenderer::DrawStaticSection(FGSPrimitiveEmitter& Emitter, const FStaticMeshSceneProxy& Proxy,
	int32 SectionIndex, FGSCommandList& List, const FGSDrawEnvironment& Environment)
{
	const FStaticMeshLODResources& Resources = Proxy.GetStaticMesh().GetLODResources();
	const FMeshSection& Section = Resources.Sections.IsValidIndex(SectionIndex)
		? Resources.Sections[SectionIndex]
		: FMeshSection{0, Resources.Indices.Num(), 0};
	const FSectionState State = BindMaterial(Proxy.GetSectionMaterial(SectionIndex), List);
	if (State.bTranslucent)
	{
		SetDepthWrite(List, Environment, false);
	}
	DrawTriangles(Emitter, Resources.Indices, Section.IndexOffset, Section.IndexCount, State, MeshTexCoords);
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
	FrameStats = FFrameStats();
	TextureCache.ResetStats();

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

	Lights.Directional.Reset();
	Lights.Point.Reset();
	WorldMeshes.Reset();
	ViewModelMeshes.Reset();
	if (Scene != nullptr)
	{
		Scene->GatherStaticMeshes(View, WorldMeshes, ViewModelMeshes);
		for (const FLightSceneInfo& Info : Scene->GetLights())
		{
			const FLightSceneProxy* Light = Info.Proxy.Get();
			TArray<const FLightSceneProxy*>& Kind =
				Light->GetLightType() == ELightSceneProxyType::Directional ? Lights.Directional : Lights.Point;
			if (Kind.Num() < (&Kind == &Lights.Directional ? MaxDirectionalLights : MaxPointLights))
			{
				Kind.Add(Light);
			}
		}
	}

	const FMatrix ViewProjection = View.ViewMatrix * View.ProjectionMatrix;
	FFrustum Frustum;
	Frustum.ExtractFromViewProjection(View.ViewMatrix * ToGLClipSpace(View.ProjectionMatrix));
	FGSPrimitiveEmitter Emitter(Environment, List);

	// Opaque sections in the scene's order; translucent ones afterwards, back to front.
	struct FTranslucentSection
	{
		const FStaticMeshSceneProxy* Proxy = nullptr;
		int32 Section = 0;
		float DistanceSquared = 0.0f;
	};
	TArray<FTranslucentSection> Translucent;
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
		bool bTransformed = false;
		for (int32 Section = 0; Section < Proxy->GetNumSections(); ++Section)
		{
			if (Proxy->GetSectionMaterial(Section).IsTransparent())
			{
				Translucent.Add({Proxy, Section, FVector::DistSquared(Bounds.GetCenter(), View.ViewLocation)});
				continue;
			}
			if (!bTransformed)
			{
				TransformStaticMesh(*Proxy, ViewProjection);
				bTransformed = true;
			}
			DrawStaticSection(Emitter, *Proxy, Section, List, Environment);
		}
	}

	// The skinned meshes (one material each).
	if (Scene != nullptr)
	{
		for (const FPrimitiveSceneInfo& Info : Scene->GetPrimitives())
		{
			const FPrimitiveSceneProxy* Proxy = Info.Proxy.Get();
			if (Proxy->GetProxyType() != EPrimitiveSceneProxyType::SkeletalMesh)
			{
				continue;
			}
			const FSkeletalMeshSceneProxy& Skeletal = *static_cast<const FSkeletalMeshSceneProxy*>(Proxy);
			if (!Skeletal.IsShown(&View) || !Skeletal.GetSkeletalMesh().HasValidRenderData())
			{
				continue;
			}
			++FrameStats.ObjectsTotal;
			++FrameStats.ObjectsVisible;
			TransformSkeletalMesh(Skeletal, ViewProjection);
			const FSectionState State = BindMaterial(Skeletal.GetMaterial(), List);
			const TArray<uint32>& Indices = Skeletal.GetSkeletalMesh().GetIndices();
			DrawTriangles(Emitter, Indices, 0, Indices.Num(), State, MeshTexCoords);
			++FrameStats.DrawsSubmitted;
		}
	}

	DrawImpactMarks(ViewFamily, ViewProjection, Emitter, List, Environment);

	Translucent.StableSort([](const FTranslucentSection& A, const FTranslucentSection& B)
		{ return A.DistanceSquared > B.DistanceSquared; });
	for (const FTranslucentSection& Item : Translucent)
	{
		TransformStaticMesh(*Item.Proxy, ViewProjection);
		DrawStaticSection(Emitter, *Item.Proxy, Item.Section, List, Environment);
	}

	// The tracers glow over everything drawn so far.
	DrawTracers(ViewFamily, View, ViewProjection, Emitter, List, Environment);

	DrawWorldLines(ViewFamily, ViewProjection, Emitter, List);
	DrawShowFlags(ViewFamily, View, ViewProjection, Emitter, List);

	// The view model pass: Z cleared (the frame buffer masked), then the view model meshes with their projection.
	if (ViewModelMeshes.Num() > 0)
	{
		FGSFrame Masked = Environment.Frame;
		Masked.FBMSK = 0xffffffffu;
		List.SetFrame(0, Masked);
		List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
		AddFrameSprite(List, Environment, FGSRGBAQ(), 0);
		List.SetFrame(0, Environment.Frame);
		List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
		const FMatrix ViewModelProjection = View.ViewMatrix * View.ViewModelProjectionMatrix;
		for (const FStaticMeshSceneProxy* Proxy : ViewModelMeshes)
		{
			if (!Proxy->GetStaticMesh().HasValidRenderData())
			{
				continue;
			}
			++FrameStats.ObjectsTotal;
			++FrameStats.ObjectsVisible;
			TransformStaticMesh(*Proxy, ViewModelProjection);
			for (int32 Section = 0; Section < Proxy->GetNumSections(); ++Section)
			{
				DrawStaticSection(Emitter, *Proxy, Section, List, Environment);
			}
		}
	}
	FrameStats.TrianglesSubmitted = Emitter.GetNumTriangles();
	FrameStats.RegisterWrites = List.GetWrites().Num();
	FrameStats.TextureUploads = TextureCache.GetNumUploads();
}

void FGSSceneRenderer::DrawShowFlags(const FSceneViewFamily& ViewFamily, const FSceneView& View,
	const FMatrix& ViewProjection, FGSPrimitiveEmitter& Emitter, FGSCommandList& List)
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

void FGSSceneRenderer::DrawImpactMarks(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection,
	FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment)
{
	UWorld* World = ViewFamily.Scene != nullptr ? ViewFamily.Scene->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}
	TArray<FWorldEffectVertex> Vertices;
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
	TArray<FWorldEffectVertex> Vertices;
	FWorldEffectsGeometry::BuildTracerVertices(World->Tracers, View.ViewLocation, Vertices);
	if (Vertices.Num() == 0)
	{
		return;
	}
	// Added (Cs * As + Cd), fading across the ribbon through the mask; unmasked without a texture arena.
	const bool bMasked = BindEffectsMask(List);
	List.SetAlpha(0, FGSAlpha::Additive());
	SetDepthWrite(List, Environment, false);
	DrawEffectVertices(Emitter, Vertices, ViewProjection, bMasked);
	SetDepthWrite(List, Environment, true);
	List.SetAlpha(0, FGSAlpha::Translucent());
}

bool FGSSceneRenderer::BindEffectsMask(FGSCommandList& List)
{
	// The effects' mask with the spot moved to the alpha (MODULATE takes the colour from the vertex).
	TArray<uint8> Texels;
	FWorldEffectsGeometry::BuildMaskTexels(Texels);
	for (int32 Index = 0; Index < Texels.Num(); Index += 4)
	{
		Texels[Index + 3] = Texels[Index];
		Texels[Index + 0] = 255;
		Texels[Index + 1] = 255;
		Texels[Index + 2] = 255;
	}
	FGSTex0 Mask;
	if (!TextureCache.BindTexels(&GEffectsMaskKey, FWorldEffectsGeometry::MaskSize, FWorldEffectsGeometry::MaskSize,
			Texels, true, List, Mask))
	{
		return false;
	}
	List.SetTex1(0, BilinearSampling());
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	Clamp.WMT = EGSWrapMode::Clamp;
	List.SetClamp(0, Clamp);
	List.SetTex0(0, Mask);
	return true;
}

void FGSSceneRenderer::DrawEffectVertices(FGSPrimitiveEmitter& Emitter, const TArray<FWorldEffectVertex>& Vertices,
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

void FGSSceneRenderer::DrawCanvas(const FCanvas& Canvas, const FGSDrawEnvironment& Environment, FGSCommandList& List)
{
	if (Canvas.IsEmpty())
	{
		return;
	}
	TArray<FCanvasVertex> Vertices;
	Canvas.GetTriangles(Vertices);
	// Pixel coordinates (top-left origin) blended by their alpha over the frame, without the depth test or Z writes.
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	SetDepthWrite(List, Environment, false);
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Triangle;
	Prim.bGouraud = true;
	Prim.bAlphaBlend = true;
	List.SetPrim(Prim);
	FGSRGBAQ Last;
	bool bHasLast = false;
	for (const FCanvasVertex& Vertex : Vertices)
	{
		FGSRGBAQ Color;
		Color.R = UnitByte(Vertex.R);
		Color.G = UnitByte(Vertex.G);
		Color.B = UnitByte(Vertex.B);
		Color.A = uint8(FMath::Clamp(FMath::RoundToInt(Vertex.A * 128.0f), 0, 0x80));
		if (!bHasLast || Color.Encode() != Last.Encode())
		{
			List.SetRGBAQ(Color);
			Last = Color;
			bHasLast = true;
		}
		// OpenGL covers pixel i when i + 0.5 is inside; the GS samples pixel i at i.
		List.AddVertex(Environment.PixelVertex(Vertex.X - 0.5f, Vertex.Y - 0.5f, 0));
	}
	SetDepthWrite(List, Environment, true);
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
}
