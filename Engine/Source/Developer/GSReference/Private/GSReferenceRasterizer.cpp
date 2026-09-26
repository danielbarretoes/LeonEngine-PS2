#include "GSReferenceRasterizer.h"

#include <cmath>

namespace
{

	/**
	 * The reference computes in double precision (a 32-bit Z does not fit a float's mantissa); FMath is float only for
	 * the EE, and this module only runs on the host.
	 */
	double FloorDouble(double Value)
	{
		return std::floor(Value);
	}

	uint8 ClampByte(int32 Value)
	{
		return uint8(FMath::Clamp(Value, 0, 255));
	}

	/** A * B = (A x B) >> 7, clamped (manual 3.4.9). */
	uint8 Multiply7(int32 A, int32 B)
	{
		return ClampByte((A * B) >> 7);
	}

	/** Value >> 7 toward minus infinity (a blend's (A - B) * C may be negative). */
	int32 FloorShift7(int32 Value)
	{
		return Value >= 0 ? Value >> 7 : -((-Value + 127) >> 7);
	}

	/** The GS drops the lower 8 bits of S, T and Q's mantissas (manual 3.4.4). */
	double TruncateMantissa(float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Bits &= 0xffffff00u;
		float Truncated = 0.0f;
		FMemory::Memcpy(&Truncated, &Bits, sizeof(Truncated));
		return double(Truncated);
	}

	int32 FloorDiv16(int32 Value)
	{
		return Value >= 0 ? Value / 16 : -((-Value + 15) / 16);
	}

	int32 CeilDiv16(int32 Value)
	{
		return -FloorDiv16(-Value);
	}

	int32 RoundToByte(double Value)
	{
		return FMath::Clamp(int32(FloorDouble(Value + 0.5)), 0, 255);
	}

	uint32 MaxZ(EGSPixelFormat Format)
	{
		switch (Format)
		{
			case EGSPixelFormat::PSMZ24:
				return 0xffffffu;
			case EGSPixelFormat::PSMZ16:
			case EGSPixelFormat::PSMZ16S:
				return 0xffffu;
			default:
				return 0xffffffffu;
		}
	}

	bool IsColor16(EGSPixelFormat Format)
	{
		return Format == EGSPixelFormat::PSMCT16 || Format == EGSPixelFormat::PSMCT16S;
	}

	bool IsClutFormat(EGSPixelFormat Format)
	{
		return Format == EGSPixelFormat::PSMT8 || Format == EGSPixelFormat::PSMT4;
	}

	/** Where CLUT entry Index of an IDTEX8 CSM1 CLUT is in its 16 x 16 rectangle: bits 3 and 4 swapped (2.7.3). */
	void ClutPosition8(uint32 Index, uint32& OutX, uint32& OutY)
	{
		const uint32 Position = (Index & ~0x18u) | ((Index & 0x08u) << 1) | ((Index & 0x10u) >> 1);
		OutX = Position % 16;
		OutY = Position / 16;
	}

	/** The frame buffer's 32-bit pre-conversion write mask as the 16-bit pixel's bits (3.9.5). */
	uint32 Mask16(uint32 Mask32)
	{
		return ((Mask32 >> 3) & 0x1f) | (((Mask32 >> 11) & 0x1f) << 5) | (((Mask32 >> 19) & 0x1f) << 10) |
			(((Mask32 >> 31) & 1) << 15);
	}

	bool AlphaTestPasses(EGSAlphaTest Test, uint8 Alpha, uint8 Reference)
	{
		switch (Test)
		{
			case EGSAlphaTest::Never:
				return false;
			case EGSAlphaTest::Always:
				return true;
			case EGSAlphaTest::Less:
				return Alpha < Reference;
			case EGSAlphaTest::LessEqual:
				return Alpha <= Reference;
			case EGSAlphaTest::Equal:
				return Alpha == Reference;
			case EGSAlphaTest::GreaterEqual:
				return Alpha >= Reference;
			case EGSAlphaTest::Greater:
				return Alpha > Reference;
			case EGSAlphaTest::NotEqual:
				return Alpha != Reference;
		}
		return true;
	}

} // namespace

FGSReferenceRasterizer::FGSReferenceRasterizer()
{
	for (FContext& Context : Contexts)
	{
		Context.Scissor.SCAX1 = 2047;
		Context.Scissor.SCAY1 = 2047;
	}
}

void FGSReferenceRasterizer::Execute(const FGSCommandList& List)
{
	for (const FGSRegisterWrite& Write : List.GetWrites())
	{
		WriteRegister(Write, List);
	}
}

