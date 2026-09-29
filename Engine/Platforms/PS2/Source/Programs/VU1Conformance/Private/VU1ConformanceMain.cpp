#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSPrimitiveEmitter.h"
#include "GSVertexBatch.h"
#include "HAL/PlatformProcess.h"
#include "Math/RotationTranslationMatrix.h"
#include "Misc/CommandLine.h"
#include "Misc/MemStack.h"
#include "PS2RHI.h"
#include "PS2VU1.h"

DEFINE_LOG_CATEGORY_STATIC(LogVU1Conformance, Log, All);

namespace
{

	constexpr int32 ScreenWidth = 640;
	constexpr int32 ScreenHeight = 448;

	/**
	 * Plan D2's tolerances after FTOI4: X and Y 1 (a 16th of a pixel), RGBA 1. VU1 truncates where the EE rounds, so a
	 * few units in the last place separate them after a chain of operations: S, T and Q within 8 of the float's (the GS
	 * keeps 16 of its 24 bits of mantissa: 256), Z within 8 of the 24-bit depth (2^-21 of the range; the depth is
	 * 1 - z / w, which cancels bits). A triangle one side culls and the other draws must have less than
	 * MaxDisagreeingArea square pixels (a sliver whose facing the rounding decides).
	 */
	constexpr int32 MaxXY = 1;
	constexpr int32 MaxZ = 8;
	constexpr int32 MaxColor = 1;
	constexpr int32 MaxUlps = 8;
	constexpr float MaxDisagreeingArea = 1.0f;
	/** The fog coefficient (N15): VU1 truncates its multiply-add where the EE rounds, so F may round the other way. */
	constexpr int32 MaxFog = 1;

	/** A GS vertex as the GS takes it: its ST and Q, RGBA, XYZ, the fog's F and whether it kicks the drawing. */
	struct FGSVertexState
	{
		float S = 0.0f;
		float T = 0.0f;
		float Q = 1.0f;
		uint8 Color[4] = {0, 0, 0, 0};
		uint16 X = 0;
		uint16 Y = 0;
		uint32 Z = 0;
		uint8 F = 0xff;
		bool bKick = true;
	};

	struct FTriangle
	{
		FGSVertexState Corners[3];
	};

	/** The largest differences found, and the checks' result. */
	struct FReport
	{
		int32 NumBatches = 0;
		int32 NumTriangles = 0;
		int32 NumDisagreements = 0;
		int32 NumFailures = 0;
		int32 MaxXYSeen = 0;
		int32 MaxZSeen = 0;
		int32 MaxColorSeen = 0;
		int32 MaxUlpsSeen = 0;
		int32 MaxFogSeen = 0;
	};

	/** A deterministic sequence (a linear congruential generator). */
	struct FRandom
	{
		uint32 State = 1;

		uint32 Next()
		{
			State = (State * 1664525u) + 1013904223u;
			return State >> 8;
		}
		[[nodiscard]] float Unit()
		{
			return float(Next() & 0xffff) / 65536.0f;
		}
		[[nodiscard]] int32 Range(int32 Min, int32 Max)
		{
			return Min + int32(Next() % uint32(Max - Min + 1));
		}
	};

	/**
	 * A batch's streams as LPS2 v2 lays them out: quadword aligned, zero padded (VIF NOPs after an UNPACK's data); a
	 * skinned batch's skin and palette too.
	 */
	struct alignas(16) FTestStreams
	{
		int16 Positions[FGSVertexBatch::MaxVertices * 3] = {};
		int8 Normals[FGSVertexBatch::MaxVertices * 4] = {};
		uint8 Colors[FGSVertexBatch::MaxVertices * 4] = {};
		int16 TexCoords[FGSVertexBatch::MaxVertices * 2] = {};
		uint8 Skin[FGSVertexBatch::MaxVertices * 4] = {};
		FGSSkinMatrix Palette[FGSVertexBatch::MaxBones];
		uint32 NumBones = 0;
	};

