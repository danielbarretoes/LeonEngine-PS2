#include "GSPrimitiveEmitter.h"

namespace
{

	/** Clip planes, inside when the distance is >= 0: near, far, then the guard band's four sides. */
	constexpr int32 NumPlanes = 6;
	/** A clipped triangle has at most one more vertex per plane. */
	constexpr int32 MaxPolygon = 3 + NumPlanes;
	/** The largest primitive coordinate distance from the frame's center that stays in 0..4095 with a margin. */
	constexpr float GuardExtent = 2000.0f;

	[[nodiscard]] float PlaneDistance(const FVector4& Clip, int32 Plane, float GuardX, float GuardY)
	{
		switch (Plane)
		{
			case 0:
				return Clip.Z;
			case 1:
				return Clip.W - Clip.Z;
			case 2:
				return (GuardX * Clip.W) - Clip.X;
			case 3:
				return (GuardX * Clip.W) + Clip.X;
			case 4:
				return (GuardY * Clip.W) - Clip.Y;
			default:
				return (GuardY * Clip.W) + Clip.Y;
		}
	}

	/** The bits of the view's planes (x, y in [-w, w], z in [0, w]) a vertex is outside of. */
	[[nodiscard]] uint32 ViewOutcode(const FVector4& Clip)
	{
		uint32 Code = 0;
		Code |= Clip.X > Clip.W ? 1u : 0u;
		Code |= Clip.X < -Clip.W ? 2u : 0u;
		Code |= Clip.Y > Clip.W ? 4u : 0u;
		Code |= Clip.Y < -Clip.W ? 8u : 0u;
		Code |= Clip.Z < 0.0f ? 16u : 0u;
		Code |= Clip.Z > Clip.W ? 32u : 0u;
		return Code;
	}

	/** The bits of the clip planes (PlaneDistance) a vertex is outside of. */
	[[nodiscard]] uint32 ClipOutcode(const FVector4& Clip, float GuardX, float GuardY)
	{
		uint32 Code = 0;
		for (int32 Plane = 0; Plane < NumPlanes; ++Plane)
		{
			Code |= PlaneDistance(Clip, Plane, GuardX, GuardY) < 0.0f ? (1u << Plane) : 0u;
		}
		return Code;
	}

	[[nodiscard]] FGSClipVertex Lerp(const FGSClipVertex& A, const FGSClipVertex& B, float T)
	{
		FGSClipVertex Result;
		Result.Clip = A.Clip + ((B.Clip - A.Clip) * T);
		Result.Color = A.Color + ((B.Color - A.Color) * T);
		Result.U = A.U + ((B.U - A.U) * T);
		Result.V = A.V + ((B.V - A.V) * T);
		return Result;
	}

	/** Sutherland-Hodgman against the planes in Planes (ClipOutcode bits); returns the vertex count. */
	int32 ClipPolygon(FGSClipVertex* Polygon, int32 Count, uint32 Planes, float GuardX, float GuardY)
	{
		FGSClipVertex Scratch[MaxPolygon];
		FGSClipVertex* In = Polygon;
		FGSClipVertex* Out = Scratch;
		for (int32 Plane = 0; Plane < NumPlanes && Count > 0; ++Plane)
		{
			if ((Planes & (1u << Plane)) == 0)
			{
				continue;
			}
			int32 OutCount = 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FGSClipVertex& A = In[Index];
				const FGSClipVertex& B = In[(Index + 1) % Count];
				const float DistanceA = PlaneDistance(A.Clip, Plane, GuardX, GuardY);
				const float DistanceB = PlaneDistance(B.Clip, Plane, GuardX, GuardY);
				if (DistanceA >= 0.0f && OutCount < MaxPolygon)
				{
					Out[OutCount++] = A;
				}
				if ((DistanceA >= 0.0f) != (DistanceB >= 0.0f) && OutCount < MaxPolygon)
				{
					Out[OutCount++] = Lerp(A, B, DistanceA / (DistanceA - DistanceB));
				}
			}
			Count = OutCount;
			FGSClipVertex* Swap = In;
			In = Out;
			Out = Swap;
		}
		if (In != Polygon)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Polygon[Index] = In[Index];
			}
		}
		return Count;
	}

	/** True when the triangle winds counter-clockwise in normalized device coordinates (Y up): a front face. */
	[[nodiscard]] bool IsFrontFacing(const FVector4& P0, const FVector4& P1, const FVector4& P2)
	{
		const float X0 = P0.X / P0.W;
		const float Y0 = P0.Y / P0.W;
		const float Area =
			(((P1.X / P1.W) - X0) * ((P2.Y / P2.W) - Y0)) - (((P2.X / P2.W) - X0) * ((P1.Y / P1.W) - Y0));
		return Area > 0.0f;
	}

	[[nodiscard]] uint8 ToByte(float Value, float Scale)
	{
		return uint8(FMath::Clamp(FMath::RoundToInt(Value * Scale), 0, 255));
	}

} // namespace