void FGSReferenceRasterizer::WriteRegister(const FGSRegisterWrite& Write, const FGSCommandList& List)
{
	const uint64 Value = Write.Value;
	switch (Write.Register)
	{
		case EGSRegister::PRIM:
			Prim = FGSPrim::Decode(Value);
			Queue.Reset();
			NumVertices = 0;
			break;
		case EGSRegister::RGBAQ:
			RGBAQ = FGSRGBAQ::Decode(Value);
			break;
		case EGSRegister::ST:
			ST = FGSST::Decode(Value);
			break;
		case EGSRegister::UV:
			UV = FGSUV::Decode(Value);
			break;
		case EGSRegister::FOG:
			Fog = FGSFog::Decode(Value);
			break;
		case EGSRegister::XYZ2:
		case EGSRegister::XYZ3:
		{
			const FGSXYZ Vertex = FGSXYZ::Decode(Value);
			AddVertex(Vertex.X, Vertex.Y, Vertex.Z, Fog.F, Write.Register == EGSRegister::XYZ2);
			break;
		}
		case EGSRegister::XYZF2:
		case EGSRegister::XYZF3:
		{
			const FGSXYZF Vertex = FGSXYZF::Decode(Value);
			AddVertex(Vertex.X, Vertex.Y, Vertex.Z, Vertex.F, Write.Register == EGSRegister::XYZF2);
			break;
		}
		case EGSRegister::TEX0_1:
		case EGSRegister::TEX0_2:
		{
			const FGSTex0 Tex0 = FGSTex0::Decode(Value);
			Contexts[uint8(Write.Register) - uint8(EGSRegister::TEX0_1)].Tex0 = Tex0;
			if (Tex0.CLD == 1 && IsClutFormat(Tex0.PSM))
			{
				LoadClut(Tex0);
			}
			break;
		}
		case EGSRegister::TEX1_1:
		case EGSRegister::TEX1_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::TEX1_1)].Tex1 = FGSTex1::Decode(Value);
			break;
		case EGSRegister::CLAMP_1:
		case EGSRegister::CLAMP_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::CLAMP_1)].Clamp = FGSClamp::Decode(Value);
			break;
		case EGSRegister::MIPTBP1_1:
		case EGSRegister::MIPTBP1_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::MIPTBP1_1)].MipTbp1 = FGSMipTbp::Decode(Value);
			break;
		case EGSRegister::MIPTBP2_1:
		case EGSRegister::MIPTBP2_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::MIPTBP2_1)].MipTbp2 = FGSMipTbp::Decode(Value);
			break;
		case EGSRegister::XYOFFSET_1:
		case EGSRegister::XYOFFSET_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::XYOFFSET_1)].XYOffset = FGSXYOffset::Decode(Value);
			break;
		case EGSRegister::SCISSOR_1:
		case EGSRegister::SCISSOR_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::SCISSOR_1)].Scissor = FGSScissor::Decode(Value);
			break;
		case EGSRegister::ALPHA_1:
		case EGSRegister::ALPHA_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::ALPHA_1)].Alpha = FGSAlpha::Decode(Value);
			break;
		case EGSRegister::TEST_1:
		case EGSRegister::TEST_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::TEST_1)].Test = FGSTest::Decode(Value);
			break;
		case EGSRegister::FBA_1:
		case EGSRegister::FBA_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::FBA_1)].bFba = (Value & 1) != 0;
			break;
		case EGSRegister::FRAME_1:
		case EGSRegister::FRAME_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::FRAME_1)].Frame = FGSFrame::Decode(Value);
			break;
		case EGSRegister::ZBUF_1:
		case EGSRegister::ZBUF_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::ZBUF_1)].ZBuf = FGSZBuf::Decode(Value);
			break;
		case EGSRegister::PRMODECONT:
			// Only PRIM's attributes (AC = 1) are in the contract.
			check((Value & 1) == 1);
			break;
		case EGSRegister::FOGCOL:
			FogCol = FGSFogCol::Decode(Value);
			break;
		case EGSRegister::DIMX:
			Dimx = FGSDimx::Decode(Value);
			break;
		case EGSRegister::DTHE:
			bDither = (Value & 1) != 0;
			break;
		case EGSRegister::COLCLAMP:
			bColorClamp = (Value & 1) != 0;
			break;
		case EGSRegister::PABE:
			bPixelAlphaBlend = (Value & 1) != 0;
			break;
		case EGSRegister::TEXA:
			TexA = FGSTexA::Decode(Value);
			break;
		case EGSRegister::TEXCLUT:
		case EGSRegister::TEXFLUSH:
			// CSM2 is not in the contract; the reference has no texture cache to flush.
			break;
		case EGSRegister::BITBLTBUF:
			BitBltBuf = FGSBitBltBuf::Decode(Value);
			break;
		case EGSRegister::TRXPOS:
			TrxPos = FGSTrxPos::Decode(Value);
			break;
		case EGSRegister::TRXREG:
			TrxReg = FGSTrxReg::Decode(Value);
			break;
		case EGSRegister::TRXDIR:
			check(EGSTransferDirection(Value & 3) == EGSTransferDirection::HostToLocal);
			break;
		case EGSRegister::HWREG:
			Transfer(List.GetImageData()[int32(Value)]);
			break;
		default:
			checkNoEntry();
			break;
	}
}

void FGSReferenceRasterizer::AddVertex(uint16 X, uint16 Y, uint32 Z, uint8 F, bool bKick)
{
	const FContext& Context = GetContext();
	FVertex Vertex;
	Vertex.X = int32(X) - int32(Context.XYOffset.OFX);
	Vertex.Y = int32(Y) - int32(Context.XYOffset.OFY);
	Vertex.Z = Z;
	Vertex.F = F;
	Vertex.Color = RGBAQ;
	Vertex.ST = ST;
	Vertex.UV = UV;
	++NumVertices;

	switch (Prim.Type)
	{
		case EGSPrimitive::Point:
			if (bKick)
			{
				DrawPoint(Vertex);
			}
			break;
		case EGSPrimitive::Line:
		case EGSPrimitive::Sprite:
			Queue.Add(Vertex);
			if (Queue.Num() == 2)
			{
				if (bKick)
				{
					Prim.Type == EGSPrimitive::Line ? DrawLine(Queue[0], Queue[1]) : DrawSprite(Queue[0], Queue[1]);
				}
				Queue.Reset();
			}
			break;
		case EGSPrimitive::LineStrip:
			Queue.Add(Vertex);
			if (Queue.Num() == 2)
			{
				if (bKick)
				{
					DrawLine(Queue[0], Queue[1]);
				}
				Queue.RemoveAt(0);
			}
			break;
		case EGSPrimitive::Triangle:
			Queue.Add(Vertex);
			if (Queue.Num() == 3)
			{
				if (bKick)
				{
					DrawTriangle(Queue[0], Queue[1], Queue[2]);
				}
				Queue.Reset();
			}
			break;
		case EGSPrimitive::TriangleStrip:
			Queue.Add(Vertex);
			if (Queue.Num() == 3)
			{
				if (bKick)
				{
					DrawTriangle(Queue[0], Queue[1], Queue[2]);
				}
				Queue.RemoveAt(0);
			}
			break;
		case EGSPrimitive::TriangleFan:
			if (NumVertices == 1)
			{
				FanFirst = Vertex;
				break;
			}
			Queue.Add(Vertex);
			if (Queue.Num() == 2)
			{
				if (bKick)
				{
					DrawTriangle(FanFirst, Queue[0], Queue[1]);
				}
				Queue.RemoveAt(0);
			}
			break;
	}
}

