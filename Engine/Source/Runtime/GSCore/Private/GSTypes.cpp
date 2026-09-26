#include "GSTypes.h"

namespace
{

	/** Value's low Width bits at bit Lo. */
	constexpr uint64 Put(uint64 Value, uint32 Lo, uint32 Width)
	{
		return (Value & ((uint64(1) << Width) - 1)) << Lo;
	}

	/** The Width bits at bit Lo. */
	constexpr uint64 Get(uint64 Value, uint32 Lo, uint32 Width)
	{
		return (Value >> Lo) & ((uint64(1) << Width) - 1);
	}

	/** The Width-bit two's complement field at bit Lo, sign extended. */
	int32 GetSigned(uint64 Value, uint32 Lo, uint32 Width)
	{
		const int32 Raw = int32(Get(Value, Lo, Width));
		return (Raw & (1 << (Width - 1))) != 0 ? Raw - (1 << Width) : Raw;
	}

	uint32 FloatBits(float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	float BitsFloat(uint64 Bits)
	{
		const uint32 Low = uint32(Bits);
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &Low, sizeof(Value));
		return Value;
	}

} // namespace

uint16 GSToFixed4(float Value, uint32 FieldBits)
{
	const int32 Max = (1 << FieldBits) - 1;
	return uint16(FMath::Clamp(FMath::RoundToInt(Value * 16.0f), 0, Max));
}

uint64 FGSPrim::Encode() const
{
	return Put(uint64(Type), 0, 3) | Put(bGouraud, 3, 1) | Put(bTextured, 4, 1) | Put(bFog, 5, 1) |
		Put(bAlphaBlend, 6, 1) | Put(bAntialias, 7, 1) | Put(bUseUV, 8, 1) | Put(Context, 9, 1) |
		Put(bFixFragment, 10, 1);
}

FGSPrim FGSPrim::Decode(uint64 Value)
{
	FGSPrim Prim;
	Prim.Type = EGSPrimitive(Get(Value, 0, 3));
	Prim.bGouraud = Get(Value, 3, 1) != 0;
	Prim.bTextured = Get(Value, 4, 1) != 0;
	Prim.bFog = Get(Value, 5, 1) != 0;
	Prim.bAlphaBlend = Get(Value, 6, 1) != 0;
	Prim.bAntialias = Get(Value, 7, 1) != 0;
	Prim.bUseUV = Get(Value, 8, 1) != 0;
	Prim.Context = uint8(Get(Value, 9, 1));
	Prim.bFixFragment = Get(Value, 10, 1) != 0;
	return Prim;
}

uint64 FGSRGBAQ::Encode() const
{
	return Put(R, 0, 8) | Put(G, 8, 8) | Put(B, 16, 8) | Put(A, 24, 8) | Put(FloatBits(Q), 32, 32);
}

FGSRGBAQ FGSRGBAQ::Decode(uint64 Value)
{
	FGSRGBAQ Color;
	Color.R = uint8(Get(Value, 0, 8));
	Color.G = uint8(Get(Value, 8, 8));
	Color.B = uint8(Get(Value, 16, 8));
	Color.A = uint8(Get(Value, 24, 8));
	Color.Q = BitsFloat(Get(Value, 32, 32));
	return Color;
}

uint64 FGSST::Encode() const
{
	return Put(FloatBits(S), 0, 32) | Put(FloatBits(T), 32, 32);
}

FGSST FGSST::Decode(uint64 Value)
{
	FGSST ST;
	ST.S = BitsFloat(Get(Value, 0, 32));
	ST.T = BitsFloat(Get(Value, 32, 32));
	return ST;
}

uint64 FGSUV::Encode() const
{
	return Put(U, 0, 14) | Put(V, 16, 14);
}

FGSUV FGSUV::Decode(uint64 Value)
{
	FGSUV UV;
	UV.U = uint16(Get(Value, 0, 14));
	UV.V = uint16(Get(Value, 16, 14));
	return UV;
}