FGSPrimitiveEmitter::FGSPrimitiveEmitter(const FGSDrawEnvironment& InEnvironment, FGSCommandList& InList)
	: Environment(InEnvironment)
	, List(InList)
{
	GuardX = GuardExtent / (float(Environment.Width) * 0.5f);
	GuardY = GuardExtent / (float(Environment.Height) * 0.5f);
}

void FGSPrimitiveEmitter::BeginTriangles(bool bInTextured, bool bBlend, bool bInCullBackFaces)
{
	BeginPrimitives(EGSPrimitive::Triangle, bInTextured, bBlend, bInCullBackFaces);
}

void FGSPrimitiveEmitter::BeginStrip(bool bInTextured, bool bBlend, bool bInCullBackFaces)
{
	BeginPrimitives(EGSPrimitive::TriangleStrip, bInTextured, bBlend, bInCullBackFaces);
}

void FGSPrimitiveEmitter::BeginPrimitives(EGSPrimitive Type, bool bInTextured, bool bBlend, bool bInCullBackFaces)
{
	bTextured = bInTextured;
	bCullBackFaces = bInCullBackFaces;
	bFogging = Fog.bEnabled;
	ColorScale = bTextured ? 128.0f : 255.0f;
	FGSPrim Prim;
	Prim.Type = Type;
	Prim.bGouraud = true;
	Prim.bTextured = bTextured;
	Prim.bFog = bFogging;
	Prim.bAlphaBlend = bBlend;
	List.SetPrim(Prim);
	bHasLastColor = false;
}

void FGSPrimitiveEmitter::BeginLines(bool bBlend)
{
	bTextured = false;
	bCullBackFaces = false;
	bFogging = false;
	ColorScale = 255.0f;
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Line;
	Prim.bGouraud = true;
	Prim.bAlphaBlend = bBlend;
	List.SetPrim(Prim);
	bHasLastColor = false;
}

void FGSPrimitiveEmitter::AddTriangle(const FGSClipVertex& A, const FGSClipVertex& B, const FGSClipVertex& C)
{
	if ((ViewOutcode(A.Clip) & ViewOutcode(B.Clip) & ViewOutcode(C.Clip)) != 0)
	{
		++NumRejected;
		return;
	}
	FGSClipVertex Polygon[MaxPolygon] = {A, B, C};
	int32 Count = 3;
	const uint32 Planes =
		ClipOutcode(A.Clip, GuardX, GuardY) | ClipOutcode(B.Clip, GuardX, GuardY) | ClipOutcode(C.Clip, GuardX, GuardY);
	if (Planes != 0)
	{
		Count = ClipPolygon(Polygon, Count, Planes, GuardX, GuardY);
		if (Count < 3)
		{
			++NumRejected;
			return;
		}
	}
	// Front faces wind counter-clockwise in normalized device coordinates (Y up); a clipped polygon keeps the winding.
	if (bCullBackFaces && !IsFrontFacing(Polygon[0].Clip, Polygon[1].Clip, Polygon[2].Clip))
	{
		++NumRejected;
		return;
	}
	for (int32 Index = 1; Index + 1 < Count; ++Index)
	{
		Emit(Polygon[0]);
		Emit(Polygon[Index]);
		Emit(Polygon[Index + 1]);
		++NumTriangles;
	}
}