void FGSReferenceRasterizer::Transfer(const TArray<uint8>& Data)
{
	const uint32 Bits = GSBitsPerPixel(BitBltBuf.DPSM);
	const uint32 Base = uint32(BitBltBuf.DBP) * 64;
	const uint32 Width = uint32(BitBltBuf.DBW) * 64;
	uint64 BitCursor = 0;
	for (uint32 Y = 0; Y < TrxReg.RRH; ++Y)
	{
		for (uint32 X = 0; X < TrxReg.RRW; ++X)
		{
			// Little endian, the first 4-bit pixel in the low nibble (manual 4.3).
			uint32 Value = 0;
			for (uint32 Bit = 0; Bit < Bits; ++Bit)
			{
				const uint64 Source = BitCursor + Bit;
				Value |= uint32((Data[int32(Source / 8)] >> (Source % 8)) & 1) << Bit;
			}
			BitCursor += Bits;
			const uint32 PixelX = TrxPos.DSAX + X;
			const uint32 PixelY = TrxPos.DSAY + Y;
			if (Bits == 24)
			{
				// A 24-bit pixel keeps its unused high byte.
				Value |= Memory.ReadPixel(Base, Width, BitBltBuf.DPSM, PixelX, PixelY) & 0xff000000u;
			}
			Memory.WritePixel(Base, Width, BitBltBuf.DPSM, PixelX, PixelY, Value);
		}
	}
}

void FGSReferenceRasterizer::LoadClut(const FGSTex0& Tex0)
{
	const bool bIndex8 = Tex0.PSM == EGSPixelFormat::PSMT8;
	const uint32 NumEntries = bIndex8 ? 256 : 16;
	const uint32 Base = uint32(Tex0.CBP) * 64;
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		uint32 X = Index % 8;
		uint32 Y = Index / 8;
		if (bIndex8)
		{
			ClutPosition8(Index, X, Y);
		}
		// The temporary buffer takes the entries at CSA * 16 (manual 3.4.7).
		ClutBuffer[((uint32(Tex0.CSA) * 16) + Index) % 512] = Memory.ReadPixel(Base, 64, Tex0.CPSM, X, Y);
	}
}

void FGSReferenceRasterizer::DrawPoint(const FVertex& Vertex)
{
	FFragment Fragment;
	Fragment.X = FloorDiv16(Vertex.X + 8);
	Fragment.Y = FloorDiv16(Vertex.Y + 8);
	Fragment.Z = double(Vertex.Z);
	Fragment.F = Vertex.F;
	Fragment.R = Vertex.Color.R;
	Fragment.G = Vertex.Color.G;
	Fragment.B = Vertex.Color.B;
	Fragment.A = Vertex.Color.A;
	Fragment.S = TruncateMantissa(Vertex.ST.S);
	Fragment.T = TruncateMantissa(Vertex.ST.T);
	Fragment.Q = TruncateMantissa(Vertex.Color.Q);
	Fragment.U = Vertex.UV.U;
	Fragment.V = Vertex.UV.V;
	ShadePixel(Fragment);
}

void FGSReferenceRasterizer::DrawLine(const FVertex& From, const FVertex& To)
{
	const int32 DeltaX = To.X - From.X;
	const int32 DeltaY = To.Y - From.Y;
	const bool bMajorX = FMath::Abs(DeltaX) >= FMath::Abs(DeltaY);
	const int32 MajorFrom = FloorDiv16((bMajorX ? From.X : From.Y) + 8);
	const int32 MajorTo = FloorDiv16((bMajorX ? To.X : To.Y) + 8);
	const int32 MajorDelta = bMajorX ? DeltaX : DeltaY;
	if (MajorFrom == MajorTo || MajorDelta == 0)
	{
		return;
	}
	const int32 Step = MajorTo > MajorFrom ? 1 : -1;
	// The end point is not drawn (manual 3.2.9).
	for (int32 Major = MajorFrom; Major != MajorTo; Major += Step)
	{
		const double T =
			FMath::Clamp(double((Major * 16) - (bMajorX ? From.X : From.Y)) / double(MajorDelta), 0.0, 1.0);
		const double Minor = (bMajorX ? From.Y : From.X) + (T * double(bMajorX ? DeltaY : DeltaX));
		FFragment Fragment;
		const int32 MinorPixel = int32(FloorDouble((Minor + 8.0) / 16.0));
		Fragment.X = bMajorX ? Major : MinorPixel;
		Fragment.Y = bMajorX ? MinorPixel : Major;
		const FGSRGBAQ& Flat = To.Color;
		const auto Lerp = [T](double A, double B) { return A + ((B - A) * T); };
		Fragment.Z = Lerp(From.Z, To.Z);
		Fragment.F = Lerp(From.F, To.F);
		Fragment.R = Prim.bGouraud ? Lerp(From.Color.R, To.Color.R) : Flat.R;
		Fragment.G = Prim.bGouraud ? Lerp(From.Color.G, To.Color.G) : Flat.G;
		Fragment.B = Prim.bGouraud ? Lerp(From.Color.B, To.Color.B) : Flat.B;
		Fragment.A = Prim.bGouraud ? Lerp(From.Color.A, To.Color.A) : Flat.A;
		Fragment.S = Lerp(TruncateMantissa(From.ST.S), TruncateMantissa(To.ST.S));
		Fragment.T = Lerp(TruncateMantissa(From.ST.T), TruncateMantissa(To.ST.T));
		Fragment.Q = Lerp(TruncateMantissa(From.Color.Q), TruncateMantissa(To.Color.Q));
		Fragment.U = Lerp(From.UV.U, To.UV.U);
		Fragment.V = Lerp(From.UV.V, To.UV.V);
		ShadePixel(Fragment);
	}
}