uint64 FGSXYZ::Encode() const
{
	return Put(X, 0, 16) | Put(Y, 16, 16) | Put(Z, 32, 32);
}

FGSXYZ FGSXYZ::Decode(uint64 Value)
{
	FGSXYZ XYZ;
	XYZ.X = uint16(Get(Value, 0, 16));
	XYZ.Y = uint16(Get(Value, 16, 16));
	XYZ.Z = uint32(Get(Value, 32, 32));
	return XYZ;
}

uint64 FGSXYZF::Encode() const
{
	return Put(X, 0, 16) | Put(Y, 16, 16) | Put(Z, 32, 24) | Put(F, 56, 8);
}

FGSXYZF FGSXYZF::Decode(uint64 Value)
{
	FGSXYZF XYZF;
	XYZF.X = uint16(Get(Value, 0, 16));
	XYZF.Y = uint16(Get(Value, 16, 16));
	XYZF.Z = uint32(Get(Value, 32, 24));
	XYZF.F = uint8(Get(Value, 56, 8));
	return XYZF;
}

uint64 FGSTex0::Encode() const
{
	return Put(TBP0, 0, 14) | Put(TBW, 14, 6) | Put(uint64(PSM), 20, 6) | Put(TW, 26, 4) | Put(TH, 30, 4) |
		Put(bRGBA, 34, 1) | Put(uint64(TFX), 35, 2) | Put(CBP, 37, 14) | Put(uint64(CPSM), 51, 4) | Put(bCSM2, 55, 1) |
		Put(CSA, 56, 5) | Put(CLD, 61, 3);
}

FGSTex0 FGSTex0::Decode(uint64 Value)
{
	FGSTex0 Tex0;
	Tex0.TBP0 = uint16(Get(Value, 0, 14));
	Tex0.TBW = uint8(Get(Value, 14, 6));
	Tex0.PSM = EGSPixelFormat(Get(Value, 20, 6));
	Tex0.TW = uint8(Get(Value, 26, 4));
	Tex0.TH = uint8(Get(Value, 30, 4));
	Tex0.bRGBA = Get(Value, 34, 1) != 0;
	Tex0.TFX = EGSTextureFunction(Get(Value, 35, 2));
	Tex0.CBP = uint16(Get(Value, 37, 14));
	Tex0.CPSM = EGSPixelFormat(Get(Value, 51, 4));
	Tex0.bCSM2 = Get(Value, 55, 1) != 0;
	Tex0.CSA = uint8(Get(Value, 56, 5));
	Tex0.CLD = uint8(Get(Value, 61, 3));
	return Tex0;
}

uint64 FGSTex1::Encode() const
{
	return Put(bFixedLOD, 0, 1) | Put(MXL, 2, 3) | Put(uint64(MMAG), 5, 1) | Put(uint64(MMIN), 6, 3) |
		Put(bAutoMipBase, 9, 1) | Put(L, 19, 2) | Put(uint64(uint16(K)), 32, 12);
}

FGSTex1 FGSTex1::Decode(uint64 Value)
{
	FGSTex1 Tex1;
	Tex1.bFixedLOD = Get(Value, 0, 1) != 0;
	Tex1.MXL = uint8(Get(Value, 2, 3));
	Tex1.MMAG = EGSFilter(Get(Value, 5, 1));
	Tex1.MMIN = EGSFilter(Get(Value, 6, 3));
	Tex1.bAutoMipBase = Get(Value, 9, 1) != 0;
	Tex1.L = uint8(Get(Value, 19, 2));
	Tex1.K = int16(GetSigned(Value, 32, 12));
	return Tex1;
}

uint64 FGSClamp::Encode() const
{
	return Put(uint64(WMS), 0, 2) | Put(uint64(WMT), 2, 2) | Put(MINU, 4, 10) | Put(MAXU, 14, 10) | Put(MINV, 24, 10) |
		Put(MAXV, 34, 10);
}