void FGSPrimitiveEmitter::AddStrips(
	TArrayView<const FGSClipVertex> Vertices, TArrayView<const EGSStripTriangle> Triangles)
{
	check(Vertices.Num() == Triangles.Num());
	const int32 Num = Vertices.Num();
	StripOutcodes.SetNumUninitialized(Num, false);
	StripDrawn.SetNumUninitialized(Num, false);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		StripOutcodes[Index] = ViewOutcode(Vertices[Index].Clip);
		StripDrawn[Index] = false;
		const EGSStripTriangle Triangle = Triangles[Index];
		if (Triangle == EGSStripTriangle::None || Index < 2)
		{
			continue;
		}
		// The triangle of the last two vertices and this one, in the source's winding.
		const bool bReversed = Triangle == EGSStripTriangle::Reversed;
		const FVector4& First = Vertices[bReversed ? Index - 1 : Index - 2].Clip;
		const FVector4& Second = Vertices[bReversed ? Index - 2 : Index - 1].Clip;
		if ((StripOutcodes[Index - 2] & StripOutcodes[Index - 1] & StripOutcodes[Index]) != 0 ||
			(bCullBackFaces && !IsFrontFacing(First, Second, Vertices[Index].Clip)))
		{
			++NumRejected;
			continue;
		}
		StripDrawn[Index] = true;
		++NumTriangles;
	}
	// A vertex goes to the GS when it or one of the next two closes a drawn triangle: the vertex queue then holds the
	// right two vertices before each kick, and strips with nothing to draw cost nothing.
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const bool bNeeded = StripDrawn[Index] || (Index + 1 < Num && StripDrawn[Index + 1]) ||
			(Index + 2 < Num && StripDrawn[Index + 2]);
		if (bNeeded)
		{
			Emit(Vertices[Index], StripDrawn[Index]);
		}
	}
}

void FGSPrimitiveEmitter::TransformVertexBatch(
	const FGSVertexDraw& Draw, const FGSVertexBatch& Batch, FGSClipVertex* OutVertices, EGSStripTriangle* OutTriangles)
{
	const int32 NumVertices = int32(Batch.NumVertices);
	const float* PositionScale = Draw.PositionScale;
	const float* PositionBias = Draw.PositionBias;
	constexpr float InverseNormal = 1.0f / 127.0f;
	constexpr float InverseTexCoord = 1.0f / 4096.0f;
	constexpr float InverseByte = 1.0f / 255.0f;
	for (int32 Index = 0; Index < NumVertices; ++Index)
	{
		const int8* Normal = Batch.Normals + (Index * 4);
		const uint8 Flags = uint8(Normal[3]);
		OutTriangles[Index] = (Flags & FGSVertexBatch::FlagNoKick) != 0 ? EGSStripTriangle::None
			: (Flags & FGSVertexBatch::FlagReversed) != 0               ? EGSStripTriangle::Reversed
																		: EGSStripTriangle::Forward;
		const int16* Quantized = Batch.Positions + (Index * 3);
		FVector Position((float(Quantized[0]) * PositionScale[0]) + PositionBias[0],
			(float(Quantized[1]) * PositionScale[1]) + PositionBias[1],
			(float(Quantized[2]) * PositionScale[2]) + PositionBias[2]);
		FVector LocalNormal(
			float(Normal[0]) * InverseNormal, float(Normal[1]) * InverseNormal, float(Normal[2]) * InverseNormal);
		if (Batch.IsSkinned())
		{
			// Linear blend skinning with the vertex's two palette bones (what the Skinned programs do on VU1).
			const uint8* Influence = Batch.Skin + (Index * 4);
			check(uint32(Influence[0]) < Batch.NumBones && uint32(Influence[1]) < Batch.NumBones);
			const FGSSkinMatrix& Skin0 = Batch.Palette[Influence[0]];
			const FGSSkinMatrix& Skin1 = Batch.Palette[Influence[1]];
			const float Weight0 = float(Influence[2]) * InverseByte;
			const float Weight1 = float(Influence[3]) * InverseByte;
			Position = (Skin0.TransformPosition(Position) * Weight0) + (Skin1.TransformPosition(Position) * Weight1);
			LocalNormal =
				(Skin0.TransformVector(LocalNormal) * Weight0) + (Skin1.TransformVector(LocalNormal) * Weight1);
		}
		FGSClipVertex& Vertex = OutVertices[Index];
		Vertex.Clip = Draw.LocalToClip.TransformPosition(Position);
		FLinearColor Color(Draw.Color.R, Draw.Color.G, Draw.Color.B, 1.0f);
		if (Draw.bLit)
		{
			// UE's per vertex Lambert on the world normal (the inverse transpose of LocalToWorld, normalized).
			const FVector4 World = Draw.LocalToWorld.TransformPosition(Position);
			const FVector4 WorldNormal = Draw.NormalToWorld.TransformVector(LocalNormal);
			const FVector Light = Draw.Lights.Irradiance(FVector(World.X, World.Y, World.Z),
				FVector(WorldNormal.X, WorldNormal.Y, WorldNormal.Z).GetSafeNormal());
			Color = FLinearColor(Color.R * Light.X, Color.G * Light.Y, Color.B * Light.Z, 1.0f);
		}
		// The baked colour (white until the baked lighting) scales the material's.
		const uint8* Baked = Batch.Colors + (Index * 4);
		Vertex.Color =
			FLinearColor(Color.R * (float(Baked[0]) * InverseByte), Color.G * (float(Baked[1]) * InverseByte),
				Color.B * (float(Baked[2]) * InverseByte), Draw.Color.A * (float(Baked[3]) * InverseByte));
		const int16* TexCoord = Batch.TexCoords + (Index * 2);
		Vertex.U = ((float(TexCoord[0]) * InverseTexCoord) + Batch.TexCoordOffset[0]) * Draw.UvScale.X;
		Vertex.V = ((float(TexCoord[1]) * InverseTexCoord) + Batch.TexCoordOffset[1]) * Draw.UvScale.Y;
	}
}