void FGSReferenceRasterizer::DrawTriangle(const FVertex& V0, const FVertex& V1, const FVertex& V2)
{
	const auto Edge = [](const FVertex& A, const FVertex& B, int64 PX, int64 PY)
	{ return (int64(B.X - A.X) * (PY - A.Y)) - (int64(B.Y - A.Y) * (PX - A.X)); };
	const int64 Area = Edge(V0, V1, V2.X, V2.Y);
	if (Area == 0)
	{
		return;
	}
	const int64 Sign = Area > 0 ? 1 : -1;
	// A pixel center on a side is drawn on the left and top sides (manual 3.2.9): the side whose inside lies toward
	// +X, or toward +Y when the side is horizontal.
	const auto IsTopLeft = [Sign](const FVertex& A, const FVertex& B)
	{
		const int64 NormalX = -Sign * int64(B.Y - A.Y);
		const int64 NormalY = Sign * int64(B.X - A.X);
		return NormalX > 0 || (NormalX == 0 && NormalY > 0);
	};
	const bool bTopLeft0 = IsTopLeft(V1, V2);
	const bool bTopLeft1 = IsTopLeft(V2, V0);
	const bool bTopLeft2 = IsTopLeft(V0, V1);

	const FContext& Context = GetContext();
	const int32 MinX = FMath::Max(CeilDiv16(FMath::Min3(V0.X, V1.X, V2.X)), int32(Context.Scissor.SCAX0));
	const int32 MaxX = FMath::Min(FloorDiv16(FMath::Max3(V0.X, V1.X, V2.X)), int32(Context.Scissor.SCAX1));
	const int32 MinY = FMath::Max(CeilDiv16(FMath::Min3(V0.Y, V1.Y, V2.Y)), int32(Context.Scissor.SCAY0));
	const int32 MaxY = FMath::Min(FloorDiv16(FMath::Max3(V0.Y, V1.Y, V2.Y)), int32(Context.Scissor.SCAY1));
	const double InvArea = 1.0 / double(Area * Sign);
	// Flat shading takes the color set before the kick: the last vertex's (manual 3.2.8).
	const FGSRGBAQ& Flat = V2.Color;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int64 W0 = Edge(V1, V2, int64(X) * 16, int64(Y) * 16) * Sign;
			const int64 W1 = Edge(V2, V0, int64(X) * 16, int64(Y) * 16) * Sign;
			const int64 W2 = Edge(V0, V1, int64(X) * 16, int64(Y) * 16) * Sign;
			if (W0 < 0 || W1 < 0 || W2 < 0 || (W0 == 0 && !bTopLeft0) || (W1 == 0 && !bTopLeft1) ||
				(W2 == 0 && !bTopLeft2))
			{
				continue;
			}
			const double L0 = double(W0) * InvArea;
			const double L1 = double(W1) * InvArea;
			const double L2 = double(W2) * InvArea;
			const auto Mix = [L0, L1, L2](double A, double B, double C) { return (A * L0) + (B * L1) + (C * L2); };
			FFragment Fragment;
			Fragment.X = X;
			Fragment.Y = Y;
			Fragment.Z = Mix(V0.Z, V1.Z, V2.Z);
			Fragment.F = Mix(V0.F, V1.F, V2.F);
			Fragment.R = Prim.bGouraud ? Mix(V0.Color.R, V1.Color.R, V2.Color.R) : Flat.R;
			Fragment.G = Prim.bGouraud ? Mix(V0.Color.G, V1.Color.G, V2.Color.G) : Flat.G;
			Fragment.B = Prim.bGouraud ? Mix(V0.Color.B, V1.Color.B, V2.Color.B) : Flat.B;
			Fragment.A = Prim.bGouraud ? Mix(V0.Color.A, V1.Color.A, V2.Color.A) : Flat.A;
			Fragment.S = Mix(TruncateMantissa(V0.ST.S), TruncateMantissa(V1.ST.S), TruncateMantissa(V2.ST.S));
			Fragment.T = Mix(TruncateMantissa(V0.ST.T), TruncateMantissa(V1.ST.T), TruncateMantissa(V2.ST.T));
			Fragment.Q = Mix(TruncateMantissa(V0.Color.Q), TruncateMantissa(V1.Color.Q), TruncateMantissa(V2.Color.Q));
			Fragment.U = Mix(V0.UV.U, V1.UV.U, V2.UV.U);
			Fragment.V = Mix(V0.UV.V, V1.UV.V, V2.UV.V);
			ShadePixel(Fragment);
		}
	}
}