FGSClamp FGSClamp::Decode(uint64 Value)
{
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode(Get(Value, 0, 2));
	Clamp.WMT = EGSWrapMode(Get(Value, 2, 2));
	Clamp.MINU = uint16(Get(Value, 4, 10));
	Clamp.MAXU = uint16(Get(Value, 14, 10));
	Clamp.MINV = uint16(Get(Value, 24, 10));
	Clamp.MAXV = uint16(Get(Value, 34, 10));
	return Clamp;
}

uint64 FGSFog::Encode() const
{
	return Put(F, 56, 8);
}

FGSFog FGSFog::Decode(uint64 Value)
{
	FGSFog Fog;
	Fog.F = uint8(Get(Value, 56, 8));
	return Fog;
}

uint64 FGSFogCol::Encode() const
{
	return Put(R, 0, 8) | Put(G, 8, 8) | Put(B, 16, 8);
}

FGSFogCol FGSFogCol::Decode(uint64 Value)
{
	FGSFogCol FogCol;
	FogCol.R = uint8(Get(Value, 0, 8));
	FogCol.G = uint8(Get(Value, 8, 8));
	FogCol.B = uint8(Get(Value, 16, 8));
	return FogCol;
}

FGSAlpha FGSAlpha::Translucent()
{
	return FGSAlpha();
}

FGSAlpha FGSAlpha::Additive()
{
	FGSAlpha Alpha;
	Alpha.B = EGSBlendColor::Zero;
	return Alpha;
}

uint64 FGSAlpha::Encode() const
{
	return Put(uint64(A), 0, 2) | Put(uint64(B), 2, 2) | Put(uint64(C), 4, 2) | Put(uint64(D), 6, 2) | Put(FIX, 32, 8);
}

FGSAlpha FGSAlpha::Decode(uint64 Value)
{
	FGSAlpha Alpha;
	Alpha.A = EGSBlendColor(Get(Value, 0, 2));
	Alpha.B = EGSBlendColor(Get(Value, 2, 2));
	Alpha.C = EGSBlendAlpha(Get(Value, 4, 2));
	Alpha.D = EGSBlendColor(Get(Value, 6, 2));
	Alpha.FIX = uint8(Get(Value, 32, 8));
	return Alpha;
}

uint64 FGSTest::Encode() const
{
	return Put(bAlphaTest, 0, 1) | Put(uint64(ATST), 1, 3) | Put(AREF, 4, 8) | Put(uint64(AFAIL), 12, 2) |
		Put(bDestinationAlphaTest, 14, 1) | Put(bDestinationAlphaOne, 15, 1) | Put(bDepthTest, 16, 1) |
		Put(uint64(ZTST), 17, 2);
}

FGSTest FGSTest::Decode(uint64 Value)
{
	FGSTest Test;
	Test.bAlphaTest = Get(Value, 0, 1) != 0;
	Test.ATST = EGSAlphaTest(Get(Value, 1, 3));
	Test.AREF = uint8(Get(Value, 4, 8));
	Test.AFAIL = EGSAlphaFail(Get(Value, 12, 2));
	Test.bDestinationAlphaTest = Get(Value, 14, 1) != 0;
	Test.bDestinationAlphaOne = Get(Value, 15, 1) != 0;
	Test.bDepthTest = Get(Value, 16, 1) != 0;
	Test.ZTST = EGSDepthTest(Get(Value, 17, 2));
	return Test;
}

uint64 FGSZBuf::Encode() const
{
	return Put(ZBP, 0, 9) | Put(uint64(PSM), 24, 4) | Put(bMask, 32, 1);
}

