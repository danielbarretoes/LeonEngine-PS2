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
	bTextured = bInTextured;
	bCullBackFaces = bInCullBackFaces;
	ColorScale = bTextured ? 128.0f : 255.0f;
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Triangle;
	Prim.bGouraud = true;
	Prim.bTextured = bTextured;
	Prim.bAlphaBlend = bBlend;
	List.SetPrim(Prim);
	bHasLastColor = false;
}

void FGSPrimitiveEmitter::BeginLines(bool bBlend)
{
	bTextured = false;
	bCullBackFaces = false;
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
	const FVector4& P0 = Polygon[0].Clip;
	const FVector4& P1 = Polygon[1].Clip;
	const FVector4& P2 = Polygon[2].Clip;
	const float X0 = P0.X / P0.W;
	const float Y0 = P0.Y / P0.W;
	const float Area = (((P1.X / P1.W) - X0) * ((P2.Y / P2.W) - Y0)) - (((P2.X / P2.W) - X0) * ((P1.Y / P1.W) - Y0));
	if (bCullBackFaces && Area <= 0.0f)
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

void FGSPrimitiveEmitter::Emit(const FGSClipVertex& Vertex)
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
	List.AddVertex(Environment.PixelVertex(PixelX, PixelY, Z));
}