void FGSReferenceRasterizer::DrawSprite(const FVertex& V0, const FVertex& V1)
{
	const FContext& Context = GetContext();
	const int32 Left = FMath::Min(V0.X, V1.X);
	const int32 Right = FMath::Max(V0.X, V1.X);
	const int32 Top = FMath::Min(V0.Y, V1.Y);
	const int32 Bottom = FMath::Max(V0.Y, V1.Y);
	// Top and left sides drawn, bottom and right not (manual 3.2.9).
	const int32 MinX = FMath::Max(CeilDiv16(Left), int32(Context.Scissor.SCAX0));
	const int32 MaxX = FMath::Min(CeilDiv16(Right) - 1, int32(Context.Scissor.SCAX1));
	const int32 MinY = FMath::Max(CeilDiv16(Top), int32(Context.Scissor.SCAY0));
	const int32 MaxY = FMath::Min(CeilDiv16(Bottom) - 1, int32(Context.Scissor.SCAY1));
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		const double TY = V1.Y != V0.Y ? double((Y * 16) - V0.Y) / double(V1.Y - V0.Y) : 0.0;
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const double TX = V1.X != V0.X ? double((X * 16) - V0.X) / double(V1.X - V0.X) : 0.0;
			const auto LerpX = [TX](double A, double B) { return A + ((B - A) * TX); };
			const auto LerpY = [TY](double A, double B) { return A + ((B - A) * TY); };
			// Z, fog and color are the second vertex's (manual 3.2.8); the texture spans the rectangle.
			FFragment Fragment;
			Fragment.X = X;
			Fragment.Y = Y;
			Fragment.Z = double(V1.Z);
			Fragment.F = V1.F;
			Fragment.R = V1.Color.R;
			Fragment.G = V1.Color.G;
			Fragment.B = V1.Color.B;
			Fragment.A = V1.Color.A;
			Fragment.S = LerpX(TruncateMantissa(V0.ST.S), TruncateMantissa(V1.ST.S));
			Fragment.T = LerpY(TruncateMantissa(V0.ST.T), TruncateMantissa(V1.ST.T));
			Fragment.Q = TruncateMantissa(V1.Color.Q);
			Fragment.U = LerpX(V0.UV.U, V1.UV.U);
			Fragment.V = LerpY(V0.UV.V, V1.UV.V);
			ShadePixel(Fragment);
		}
	}
}

FColor FGSReferenceRasterizer::ExpandColor(uint32 Value, EGSPixelFormat Format) const
{
	// 5-bit colors shifted left 3; the alpha from TEXA (manual 3.4.6).
	if (IsColor16(Format))
	{
		const uint8 R = uint8((Value & 0x1f) << 3);
		const uint8 G = uint8(((Value >> 5) & 0x1f) << 3);
		const uint8 B = uint8(((Value >> 10) & 0x1f) << 3);
		const bool bAlphaBit = ((Value >> 15) & 1) != 0;
		const bool bBlack = (R | G | B) == 0;
		const uint8 A = bAlphaBit ? TexA.TA1 : (TexA.bAlphaExpandBlack && bBlack ? 0 : TexA.TA0);
		return FColor(R, G, B, A);
	}
	const uint8 R = uint8(Value);
	const uint8 G = uint8(Value >> 8);
	const uint8 B = uint8(Value >> 16);
	const uint8 A = TexA.bAlphaExpandBlack && (R | G | B) == 0 ? 0 : TexA.TA0;
	return FColor(R, G, B, A);
}

int32 FGSReferenceRasterizer::Wrap(EGSWrapMode Mode, int32 Coordinate, int32 Size, uint32 Level, uint16 Min, uint16 Max)
{
	switch (Mode)
	{
		case EGSWrapMode::Repeat:
			return ((Coordinate % Size) + Size) % Size;
		case EGSWrapMode::Clamp:
			return FMath::Clamp(Coordinate, 0, Size - 1);
		case EGSWrapMode::RegionClamp:
			return FMath::Clamp(Coordinate, int32(Min >> Level), int32(Max >> Level));
		case EGSWrapMode::RegionRepeat:
			// MINU / MINV are the masks, MAXU / MAXV the fixed bits.
			return (Coordinate & int32(Min)) | int32(Max);
	}
	return Coordinate;
}

FColor FGSReferenceRasterizer::FetchTexel(const FContext& Context, uint32 Level, int32 U, int32 V) const
{
	const FGSTex0& Tex0 = Context.Tex0;
	const int32 Width = FMath::Max(1, (1 << Tex0.TW) >> Level);
	const int32 Height = FMath::Max(1, (1 << Tex0.TH) >> Level);
	const FGSClamp& Clamp = Context.Clamp;
	const int32 WrappedU = Wrap(Clamp.WMS, U, Width, Level, Clamp.MINU, Clamp.MAXU);
	const int32 WrappedV = Wrap(Clamp.WMT, V, Height, Level, Clamp.MINV, Clamp.MAXV);
	uint32 BasePointer = Tex0.TBP0;
	uint32 BufferWidth = Tex0.TBW;
	if (Level >= 1 && Level <= 3)
	{
		BasePointer = Context.MipTbp1.TBP[Level - 1];
		BufferWidth = Context.MipTbp1.TBW[Level - 1];
	}
	else if (Level >= 4)
	{
		BasePointer = Context.MipTbp2.TBP[Level - 4];
		BufferWidth = Context.MipTbp2.TBW[Level - 4];
	}
	const uint32 Raw = Memory.ReadPixel(
		BasePointer * 64, BufferWidth * 64, Tex0.PSM, uint32(WrappedU & 0x7ff), uint32(WrappedV & 0x7ff));
	switch (Tex0.PSM)
	{
		case EGSPixelFormat::PSMCT32:
			return FColor(uint8(Raw), uint8(Raw >> 8), uint8(Raw >> 16), uint8(Raw >> 24));
		case EGSPixelFormat::PSMCT24:
			return ExpandColor(Raw & 0xffffffu, EGSPixelFormat::PSMCT24);
		case EGSPixelFormat::PSMCT16:
		case EGSPixelFormat::PSMCT16S:
			return ExpandColor(Raw, EGSPixelFormat::PSMCT16);
		default:
		{
			// Through the CLUT's temporary buffer: IDTEX8 from entry 0, IDTEX4 from CSA * 16 (manual 3.4.7).
			const uint32 Entry = Tex0.PSM == EGSPixelFormat::PSMT8 ? Raw : (uint32(Tex0.CSA) * 16) + Raw;
			const uint32 Color = ClutBuffer[Entry % 512];
			if (Tex0.CPSM == EGSPixelFormat::PSMCT32)
			{
				return FColor(uint8(Color), uint8(Color >> 8), uint8(Color >> 16), uint8(Color >> 24));
			}
			return ExpandColor(Color, EGSPixelFormat::PSMCT16);
		}
	}
}