	[[nodiscard]] float BitsToFloat(uint32 Bits)
	{
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	[[nodiscard]] int32 Ulps(float A, float B)
	{
		int32 BitsA = 0;
		int32 BitsB = 0;
		FMemory::Memcpy(&BitsA, &A, sizeof(A));
		FMemory::Memcpy(&BitsB, &B, sizeof(B));
		if ((BitsA < 0) != (BitsB < 0))
		{
			return A == B ? 0 : 0x7fffffff;
		}
		return FMath::Abs(BitsA - BitsB);
	}

	/** The GS vertices a list's writes send (ST and RGBAQ as the GS holds them at each XYZ). */
	TArray<FGSVertexState> DecodeWrites(const FGSCommandList& List)
	{
		TArray<FGSVertexState> Vertices;
		FGSVertexState Current;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			switch (Write.Register)
			{
				case EGSRegister::RGBAQ:
				{
					const FGSRGBAQ Color = FGSRGBAQ::Decode(Write.Value);
					Current.Color[0] = Color.R;
					Current.Color[1] = Color.G;
					Current.Color[2] = Color.B;
					Current.Color[3] = Color.A;
					Current.Q = Color.Q;
					break;
				}
				case EGSRegister::ST:
				{
					const FGSST ST = FGSST::Decode(Write.Value);
					Current.S = ST.S;
					Current.T = ST.T;
					break;
				}
				case EGSRegister::XYZ2:
				case EGSRegister::XYZ3:
				{
					const FGSXYZ Position = FGSXYZ::Decode(Write.Value);
					Current.X = Position.X;
					Current.Y = Position.Y;
					Current.Z = Position.Z;
					Current.F = 0xff;
					Current.bKick = Write.Register == EGSRegister::XYZ2;
					Vertices.Add(Current);
					break;
				}
				case EGSRegister::XYZF2:
				case EGSRegister::XYZF3:
				{
					const FGSXYZF Position = FGSXYZF::Decode(Write.Value);
					Current.X = Position.X;
					Current.Y = Position.Y;
					Current.Z = Position.Z;
					Current.F = Position.F;
					Current.bKick = Write.Register == EGSRegister::XYZF2;
					Vertices.Add(Current);
					break;
				}
				default:
					break;
			}
		}
		return Vertices;
	}

	/**
	 * The GS vertices of VU1's PACKED packet (its GIFtag, then ST, RGBAQ and XYZF2 a vertex; Q is ST's): XYZF2's Z is
	 * 24 bits at bit 4 of the third word, F 8 bits at bit 4 of the fourth, ADC its bit 15.
	 */
	TArray<FGSVertexState> DecodePacket(const TArray<uint64>& Packet)
	{
		TArray<FGSVertexState> Vertices;
		const uint32 NumVertices = uint32(Packet[0] & 0x7fff);
		for (uint32 Index = 0; Index < NumVertices; ++Index)
		{
			const uint64* Quadwords = &Packet[2 + (Index * 6)];
			FGSVertexState& Vertex = Vertices.AddDefaulted_GetRef();
			Vertex.S = BitsToFloat(uint32(Quadwords[0]));
			Vertex.T = BitsToFloat(uint32(Quadwords[0] >> 32));
			Vertex.Q = BitsToFloat(uint32(Quadwords[1]));
			Vertex.Color[0] = uint8(Quadwords[2]);
			Vertex.Color[1] = uint8(Quadwords[2] >> 32);
			Vertex.Color[2] = uint8(Quadwords[3]);
			Vertex.Color[3] = uint8(Quadwords[3] >> 32);
			Vertex.X = uint16(Quadwords[4]);
			Vertex.Y = uint16(Quadwords[4] >> 32);
			Vertex.Z = uint32(Quadwords[5] >> 4) & 0xffffffu;
			Vertex.F = uint8(Quadwords[5] >> 36);
			Vertex.bKick = (Quadwords[5] & (uint64(1) << 47)) == 0;
		}
		return Vertices;
	}

	/** The triangles a TRISTRIP of these vertices draws: each kick with two vertices before it since the PRIM. */
	TArray<FTriangle> StripTriangles(const TArray<FGSVertexState>& Vertices)
	{
		TArray<FTriangle> Triangles;
		for (int32 Index = 2; Index < Vertices.Num(); ++Index)
		{
			if (Vertices[Index].bKick)
			{
				Triangles.Add({{Vertices[Index - 2], Vertices[Index - 1], Vertices[Index]}});
			}
		}
		return Triangles;
	}

	[[nodiscard]] float AreaInPixels(const FTriangle& Triangle)
	{
		const auto Pixel = [](uint16 Value) { return float(Value) / 16.0f; };
		const FGSVertexState* C = Triangle.Corners;
		const float Area = ((Pixel(C[1].X) - Pixel(C[0].X)) * (Pixel(C[2].Y) - Pixel(C[0].Y))) -
			((Pixel(C[2].X) - Pixel(C[0].X)) * (Pixel(C[1].Y) - Pixel(C[0].Y)));
		return FMath::Abs(Area) * 0.5f;
	}