void FGSPrimitiveEmitter::AddVertexBatch(const FGSVertexDraw& Draw, const FGSVertexBatch& Batch)
{
	BatchVertices.SetNumUninitialized(int32(Batch.NumVertices), false);
	BatchTriangles.SetNumUninitialized(int32(Batch.NumVertices), false);
	TransformVertexBatch(Draw, Batch, BatchVertices.GetData(), BatchTriangles.GetData());
	AddStrips(BatchVertices, BatchTriangles);
}

void FGSPrimitiveEmitter::AddLine(const FGSClipVertex& A, const FGSClipVertex& B)
{
	if ((ViewOutcode(A.Clip) & ViewOutcode(B.Clip)) != 0)
	{
		return;
	}
	FGSClipVertex Ends[2] = {A, B};
	for (int32 Plane = 0; Plane < NumPlanes; ++Plane)
	{
		const float DistanceA = PlaneDistance(Ends[0].Clip, Plane, GuardX, GuardY);
		const float DistanceB = PlaneDistance(Ends[1].Clip, Plane, GuardX, GuardY);
		if (DistanceA < 0.0f && DistanceB < 0.0f)
		{
			return;
		}
		if (DistanceA < 0.0f)
		{
			Ends[0] = Lerp(Ends[0], Ends[1], DistanceA / (DistanceA - DistanceB));
		}
		else if (DistanceB < 0.0f)
		{
			Ends[1] = Lerp(Ends[0], Ends[1], DistanceA / (DistanceA - DistanceB));
		}
	}
	Emit(Ends[0]);
	Emit(Ends[1]);
}

void FGSPrimitiveEmitter::Emit(const FGSClipVertex& Vertex, bool bKick)
{
	const float InvW = 1.0f / Vertex.Clip.W;
	FGSRGBAQ Color;
	Color.R = ToByte(Vertex.Color.R, ColorScale);
	Color.G = ToByte(Vertex.Color.G, ColorScale);
	Color.B = ToByte(Vertex.Color.B, ColorScale);
	Color.A = ToByte(Vertex.Color.A, 128.0f);
	Color.Q = bTextured ? InvW : 1.0f;
	if (!bHasLastColor || Color.Encode() != LastColor.Encode())
	{
		List.SetRGBAQ(Color);
		LastColor = Color;
		bHasLastColor = true;
	}
	if (bTextured)
	{
		FGSST ST;
		ST.S = Vertex.U * InvW;
		ST.T = Vertex.V * InvW;
		List.SetST(ST);
	}
	// OpenGL covers pixel i when its center, i + 0.5, is inside; the GS samples pixel i at i.
	const float PixelX = (((Vertex.Clip.X * InvW) + 1.0f) * float(Environment.Width) * 0.5f) - 0.5f;
	const float PixelY = ((1.0f - (Vertex.Clip.Y * InvW)) * float(Environment.Height) * 0.5f) - 0.5f;
	const float Depth = FMath::Clamp(1.0f - (Vertex.Clip.Z * InvW), 0.0f, 1.0f);
	const uint32 Z =
		FMath::Min(uint32(Depth * float(FGSDrawEnvironment::MaxDepth24)) + DepthBias, FGSDrawEnvironment::MaxDepth24);
	const FGSXYZ Position = Environment.PixelVertex(PixelX, PixelY, Z);
	if (bFogging)
	{
		// XYZF2 / XYZF3: the 24-bit Z and the fog coefficient of the vertex's depth (VU1's microprograms do the same).
		FGSXYZF Fogged;
		Fogged.X = Position.X;
		Fogged.Y = Position.Y;
		Fogged.Z = Position.Z;
		Fogged.F = Fog.GetF(Vertex.Clip.W);
		if (bKick)
		{
			List.AddVertex(Fogged);
		}
		else
		{
			List.AddVertexNoKick(Fogged);
		}
		return;
	}
	if (bKick)
	{
		List.AddVertex(Position);
	}
	else
	{
		List.AddVertexNoKick(Position);
	}
}