FColor FGSReferenceRasterizer::FilterLevel(
	const FContext& Context, uint32 Level, double U, double V, bool bBilinear) const
{
	const double Scale = 1.0 / double(1 << Level);
	const double LevelU = U * Scale;
	const double LevelV = V * Scale;
	if (!bBilinear)
	{
		// The texel whose square holds the point (manual 3.4.8).
		return FetchTexel(Context, Level, int32(FloorDouble(LevelU)), int32(FloorDouble(LevelV)));
	}
	// The four texels around the point, centers at .5 (manual 3.4.8).
	const double BaseU = LevelU - 0.5;
	const double BaseV = LevelV - 0.5;
	const int32 U0 = int32(FloorDouble(BaseU));
	const int32 V0 = int32(FloorDouble(BaseV));
	const double Alpha = BaseU - U0;
	const double Beta = BaseV - V0;
	const FColor A = FetchTexel(Context, Level, U0, V0);
	const FColor B = FetchTexel(Context, Level, U0 + 1, V0);
	const FColor C = FetchTexel(Context, Level, U0, V0 + 1);
	const FColor D = FetchTexel(Context, Level, U0 + 1, V0 + 1);
	const auto Blend = [Alpha, Beta](uint8 Ca, uint8 Cb, uint8 Cc, uint8 Cd)
	{
		return uint8(RoundToByte(((1.0 - Alpha) * (1.0 - Beta) * Ca) + (Alpha * (1.0 - Beta) * Cb) +
			((1.0 - Alpha) * Beta * Cc) + (Alpha * Beta * Cd)));
	};
	return FColor(
		Blend(A.R, B.R, C.R, D.R), Blend(A.G, B.G, C.G, D.G), Blend(A.B, B.B, C.B, D.B), Blend(A.A, B.A, C.A, D.A));
}

FColor FGSReferenceRasterizer::SampleTexture(const FContext& Context, const FFragment& Fragment) const
{
	const FGSTex0& Tex0 = Context.Tex0;
	const FGSTex1& Tex1 = Context.Tex1;
	double U = 0.0;
	double V = 0.0;
	if (Prim.bUseUV)
	{
		U = Fragment.U / 16.0;
		V = Fragment.V / 16.0;
	}
	else
	{
		// s = S / Q, t = T / Q (manual 3.4.10).
		const double Q = Fragment.Q != 0.0 ? Fragment.Q : 1.0e-30;
		U = (Fragment.S / Q) * double(1 << Tex0.TW);
		V = (Fragment.T / Q) * double(1 << Tex0.TH);
	}
	U = FMath::Clamp(U, -2047.0, 2047.0);
	V = FMath::Clamp(V, -2047.0, 2047.0);

	// LOD = (log2(1 / |Q|) << L) + K, or K (manual 3.4.12).
	const double K = double(Tex1.K) / 16.0;
	const double AbsQ = FMath::Abs(Fragment.Q);
	const double Lod = Tex1.bFixedLOD ? K : AbsQ > 0.0 ? (std::log2(1.0 / AbsQ) * double(1 << Tex1.L)) + K : 64.0;
	if (Lod <= 0.0)
	{
		return FilterLevel(Context, 0, U, V, Tex1.MMAG == EGSFilter::Linear);
	}
	const uint32 MaxLevel = FMath::Min<uint32>(Tex1.MXL, 6);
	switch (Tex1.MMIN)
	{
		case EGSFilter::Nearest:
			return FilterLevel(Context, 0, U, V, false);
		case EGSFilter::Linear:
			return FilterLevel(Context, 0, U, V, true);
		case EGSFilter::NearestMipmapNearest:
		case EGSFilter::LinearMipmapNearest:
		{
			const uint32 Level = FMath::Min(uint32(FloorDouble(Lod + 0.5)), MaxLevel);
			return FilterLevel(Context, Level, U, V, Tex1.MMIN == EGSFilter::LinearMipmapNearest);
		}
		default:
		{
			// Linear between levels m and m + 1 (trilinear with bilinear levels).
			const bool bBilinear = Tex1.MMIN == EGSFilter::LinearMipmapLinear;
			const uint32 Level = FMath::Min(uint32(FloorDouble(Lod)), MaxLevel);
			if (Level >= MaxLevel)
			{
				return FilterLevel(Context, MaxLevel, U, V, bBilinear);
			}
			const double Weight = Lod - double(Level);
			const FColor Near = FilterLevel(Context, Level, U, V, bBilinear);
			const FColor Far = FilterLevel(Context, Level + 1, U, V, bBilinear);
			const auto Mix = [Weight](uint8 A, uint8 B) { return uint8(RoundToByte(A + ((B - A) * Weight))); };
			return FColor(Mix(Near.R, Far.R), Mix(Near.G, Far.G), Mix(Near.B, Far.B), Mix(Near.A, Far.A));
		}
	}
}