	[[nodiscard]] bool SameCloser(const FTriangle& A, const FTriangle& B)
	{
		return FMath::Abs(int32(A.Corners[2].X) - int32(B.Corners[2].X)) <= MaxXY &&
			FMath::Abs(int32(A.Corners[2].Y) - int32(B.Corners[2].Y)) <= MaxXY;
	}

	/** Compares a triangle of VU1 with the emitter's; false beyond the tolerances. */
	bool CompareTriangle(const FTriangle& Vu1, const FTriangle& Reference, bool bTextured, bool bFog, FReport& Report)
	{
		bool bSame = true;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const FGSVertexState& A = Vu1.Corners[Corner];
			const FGSVertexState& B = Reference.Corners[Corner];
			const int32 DXY = FMath::Max(FMath::Abs(int32(A.X) - int32(B.X)), FMath::Abs(int32(A.Y) - int32(B.Y)));
			const int32 DZ = FMath::Abs(int32(A.Z) - int32(B.Z));
			int32 DColor = 0;
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				DColor = FMath::Max(DColor, FMath::Abs(int32(A.Color[Channel]) - int32(B.Color[Channel])));
			}
			const int32 DUlps = bTextured ? FMath::Max(Ulps(A.S, B.S), FMath::Max(Ulps(A.T, B.T), Ulps(A.Q, B.Q))) : 0;
			Report.MaxXYSeen = FMath::Max(Report.MaxXYSeen, DXY);
			Report.MaxZSeen = FMath::Max(Report.MaxZSeen, DZ);
			Report.MaxColorSeen = FMath::Max(Report.MaxColorSeen, DColor);
			Report.MaxUlpsSeen = FMath::Max(Report.MaxUlpsSeen, DUlps);
			const int32 DFog = bFog ? FMath::Abs(int32(A.F) - int32(B.F)) : 0;
			Report.MaxFogSeen = FMath::Max(Report.MaxFogSeen, DFog);
			if (DXY > MaxXY || DZ > MaxZ || DColor > MaxColor || DUlps > MaxUlps || DFog > MaxFog)
			{
				if (bSame)
				{
					UE_LOG(LogVU1Conformance, Error,
						"VU1Conformance: corner %d differs: VU1 (%u, %u, %u) rgba %u %u %u %u stq %g %g %g f %u, "
						"emitter (%u, %u, %u) rgba %u %u %u %u stq %g %g %g f %u",
						Corner, uint32(A.X), uint32(A.Y), A.Z, uint32(A.Color[0]), uint32(A.Color[1]),
						uint32(A.Color[2]), uint32(A.Color[3]), double(A.S), double(A.T), double(A.Q), uint32(A.F),
						uint32(B.X), uint32(B.Y), B.Z, uint32(B.Color[0]), uint32(B.Color[1]), uint32(B.Color[2]),
						uint32(B.Color[3]), double(B.S), double(B.T), double(B.Q), uint32(B.F));
				}
				bSame = false;
			}
		}
		return bSame;
	}

	/**
	 * Runs Batch of Draw on VU1 and through the C++ emitter and compares the triangles each draws, in order; a triangle
	 * only one side draws is a sliver (under MaxDisagreeingArea) or a failure.
	 */
	void CheckBatch(const TCHAR* Name, const FGSVertexDraw& Draw, const FGSVertexBatch& Batch,
		const FGSDrawEnvironment& Environment, FReport& Report)
	{
		++Report.NumBatches;
		TArray<uint64> Packet;
		if (!FPS2VU1::RunBatchForTest(Draw, Batch, Environment, Packet))
		{
			UE_LOG(LogVU1Conformance, Error, "VU1Conformance: %s: VU1 did not end", Name);
			++Report.NumFailures;
			return;
		}
		if (uint32(Packet[0] & 0x7fff) != Batch.NumVertices)
		{
			UE_LOG(LogVU1Conformance, Error, "VU1Conformance: %s: the GIFtag's NLOOP is %u, not %u", Name,
				uint32(Packet[0] & 0x7fff), Batch.NumVertices);
			++Report.NumFailures;
			return;
		}
		// The emitter's scratch is on the frame's stack.
		FMemMark Mark(FMemStack::Get());
		FGSCommandList List;
		FGSPrimitiveEmitter Emitter(Environment, List);
		Emitter.SetFog(Draw.Fog);
		Emitter.BeginStrip(Draw.bTextured, Draw.bBlend, true);
		Emitter.AddVertexBatch(Draw, Batch);
		const TArray<FTriangle> Vu1 = StripTriangles(DecodePacket(Packet));
		const TArray<FTriangle> Reference = StripTriangles(DecodeWrites(List));
		int32 IndexVu1 = 0;
		int32 IndexReference = 0;
		bool bFailed = false;
		while (IndexVu1 < Vu1.Num() || IndexReference < Reference.Num())
		{
			const bool bHasVu1 = IndexVu1 < Vu1.Num();
			const bool bHasReference = IndexReference < Reference.Num();
			if (bHasVu1 && bHasReference && SameCloser(Vu1[IndexVu1], Reference[IndexReference]))
			{
				++Report.NumTriangles;
				bFailed |= !CompareTriangle(
					Vu1[IndexVu1], Reference[IndexReference], Draw.bTextured, Draw.Fog.bEnabled, Report);
				++IndexVu1;
				++IndexReference;
				continue;
			}
			// One side has a triangle the other does not: the one whose next triangle matches the other's.
			const bool bVu1Extra = bHasVu1 &&
				(!bHasReference ||
					(IndexVu1 + 1 < Vu1.Num() && SameCloser(Vu1[IndexVu1 + 1], Reference[IndexReference])));
			const FTriangle& Extra = bVu1Extra ? Vu1[IndexVu1] : Reference[IndexReference];
			++Report.NumDisagreements;
			if (AreaInPixels(Extra) >= MaxDisagreeingArea)
			{
				UE_LOG(LogVU1Conformance, Error,
					"VU1Conformance: %s: only %s draws a triangle of %.2f square pixels closing at (%u, %u)", Name,
					bVu1Extra ? "VU1" : "the emitter", double(AreaInPixels(Extra)), uint32(Extra.Corners[2].X),
					uint32(Extra.Corners[2].Y));
				bFailed = true;
			}
			(bVu1Extra ? IndexVu1 : IndexReference)++;
		}
		if (bFailed)
		{
			UE_LOG(LogVU1Conformance, Error, "VU1Conformance: %s: FAILED (%d triangles on VU1, %d from the emitter)",
				Name, Vu1.Num(), Reference.Num());
			++Report.NumFailures;
		}
	}

	/** Random strips of Streams' NumVertices vertices within +-Extent (quantized units) of the origin. */
	void MakeStrips(FTestStreams& Streams, uint32 NumVertices, int32 Extent, FRandom& Random)
	{
		uint32 StripStart = 0;
		uint32 StripLength = 0;
		int32 CenterX = 0;
		int32 CenterY = 0;
		int32 CenterZ = 0;
		for (uint32 Index = 0; Index < NumVertices; ++Index)
		{
			if (Index == StripStart + StripLength)
			{
				// A new strip: its first two vertices close no triangle.
				StripStart = Index;
				StripLength = uint32(Random.Range(3, 12));
				CenterX = Random.Range(-Extent, Extent);
				CenterY = Random.Range(-Extent, Extent);
				CenterZ = Random.Range(-Extent, Extent);
			}
			const uint32 InStrip = Index - StripStart;
			const int32 Step = Extent / 6;
			Streams.Positions[(Index * 3) + 0] =
				int16(FMath::Clamp(CenterX + Random.Range(-Step, Step), -32767, 32767));
			Streams.Positions[(Index * 3) + 1] =
				int16(FMath::Clamp(CenterY + Random.Range(-Step, Step), -32767, 32767));
			Streams.Positions[(Index * 3) + 2] =
				int16(FMath::Clamp(CenterZ + Random.Range(-Step, Step), -32767, 32767));
			// A normal of about unit length, times 127.
			FVector Normal(Random.Unit() - 0.5f, Random.Unit() - 0.5f, Random.Unit() - 0.5f);
			Normal = Normal.GetSafeNormal();
			Streams.Normals[(Index * 4) + 0] = int8(FMath::RoundToInt(Normal.X * 127.0f));
			Streams.Normals[(Index * 4) + 1] = int8(FMath::RoundToInt(Normal.Y * 127.0f));
			Streams.Normals[(Index * 4) + 2] = int8(FMath::RoundToInt(Normal.Z * 127.0f));
			uint8 Flags = 0;
			if (InStrip < 2 || Random.Range(0, 15) == 0)
			{
				Flags = FGSVertexBatch::FlagNoKick;
			}
			else if (((InStrip - 2) & 1) != 0)
			{
				Flags = FGSVertexBatch::FlagReversed;
			}
			Streams.Normals[(Index * 4) + 3] = int8(Flags);
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				Streams.Colors[(Index * 4) + Channel] = uint8(Random.Range(Channel == 3 ? 64 : 0, 255));
			}
			Streams.TexCoords[(Index * 2) + 0] = int16(Random.Range(-8 * 4096, (8 * 4096) - 1));
			Streams.TexCoords[(Index * 2) + 1] = int16(Random.Range(-8 * 4096, (8 * 4096) - 1));
		}
	}

	/**
	 * A skinned batch's skin and palette: NumBones bones, each turned by up to 25 degrees about the mesh's origin,
	 * scaled by 0.9 to 1.1 and moved by up to 20 units; each vertex blends two of them with weights adding up to 255.
	 */
	void MakeSkin(FTestStreams& Streams, uint32 NumVertices, uint32 NumBones, FRandom& Random)
	{
		Streams.NumBones = NumBones;
		for (uint32 Bone = 0; Bone < NumBones; ++Bone)
		{
			const FRotator Rotation(
				(Random.Unit() - 0.5f) * 50.0f, (Random.Unit() - 0.5f) * 50.0f, (Random.Unit() - 0.5f) * 50.0f);
			const FVector Translation(
				(Random.Unit() - 0.5f) * 40.0f, (Random.Unit() - 0.5f) * 40.0f, (Random.Unit() - 0.5f) * 40.0f);
			const float Scale = 0.9f + (Random.Unit() * 0.2f);
			const FMatrix Scaling(FPlane(Scale, 0.0f, 0.0f, 0.0f), FPlane(0.0f, Scale, 0.0f, 0.0f),
				FPlane(0.0f, 0.0f, Scale, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f));
			Streams.Palette[Bone] =
				FGSSkinMatrix::FromMatrix(Scaling * FRotationTranslationMatrix(Rotation, Translation));
		}
		for (uint32 Index = 0; Index < NumVertices; ++Index)
		{
			uint8* Influence = Streams.Skin + (Index * 4);
			Influence[0] = uint8(Random.Range(0, int32(NumBones) - 1));
			Influence[1] = uint8(Random.Range(0, int32(NumBones) - 1));
			Influence[2] = uint8(Random.Range(0, 255));
			Influence[3] = uint8(255 - Influence[2]);
		}
	}

	/** Batch's skin and palette from Streams, for a skinned draw. */
	void SetSkin(FGSVertexBatch& Batch, const FTestStreams& Streams, bool bSkinned)
	{
		if (bSkinned)
		{
			Batch.Skin = Streams.Skin;
			Batch.Palette = Streams.Palette;
			Batch.NumBones = Streams.NumBones;
		}
	}

	/** A draw of the test: LocalToWorld (scaled, rotated, moved Distance forward) through a projection to clip space.
	 */
	FGSVertexDraw MakeDraw(
		bool bLit, bool bTextured, float PositionScale, float Distance, const FRotator& Rotation, const FVector& Scale)
	{
		FGSVertexDraw Draw;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Draw.PositionScale[Axis] = PositionScale;
			Draw.PositionBias[Axis] = Axis == 1 ? 3.25f : 0.0f;
		}
		const FMatrix Scaling(FPlane(Scale.X, 0.0f, 0.0f, 0.0f), FPlane(0.0f, Scale.Y, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, Scale.Z, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f));
		Draw.LocalToWorld = Scaling * FRotationTranslationMatrix(Rotation, FVector(0.0f, 0.0f, Distance));
		// World Z is the depth: w = Z, z = 0.9 w - 10 (0 at the near plane of 11 cm), x and y a 60 degree view.
		const FMatrix Projection(FPlane(1.2f, 0.0f, 0.0f, 0.0f), FPlane(0.0f, 1.7f, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, 0.9f, 1.0f), FPlane(0.0f, 0.0f, -10.0f, 0.0f));
		Draw.LocalToClip = Draw.LocalToWorld * Projection;
		const FMatrix Adjoint = Draw.LocalToWorld.TransposeAdjoint();
		Draw.NormalToWorld = Draw.LocalToWorld.Determinant() < 0.0f ? Adjoint * -1.0f : Adjoint;
		Draw.Color = bTextured ? FLinearColor(0.7f, 0.8f, 1.0f, 0.5f) : FLinearColor(0.9f, 0.5f, 0.2f, 1.0f);
		Draw.UvScale = bTextured ? FVector2D(2.0f, 3.0f) : FVector2D(1.0f, 1.0f);
		Draw.bLit = bLit;
		Draw.bTextured = bTextured;
		Draw.bBlend = bTextured;
		Draw.Lights.Ambient = FVector(0.10f, 0.12f, 0.15f);
		if (bLit)
		{
			Draw.Lights.NumDirectional = 1;
			Draw.Lights.Directional[0].Direction = FVector(0.3f, -0.5f, 0.8f).GetSafeNormal();
			Draw.Lights.Directional[0].Color = FVector(1.0f, 0.9f, 0.8f);
		}
		return Draw;
	}

	/**
	 * The draw lit by NumPoint point lights too (N29): 2.5 m of range around points beside the mesh (in the world, its
	 * centre Distance forward), so that some vertices are beyond the range and some behind the light.
	 */
	FGSVertexDraw WithPointLights(FGSVertexDraw Draw, float Distance, int32 NumPoint)
	{
		const FVector Positions[2] = {
			FVector(40.0f, 30.0f, Distance - 60.0f), FVector(-70.0f, -20.0f, Distance + 30.0f)};
		const FVector Colors[2] = {FVector(0.9f, 0.6f, 0.3f), FVector(0.2f, 0.4f, 0.8f)};
		Draw.Lights.NumPoint = NumPoint;
		for (int32 Index = 0; Index < NumPoint; ++Index)
		{
			Draw.Lights.Point[Index].Position = Positions[Index];
			Draw.Lights.Point[Index].Color = Colors[Index];
			Draw.Lights.Point[Index].Radius = 250.0f;
		}
		return Draw;
	}

	/** The draw with a linear fog across its depth (N15): no fog at 3 m, the fog's colour at 8 m. */
	FGSVertexDraw Fogged(FGSVertexDraw Draw)
	{
		Draw.Fog = FGSVertexFog::MakeLinear(300.0f, 800.0f);
		return Draw;
	}

	/** Whether the batch's vertices are inside the guard band and between the near and far planes (plan D8). */
	bool IsInsideGuardBand(
		const FGSVertexDraw& Draw, const FGSVertexBatch& Batch, const FGSDrawEnvironment& Environment)
	{
		FGSClipVertex Vertices[FGSVertexBatch::MaxVertices];
		EGSStripTriangle Triangles[FGSVertexBatch::MaxVertices];
		FGSPrimitiveEmitter::TransformVertexBatch(Draw, Batch, Vertices, Triangles);
		const float GuardX = 2000.0f / (float(Environment.Width) * 0.5f);
		const float GuardY = 2000.0f / (float(Environment.Height) * 0.5f);
		for (uint32 Index = 0; Index < Batch.NumVertices; ++Index)
		{
			const FVector4& Clip = Vertices[Index].Clip;
			if (Clip.Z < 0.0f || Clip.Z > Clip.W || FMath::Abs(Clip.X) > GuardX * Clip.W ||
				FMath::Abs(Clip.Y) > GuardY * Clip.W)
			{
				return false;
			}
		}
		return true;
	}

	/** The draw of a skinned batch. */
	FGSVertexDraw AsSkinned(FGSVertexDraw Draw)
	{
		Draw.bSkinned = true;
		return Draw;
	}

	/** Moves a draw across the screen by Offset of its half width (for the side by side picture). */
	FGSVertexDraw Shifted(const FGSVertexDraw& Draw, float Offset)
	{
		FGSVertexDraw Moved = Draw;
		FMatrix Shift = FMatrix::Identity;
		Shift.M[3][0] = Offset;
		Moved.LocalToClip = Draw.LocalToClip * Shift;
		Moved.bTextured = false;
		Moved.bBlend = false;
		return Moved;
	}

} // namespace