FGSZBuf FGSZBuf::Decode(uint64 Value)
{
	FGSZBuf ZBuf;
	ZBuf.ZBP = uint16(Get(Value, 0, 9));
	// The field holds the Z format's low 4 bits (the manual's PSMZ codes are 0x30 plus them).
	ZBuf.PSM = EGSPixelFormat(0x30 | Get(Value, 24, 4));
	ZBuf.bMask = Get(Value, 32, 1) != 0;
	return ZBuf;
}

uint64 FGSFrame::Encode() const
{
	return Put(FBP, 0, 9) | Put(FBW, 16, 6) | Put(uint64(PSM), 24, 6) | Put(FBMSK, 32, 32);
}

FGSFrame FGSFrame::Decode(uint64 Value)
{
	FGSFrame Frame;
	Frame.FBP = uint16(Get(Value, 0, 9));
	Frame.FBW = uint8(Get(Value, 16, 6));
	Frame.PSM = EGSPixelFormat(Get(Value, 24, 6));
	Frame.FBMSK = uint32(Get(Value, 32, 32));
	return Frame;
}

uint64 FGSScissor::Encode() const
{
	return Put(SCAX0, 0, 11) | Put(SCAX1, 16, 11) | Put(SCAY0, 32, 11) | Put(SCAY1, 48, 11);
}

FGSScissor FGSScissor::Decode(uint64 Value)
{
	FGSScissor Scissor;
	Scissor.SCAX0 = uint16(Get(Value, 0, 11));
	Scissor.SCAX1 = uint16(Get(Value, 16, 11));
	Scissor.SCAY0 = uint16(Get(Value, 32, 11));
	Scissor.SCAY1 = uint16(Get(Value, 48, 11));
	return Scissor;
}

uint64 FGSXYOffset::Encode() const
{
	return Put(OFX, 0, 16) | Put(OFY, 32, 16);
}

FGSXYOffset FGSXYOffset::Decode(uint64 Value)
{
	FGSXYOffset Offset;
	Offset.OFX = uint16(Get(Value, 0, 16));
	Offset.OFY = uint16(Get(Value, 32, 16));
	return Offset;
}

FGSDimx FGSDimx::Default()
{
	FGSDimx Dimx;
	const int8 Example[4][4] = {{-4, 2, -3, 3}, {0, -2, 1, -1}, {-3, 3, -4, 2}, {1, -1, 0, -2}};
	FMemory::Memcpy(Dimx.M, Example, sizeof(Example));
	return Dimx;
}

uint64 FGSDimx::Encode() const
{
	uint64 Value = 0;
	for (uint32 Row = 0; Row < 4; ++Row)
	{
		for (uint32 Column = 0; Column < 4; ++Column)
		{
			// DMrc at bits 16r + 4c (3-bit two's complement).
			Value |= Put(uint64(uint8(M[Row][Column])), (Row * 16) + (Column * 4), 3);
		}
	}
	return Value;
}

FGSDimx FGSDimx::Decode(uint64 Value)
{
	FGSDimx Dimx;
	for (uint32 Row = 0; Row < 4; ++Row)
	{
		for (uint32 Column = 0; Column < 4; ++Column)
		{
			Dimx.M[Row][Column] = int8(GetSigned(Value, (Row * 16) + (Column * 4), 3));
		}
	}
	return Dimx;
}

uint64 FGSTexA::Encode() const
{
	return Put(TA0, 0, 8) | Put(bAlphaExpandBlack, 15, 1) | Put(TA1, 32, 8);
}

FGSTexA FGSTexA::Decode(uint64 Value)
{
	FGSTexA TexA;
	TexA.TA0 = uint8(Get(Value, 0, 8));
	TexA.bAlphaExpandBlack = Get(Value, 15, 1) != 0;
	TexA.TA1 = uint8(Get(Value, 32, 8));
	return TexA;
}

uint64 FGSTexClut::Encode() const
{
	return Put(CBW, 0, 6) | Put(COU, 6, 6) | Put(COV, 12, 10);
}