void FGSReferenceRasterizer::ShadePixel(const FFragment& Fragment)
{
	const FContext& Context = GetContext();
	const FGSScissor& Scissor = Context.Scissor;
	if (Fragment.X < Scissor.SCAX0 || Fragment.X > Scissor.SCAX1 || Fragment.Y < Scissor.SCAY0 ||
		Fragment.Y > Scissor.SCAY1)
	{
		return;
	}
	int32 R = RoundToByte(Fragment.R);
	int32 G = RoundToByte(Fragment.G);
	int32 B = RoundToByte(Fragment.B);
	int32 A = RoundToByte(Fragment.A);

	if (Prim.bTextured)
	{
		// The texture function (manual 3.4.9).
		const FColor Texel = SampleTexture(Context, Fragment);
		const bool bRGBA = Context.Tex0.bRGBA;
		switch (Context.Tex0.TFX)
		{
			case EGSTextureFunction::Modulate:
				R = Multiply7(Texel.R, R);
				G = Multiply7(Texel.G, G);
				B = Multiply7(Texel.B, B);
				A = bRGBA ? Multiply7(Texel.A, A) : A;
				break;
			case EGSTextureFunction::Decal:
				R = Texel.R;
				G = Texel.G;
				B = Texel.B;
				A = bRGBA ? Texel.A : A;
				break;
			case EGSTextureFunction::Highlight:
			case EGSTextureFunction::Highlight2:
			{
				const int32 FragmentAlpha = A;
				R = ClampByte(Multiply7(Texel.R, R) + FragmentAlpha);
				G = ClampByte(Multiply7(Texel.G, G) + FragmentAlpha);
				B = ClampByte(Multiply7(Texel.B, B) + FragmentAlpha);
				if (bRGBA)
				{
					A = Context.Tex0.TFX == EGSTextureFunction::Highlight ? ClampByte(Texel.A + FragmentAlpha)
																		  : Texel.A;
				}
				break;
			}
		}
	}

	if (Prim.bFog)
	{
		// C = F * C + (0xff - F) * FOGCOL, A * B = (A x B) >> 8 (manual 3.5).
		const int32 F = RoundToByte(Fragment.F);
		R = ((F * R) + ((0xff - F) * FogCol.R)) >> 8;
		G = ((F * G) + ((0xff - F) * FogCol.G)) >> 8;
		B = ((F * B) + ((0xff - F) * FogCol.B)) >> 8;
	}

	// The pixel tests (manual 3.7): what the pixel may still write.
	const FGSFrame& Frame = Context.Frame;
	const FGSTest& Test = Context.Test;
	const uint32 FrameBase = uint32(Frame.FBP) * 2048;
	const uint32 FrameWidth = uint32(Frame.FBW) * 64;
	const uint32 PixelX = uint32(Fragment.X);
	const uint32 PixelY = uint32(Fragment.Y);
	bool bWriteRGB = true;
	bool bWriteA = true;
	bool bWriteZ = true;
	if (Test.bAlphaTest && !AlphaTestPasses(Test.ATST, uint8(A), Test.AREF))
	{
		switch (Test.AFAIL)
		{
			case EGSAlphaFail::Keep:
				bWriteRGB = bWriteA = bWriteZ = false;
				break;
			case EGSAlphaFail::FrameBufferOnly:
				bWriteZ = false;
				break;
			case EGSAlphaFail::ZBufferOnly:
				bWriteRGB = bWriteA = false;
				break;
			case EGSAlphaFail::RGBOnly:
				// RGB only in RGBA32; FB_ONLY otherwise.
				bWriteZ = false;
				bWriteA = Frame.PSM != EGSPixelFormat::PSMCT32;
				break;
		}
	}
	const uint32 Destination = Memory.ReadPixel(FrameBase, FrameWidth, Frame.PSM, PixelX, PixelY);
	if (Test.bDestinationAlphaTest)
	{
		bool bAlphaBit = true;
		if (Frame.PSM == EGSPixelFormat::PSMCT32)
		{
			bAlphaBit = ((Destination >> 31) & 1) != 0;
		}
		else if (IsColor16(Frame.PSM))
		{
			bAlphaBit = ((Destination >> 15) & 1) != 0;
		}
		if (Frame.PSM != EGSPixelFormat::PSMCT24 && bAlphaBit != Test.bDestinationAlphaOne)
		{
			return;
		}
	}
	const FGSZBuf& ZBuf = Context.ZBuf;
	const uint32 ZMax = MaxZ(ZBuf.PSM);
	const uint32 Z = uint32(FMath::Clamp(FloorDouble(Fragment.Z + 0.5), 0.0, double(ZMax)));
	const uint32 ZBase = uint32(ZBuf.ZBP) * 2048;
	switch (Test.ZTST)
	{
		case EGSDepthTest::Never:
			return;
		case EGSDepthTest::Always:
			break;
		case EGSDepthTest::GreaterEqual:
		case EGSDepthTest::Greater:
		{
			const uint32 Stored = Memory.ReadPixel(ZBase, FrameWidth, ZBuf.PSM, PixelX, PixelY) & ZMax;
			if (Test.ZTST == EGSDepthTest::GreaterEqual ? Z < Stored : Z <= Stored)
			{
				return;
			}
			break;
		}
	}
	if (!bWriteRGB && !bWriteA && !bWriteZ)
	{
		return;
	}

	// The destination color and alpha (RGBA16's alpha is 0 or 0x80, RGB24's 0x80: manual 3.8.1).
	int32 DestinationRGB[3] = {0, 0, 0};
	int32 DestinationAlpha = 0x80;
	if (IsColor16(Frame.PSM))
	{
		DestinationRGB[0] = int32((Destination & 0x1f) << 3);
		DestinationRGB[1] = int32(((Destination >> 5) & 0x1f) << 3);
		DestinationRGB[2] = int32(((Destination >> 10) & 0x1f) << 3);
		DestinationAlpha = ((Destination >> 15) & 1) != 0 ? 0x80 : 0;
	}
	else
	{
		DestinationRGB[0] = int32(Destination & 0xff);
		DestinationRGB[1] = int32((Destination >> 8) & 0xff);
		DestinationRGB[2] = int32((Destination >> 16) & 0xff);
		DestinationAlpha = Frame.PSM == EGSPixelFormat::PSMCT32 ? int32(Destination >> 24) : 0x80;
	}

	int32 Out[3] = {R, G, B};
	const bool bBlend = Prim.bAlphaBlend && (!bPixelAlphaBlend || (A & 0x80) != 0);
	if (bBlend)
	{
		// (A - B) * C >> 7 + D, not clamped until after dithering (manual 3.8).
		const FGSAlpha& Alpha = Context.Alpha;
		const int32 Source[3] = {R, G, B};
		const int32 Factor = Alpha.C == EGSBlendAlpha::Source ? A
			: Alpha.C == EGSBlendAlpha::Destination           ? DestinationAlpha
															  : int32(Alpha.FIX);
		for (int32 Channel = 0; Channel < 3; ++Channel)
		{
			const auto Pick = [&](EGSBlendColor Input)
			{
				return Input == EGSBlendColor::Source     ? Source[Channel]
					: Input == EGSBlendColor::Destination ? DestinationRGB[Channel]
														  : 0;
			};
			Out[Channel] = FloorShift7((Pick(Alpha.A) - Pick(Alpha.B)) * Factor) + Pick(Alpha.D);
		}
	}
	if (bDither && IsColor16(Frame.PSM))
	{
		const int32 Offset = Dimx.M[PixelY % 4][PixelX % 4];
		for (int32& Channel : Out)
		{
			Channel += Offset;
		}
	}
	for (int32& Channel : Out)
	{
		Channel = bColorClamp ? FMath::Clamp(Channel, 0, 255) : (Channel & 0xff);
	}
	const uint32 OutA = uint32(A) | (Context.bFba ? 0x80u : 0u);

	// FBMSK over the pre-conversion bits, plus what the tests hold back (manual 3.9.5).
	const uint32 Value = uint32(Out[0]) | (uint32(Out[1]) << 8) | (uint32(Out[2]) << 16) | (OutA << 24);
	const uint32 Mask = Frame.FBMSK | (bWriteRGB ? 0u : 0x00ffffffu) | (bWriteA ? 0u : 0xff000000u);
	if (Mask != 0xffffffffu)
	{
		uint32 Written = 0;
		if (IsColor16(Frame.PSM))
		{
			const uint32 Value16 = (uint32(Out[0]) >> 3) | ((uint32(Out[1]) >> 3) << 5) |
				((uint32(Out[2]) >> 3) << 10) | ((OutA >> 7) << 15);
			const uint32 Mask16Bits = Mask16(Mask);
			Written = (Destination & Mask16Bits) | (Value16 & ~Mask16Bits & 0xffffu);
		}
		else if (Frame.PSM == EGSPixelFormat::PSMCT24)
		{
			Written = (Destination & (Mask | 0xff000000u)) | (Value & ~Mask & 0x00ffffffu);
		}
		else
		{
			Written = (Destination & Mask) | (Value & ~Mask);
		}
		Memory.WritePixel(FrameBase, FrameWidth, Frame.PSM, PixelX, PixelY, Written);
	}
	if (bWriteZ && !ZBuf.bMask)
	{
		const uint32 Stored = Memory.ReadPixel(ZBase, FrameWidth, ZBuf.PSM, PixelX, PixelY);
		Memory.WritePixel(ZBase, FrameWidth, ZBuf.PSM, PixelX, PixelY, (Stored & ~ZMax) | Z);
	}
}