// Checks VU1's microprograms against the C++ emitter (Docs/PLANS/ps2-shipping.md N14, D2) on fixed batches, then shows
// the batches, VU1's (the frame's own VIF1 chain, with XGKICK) on the left and the emitter's on the right, frame after
// frame.
int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));

	if (!FPS2VU1::IsEnabled())
	{
		UE_LOG(LogVU1Conformance, Error, "VU1Conformance: -novu1 leaves nothing to check");
		return 1;
	}
	// The display, VIF1's DMA channel and the microprograms (FPS2RHI::InitDisplay uploads them).
	if (!FPS2RHI::InitDisplay(ScreenWidth, ScreenHeight, EGSPixelFormat::PSMCT32))
	{
		UE_LOG(LogVU1Conformance, Error, "VU1Conformance: the display did not initialize");
		return 1;
	}
	const FGSDrawEnvironment Environment = FPS2RHI::GetDrawEnvironment();

	struct FTestDraw
	{
		const TCHAR* Name;
		FGSVertexDraw Draw;
		bool bSkinned = false;
	};
	const FTestDraw Draws[] = {
		{"StaticUnlit", MakeDraw(false, false, 0.01f, 500.0f, FRotator(10.0f, 25.0f, 5.0f), FVector(1.1f, 0.9f, 1.2f))},
		{"StaticUnlit textured",
			MakeDraw(false, true, 0.01f, 450.0f, FRotator(-20.0f, 60.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f))},
		{"StaticLit", MakeDraw(true, false, 0.01f, 500.0f, FRotator(30.0f, -40.0f, 15.0f), FVector(1.3f, 0.8f, 1.0f))},
		{"StaticLit textured mirrored",
			MakeDraw(true, true, 0.01f, 550.0f, FRotator(0.0f, 90.0f, 45.0f), FVector(-1.0f, 1.2f, 0.9f))},
		{"StaticUnlit fogged",
			Fogged(MakeDraw(false, false, 0.01f, 500.0f, FRotator(-5.0f, 15.0f, 20.0f), FVector(1.2f, 1.0f, 0.9f)))},
		{"StaticLit textured fogged",
			Fogged(MakeDraw(true, true, 0.01f, 600.0f, FRotator(15.0f, -30.0f, 0.0f), FVector(1.0f, 1.1f, 1.0f)))},
		{"SkinnedUnlit", AsSkinned(MakeDraw(false, false, 0.01f, 500.0f, FRotator(-10.0f, 35.0f, 0.0f), FVector(1.0f))),
			true},
		{"SkinnedLit textured",
			AsSkinned(MakeDraw(true, true, 0.01f, 520.0f, FRotator(20.0f, -70.0f, 10.0f), FVector(1.1f, 1.1f, 1.1f))),
			true},
		{"SkinnedLit mirrored",
			AsSkinned(MakeDraw(true, false, 0.01f, 480.0f, FRotator(0.0f, 120.0f, -30.0f), FVector(-1.0f, 1.0f, 1.0f))),
			true},
		{"SkinnedLit textured fogged",
			AsSkinned(Fogged(MakeDraw(true, true, 0.01f, 560.0f, FRotator(-15.0f, 45.0f, 5.0f), FVector(1.0f)))), true},
		{"StaticLit one point light",
			WithPointLights(
				MakeDraw(true, false, 0.01f, 500.0f, FRotator(10.0f, 60.0f, -20.0f), FVector(1.0f)), 500.0f, 1)},
		{"StaticLit textured two point lights",
			WithPointLights(
				MakeDraw(true, true, 0.01f, 520.0f, FRotator(-25.0f, 10.0f, 0.0f), FVector(1.2f)), 520.0f, 2)},
		{"SkinnedLit two point lights",
			AsSkinned(WithPointLights(
				MakeDraw(true, false, 0.01f, 500.0f, FRotator(5.0f, -35.0f, 15.0f), FVector(1.0f)), 500.0f, 2)),
			true},
		{"StaticUnlit off the view", MakeDraw(false, false, 0.03f, 900.0f, FRotator(5.0f, 5.0f, 5.0f), FVector(1.0f))},
	};
	constexpr uint32 BatchSizes[] = {3, 4, 17, 40, 63, 64};
	// A skinned batch holds at most 48 vertices; its palette 1 to 24 bones.
	constexpr uint32 SkinnedBatchSizes[] = {3, 4, 17, 33, 47, 48};
	constexpr uint32 SkinnedBones[] = {1, 2, 7, 13, 24, 24};
	constexpr int32 NumDraws = int32(UE_ARRAY_COUNT(Draws));
	constexpr int32 NumSizes = int32(UE_ARRAY_COUNT(BatchSizes));
	static_assert(UE_ARRAY_COUNT(SkinnedBatchSizes) == NumSizes && UE_ARRAY_COUNT(SkinnedBones) == NumSizes, "");
	static FTestStreams Streams[NumDraws][NumSizes];
	const auto GetSize = [&](int32 DrawIndex, int32 SizeIndex)
	{ return Draws[DrawIndex].bSkinned ? SkinnedBatchSizes[SizeIndex] : BatchSizes[SizeIndex]; };

	FRandom Random;
	FReport Report;
	for (int32 DrawIndex = 0; DrawIndex < NumDraws; ++DrawIndex)
	{
		for (int32 SizeIndex = 0; SizeIndex < NumSizes; ++SizeIndex)
		{
			FTestStreams& Data = Streams[DrawIndex][SizeIndex];
			const uint32 NumVertices = GetSize(DrawIndex, SizeIndex);
			MakeStrips(Data, NumVertices, 15000, Random);
			if (Draws[DrawIndex].bSkinned)
			{
				MakeSkin(Data, NumVertices, SkinnedBones[SizeIndex], Random);
			}
			FGSVertexBatch Batch;
			Batch.NumVertices = NumVertices;
			Batch.Positions = Data.Positions;
			Batch.Normals = Data.Normals;
			Batch.Colors = Data.Colors;
			Batch.TexCoords = Data.TexCoords;
			Batch.TexCoordOffset[0] = float(Random.Range(-3, 3));
			Batch.TexCoordOffset[1] = float(Random.Range(-3, 3));
			SetSkin(Batch, Data, Draws[DrawIndex].bSkinned);
			const FGSVertexDraw& Draw = Draws[DrawIndex].Draw;
			if (!IsInsideGuardBand(Draw, Batch, Environment))
			{
				UE_LOG(LogVU1Conformance, Error,
					"VU1Conformance: %s, %u vertices: the test's batch leaves the guard band", Draws[DrawIndex].Name,
					Batch.NumVertices);
				++Report.NumFailures;
				continue;
			}
			CheckBatch(Draws[DrawIndex].Name, Draw, Batch, Environment, Report);
		}
	}
	UE_LOG(LogVU1Conformance, Display,
		"VU1Conformance: %d batch(es), %d triangle(s) compared, %d culled by one side only (slivers); largest "
		"differences: XY %d (1/16 px), Z %d, RGBA %d, STQ %d ulp, F %d",
		Report.NumBatches, Report.NumTriangles, Report.NumDisagreements, Report.MaxXYSeen, Report.MaxZSeen,
		Report.MaxColorSeen, Report.MaxUlpsSeen, Report.MaxFogSeen);
	UE_LOG(LogVU1Conformance, Display, "VU1Conformance: %s (%d batch(es), %d failed)",
		Report.NumFailures == 0 ? "PASSED" : "FAILED", Report.NumBatches, Report.NumFailures);

	// The picture: every batch, VU1's on the left (through the RHI's frame chain) and the emitter's on the right.
	for (;;)
	{
		FPS2RHI::ClearColor(0.1f, 0.1f, 0.12f);
		FMemMark Mark(FMemStack::Get());
		FGSCommandList List;
		FGSPrimitiveEmitter Emitter(Environment, List);
		for (int32 DrawIndex = 0; DrawIndex < NumDraws - 1; ++DrawIndex)
		{
			const int32 Left = List.AddVertexDraw(Shifted(Draws[DrawIndex].Draw, -0.5f));
			const FGSVertexDraw Right = Shifted(Draws[DrawIndex].Draw, 0.5f);
			for (int32 SizeIndex = 0; SizeIndex < NumSizes; ++SizeIndex)
			{
				const FTestStreams& Data = Streams[DrawIndex][SizeIndex];
				FGSVertexBatch Batch;
				Batch.Draw = Left;
				Batch.NumVertices = GetSize(DrawIndex, SizeIndex);
				Batch.Positions = Data.Positions;
				Batch.Normals = Data.Normals;
				Batch.Colors = Data.Colors;
				Batch.TexCoords = Data.TexCoords;
				SetSkin(Batch, Data, Draws[DrawIndex].bSkinned);
				List.DrawVertexBatch(Batch);
				Emitter.SetFog(Right.Fog);
				Emitter.BeginStrip(false, false, true);
				Emitter.AddVertexBatch(Right, Batch);
			}
		}
		FPS2RHI::Submit(List);
		FPS2RHI::WaitVSync();
	}
}