FGSTexClut FGSTexClut::Decode(uint64 Value)
{
	FGSTexClut TexClut;
	TexClut.CBW = uint8(Get(Value, 0, 6));
	TexClut.COU = uint8(Get(Value, 6, 6));
	TexClut.COV = uint16(Get(Value, 12, 10));
	return TexClut;
}

uint64 FGSMipTbp::Encode() const
{
	uint64 Value = 0;
	for (uint32 Level = 0; Level < 3; ++Level)
	{
		// TBPn at 20n, TBWn at 20n + 14.
		Value |= Put(TBP[Level], Level * 20, 14) | Put(TBW[Level], (Level * 20) + 14, 6);
	}
	return Value;
}

FGSMipTbp FGSMipTbp::Decode(uint64 Value)
{
	FGSMipTbp MipTbp;
	for (uint32 Level = 0; Level < 3; ++Level)
	{
		MipTbp.TBP[Level] = uint16(Get(Value, Level * 20, 14));
		MipTbp.TBW[Level] = uint8(Get(Value, (Level * 20) + 14, 6));
	}
	return MipTbp;
}

uint64 FGSBitBltBuf::Encode() const
{
	return Put(SBP, 0, 14) | Put(SBW, 16, 6) | Put(uint64(SPSM), 24, 6) | Put(DBP, 32, 14) | Put(DBW, 48, 6) |
		Put(uint64(DPSM), 56, 6);
}

FGSBitBltBuf FGSBitBltBuf::Decode(uint64 Value)
{
	FGSBitBltBuf Buf;
	Buf.SBP = uint16(Get(Value, 0, 14));
	Buf.SBW = uint8(Get(Value, 16, 6));
	Buf.SPSM = EGSPixelFormat(Get(Value, 24, 6));
	Buf.DBP = uint16(Get(Value, 32, 14));
	Buf.DBW = uint8(Get(Value, 48, 6));
	Buf.DPSM = EGSPixelFormat(Get(Value, 56, 6));
	return Buf;
}

uint64 FGSTrxPos::Encode() const
{
	return Put(SSAX, 0, 11) | Put(SSAY, 16, 11) | Put(DSAX, 32, 11) | Put(DSAY, 48, 11) | Put(DIR, 59, 2);
}

FGSTrxPos FGSTrxPos::Decode(uint64 Value)
{
	FGSTrxPos Pos;
	Pos.SSAX = uint16(Get(Value, 0, 11));
	Pos.SSAY = uint16(Get(Value, 16, 11));
	Pos.DSAX = uint16(Get(Value, 32, 11));
	Pos.DSAY = uint16(Get(Value, 48, 11));
	Pos.DIR = uint8(Get(Value, 59, 2));
	return Pos;
}

uint64 FGSTrxReg::Encode() const
{
	return Put(RRW, 0, 12) | Put(RRH, 32, 12);
}

FGSTrxReg FGSTrxReg::Decode(uint64 Value)
{
	FGSTrxReg Reg;
	Reg.RRW = uint16(Get(Value, 0, 12));
	Reg.RRH = uint16(Get(Value, 32, 12));
	return Reg;
}

uint32 GSBitsPerPixel(EGSPixelFormat Format)
{
	switch (Format)
	{
		case EGSPixelFormat::PSMT4:
		case EGSPixelFormat::PSMT4HL:
		case EGSPixelFormat::PSMT4HH:
			return 4;
		case EGSPixelFormat::PSMT8:
		case EGSPixelFormat::PSMT8H:
			return 8;
		case EGSPixelFormat::PSMCT16:
		case EGSPixelFormat::PSMCT16S:
		case EGSPixelFormat::PSMZ16:
		case EGSPixelFormat::PSMZ16S:
			return 16;
		case EGSPixelFormat::PSMCT24:
		case EGSPixelFormat::PSMZ24:
			return 24;
		case EGSPixelFormat::PSMCT32:
		case EGSPixelFormat::PSMZ32:
			return 32;
	}
	return 32;
}