TArray<FColor> FGSReferenceRasterizer::ReadFrame(const FGSFrame& Frame, uint32 Width, uint32 Height) const
{
	TArray<FColor> Pixels;
	Pixels.Reserve(int32(Width * Height));
	const uint32 Base = uint32(Frame.FBP) * 2048;
	const uint32 BufferWidth = uint32(Frame.FBW) * 64;
	for (uint32 Y = 0; Y < Height; ++Y)
	{
		for (uint32 X = 0; X < Width; ++X)
		{
			const uint32 Raw = Memory.ReadPixel(Base, BufferWidth, Frame.PSM, X, Y);
			if (IsColor16(Frame.PSM))
			{
				Pixels.Add(FColor(uint8((Raw & 0x1f) << 3), uint8(((Raw >> 5) & 0x1f) << 3),
					uint8(((Raw >> 10) & 0x1f) << 3), ((Raw >> 15) & 1) != 0 ? 0x80 : 0));
			}
			else
			{
				const uint8 Alpha = Frame.PSM == EGSPixelFormat::PSMCT32 ? uint8(Raw >> 24) : uint8(0x80);
				Pixels.Add(FColor(uint8(Raw), uint8(Raw >> 8), uint8(Raw >> 16), Alpha));
			}
		}
	}
	return Pixels;
}

uint32 FGSReferenceRasterizer::ReadZ(const FGSZBuf& ZBuf, uint32 WidthPixels, uint32 X, uint32 Y) const
{
	return Memory.ReadPixel(uint32(ZBuf.ZBP) * 2048, WidthPixels, ZBuf.PSM, X, Y) & MaxZ(ZBuf.PSM);
}
