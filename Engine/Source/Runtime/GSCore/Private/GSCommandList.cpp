#include "GSCommandList.h"

void FGSCommandList::Write(EGSRegister Register, uint64 Value)
{
	Writes.Add({Register, Value});
}

EGSRegister FGSCommandList::ContextRegister(EGSRegister Base, uint8 Context)
{
	check(Context <= 1);
	return EGSRegister(uint8(Base) + Context);
}

void FGSCommandList::SetPrim(const FGSPrim& Prim)
{
	check(IsSupported(Prim));
	Write(EGSRegister::PRIM, Prim.Encode());
}

void FGSCommandList::SetRGBAQ(const FGSRGBAQ& Color)
{
	Write(EGSRegister::RGBAQ, Color.Encode());
}

void FGSCommandList::SetST(const FGSST& ST)
{
	Write(EGSRegister::ST, ST.Encode());
}

void FGSCommandList::SetUV(const FGSUV& UV)
{
	Write(EGSRegister::UV, UV.Encode());
}

void FGSCommandList::SetFog(const FGSFog& Fog)
{
	Write(EGSRegister::FOG, Fog.Encode());
}

void FGSCommandList::AddVertex(const FGSXYZ& Vertex)
{
	Write(EGSRegister::XYZ2, Vertex.Encode());
}

void FGSCommandList::AddVertex(const FGSXYZF& Vertex)
{
	Write(EGSRegister::XYZF2, Vertex.Encode());
}

void FGSCommandList::AddVertexNoKick(const FGSXYZ& Vertex)
{
	Write(EGSRegister::XYZ3, Vertex.Encode());
}

void FGSCommandList::SetTex0(uint8 Context, const FGSTex0& Tex0)
{
	check(IsSupported(Tex0));
	Write(ContextRegister(EGSRegister::TEX0_1, Context), Tex0.Encode());
}

void FGSCommandList::SetTex1(uint8 Context, const FGSTex1& Tex1)
{
	check(IsSupported(Tex1));
	Write(ContextRegister(EGSRegister::TEX1_1, Context), Tex1.Encode());
}

void FGSCommandList::SetClamp(uint8 Context, const FGSClamp& Clamp)
{
	Write(ContextRegister(EGSRegister::CLAMP_1, Context), Clamp.Encode());
}

void FGSCommandList::SetMipTbp1(uint8 Context, const FGSMipTbp& MipTbp)
{
	Write(ContextRegister(EGSRegister::MIPTBP1_1, Context), MipTbp.Encode());
}

void FGSCommandList::SetMipTbp2(uint8 Context, const FGSMipTbp& MipTbp)
{
	Write(ContextRegister(EGSRegister::MIPTBP2_1, Context), MipTbp.Encode());
}

void FGSCommandList::SetXYOffset(uint8 Context, const FGSXYOffset& Offset)
{
	Write(ContextRegister(EGSRegister::XYOFFSET_1, Context), Offset.Encode());
}

void FGSCommandList::SetScissor(uint8 Context, const FGSScissor& Scissor)
{
	Write(ContextRegister(EGSRegister::SCISSOR_1, Context), Scissor.Encode());
}

void FGSCommandList::SetAlpha(uint8 Context, const FGSAlpha& Alpha)
{
	check(IsSupported(Alpha));
	Write(ContextRegister(EGSRegister::ALPHA_1, Context), Alpha.Encode());
}

void FGSCommandList::SetTest(uint8 Context, const FGSTest& Test)
{
	check(IsSupported(Test));
	Write(ContextRegister(EGSRegister::TEST_1, Context), Test.Encode());
}

void FGSCommandList::SetFba(uint8 Context, bool bForceAlphaMSB)
{
	Write(ContextRegister(EGSRegister::FBA_1, Context), bForceAlphaMSB ? 1 : 0);
}

void FGSCommandList::SetFrame(uint8 Context, const FGSFrame& Frame)
{
	check(IsSupported(Frame));
	Write(ContextRegister(EGSRegister::FRAME_1, Context), Frame.Encode());
}

void FGSCommandList::SetZBuf(uint8 Context, const FGSZBuf& ZBuf)
{
	check(IsSupported(ZBuf));
	Write(ContextRegister(EGSRegister::ZBUF_1, Context), ZBuf.Encode());
}

void FGSCommandList::SetPrimModeFromPrim()
{
	// PRMODECONT.AC = 1: PRIM's attribute fields are used.
	Write(EGSRegister::PRMODECONT, 1);
}

void FGSCommandList::SetFogCol(const FGSFogCol& FogCol)
{
	Write(EGSRegister::FOGCOL, FogCol.Encode());
}

void FGSCommandList::SetDimx(const FGSDimx& Dimx)
{
	Write(EGSRegister::DIMX, Dimx.Encode());
}

void FGSCommandList::SetDither(bool bDither)
{
	Write(EGSRegister::DTHE, bDither ? 1 : 0);
}

void FGSCommandList::SetColorClamp(bool bClamp)
{
	Write(EGSRegister::COLCLAMP, bClamp ? 1 : 0);
}

void FGSCommandList::SetPixelAlphaBlend(bool bPerPixel)
{
	Write(EGSRegister::PABE, bPerPixel ? 1 : 0);
}

void FGSCommandList::SetTexA(const FGSTexA& TexA)
{
	Write(EGSRegister::TEXA, TexA.Encode());
}

void FGSCommandList::SetTexClut(const FGSTexClut& TexClut)
{
	Write(EGSRegister::TEXCLUT, TexClut.Encode());
}

void FGSCommandList::TexFlush()
{
	Write(EGSRegister::TEXFLUSH, 0);
}

void FGSCommandList::UploadImage(
	const FGSBitBltBuf& Destination, uint16 X, uint16 Y, uint16 Width, uint16 Height, TArrayView<const uint8> Pixels)
{
	const uint64 Bytes = (uint64(Width) * Height * GSBitsPerPixel(Destination.DPSM)) / 8;
	check(Width > 0 && Height > 0 && uint64(Pixels.Num()) == Bytes && Bytes % 16 == 0);
	check(IsSupportedUpload(Destination.DPSM, X, Width));
	Write(EGSRegister::BITBLTBUF, Destination.Encode());
	FGSTrxPos Position;
	Position.DSAX = X;
	Position.DSAY = Y;
	Write(EGSRegister::TRXPOS, Position.Encode());
	FGSTrxReg Region;
	Region.RRW = Width;
	Region.RRH = Height;
	Write(EGSRegister::TRXREG, Region.Encode());
	Write(EGSRegister::TRXDIR, uint64(EGSTransferDirection::HostToLocal));
	Write(EGSRegister::HWREG, uint64(ImageData.Num()));
	ImageData.Emplace(Pixels.GetData(), Pixels.Num());
}

void FGSCommandList::Reset()
{
	Writes.Reset();
	ImageData.Reset();
}

bool FGSCommandList::IsSupported(const FGSPrim& Prim)
{
	return !Prim.bAntialias && uint8(Prim.Type) <= uint8(EGSPrimitive::Sprite) && Prim.Context <= 1;
}

bool FGSCommandList::IsSupported(const FGSAlpha& Alpha)
{
	const bool bAlphaInput = Alpha.C == EGSBlendAlpha::Source || Alpha.C == EGSBlendAlpha::Fixed;
	if (Alpha.A != EGSBlendColor::Source || !bAlphaInput)
	{
		return false;
	}
	if (Alpha.B == EGSBlendColor::Destination)
	{
		// A lerp toward the destination.
		return Alpha.D == EGSBlendColor::Destination;
	}
	// Added to the destination, or alone.
	return Alpha.B == EGSBlendColor::Zero && (Alpha.D == EGSBlendColor::Destination || Alpha.D == EGSBlendColor::Zero);
}

bool FGSCommandList::IsSupported(const FGSTex0& Tex0)
{
	if (Tex0.TW > 10 || Tex0.TH > 10 || Tex0.CLD > 1)
	{
		return false;
	}
	switch (Tex0.PSM)
	{
		case EGSPixelFormat::PSMCT32:
		case EGSPixelFormat::PSMCT24:
		case EGSPixelFormat::PSMCT16:
			return true;
		case EGSPixelFormat::PSMT8:
		case EGSPixelFormat::PSMT4:
			return !Tex0.bCSM2 && (Tex0.CPSM == EGSPixelFormat::PSMCT32 || Tex0.CPSM == EGSPixelFormat::PSMCT16);
		default:
			return false;
	}
}

bool FGSCommandList::IsSupported(const FGSTex1& Tex1)
{
	return !Tex1.bAutoMipBase;
}

bool FGSCommandList::IsSupportedUpload(EGSPixelFormat Format, uint16 X, uint16 Width)
{
	switch (GSBitsPerPixel(Format))
	{
		case 32:
			return Width % 2 == 0;
		case 24:
			return Width % 8 == 0;
		case 16:
			return Width % 4 == 0;
		case 8:
			return Width % 8 == 0 && X % 2 == 0;
		case 4:
			return Width % 8 == 0 && X % 4 == 0;
		default:
			return false;
	}
}

bool FGSCommandList::IsSupported(const FGSTest& Test)
{
	return Test.bDepthTest;
}

bool FGSCommandList::IsSupported(const FGSFrame& Frame)
{
	switch (Frame.PSM)
	{
		case EGSPixelFormat::PSMCT32:
		case EGSPixelFormat::PSMCT24:
		case EGSPixelFormat::PSMCT16:
		case EGSPixelFormat::PSMCT16S:
			return true;
		default:
			return false;
	}
}

bool FGSCommandList::IsSupported(const FGSZBuf& ZBuf)
{
	switch (ZBuf.PSM)
	{
		case EGSPixelFormat::PSMZ32:
		case EGSPixelFormat::PSMZ24:
		case EGSPixelFormat::PSMZ16:
		case EGSPixelFormat::PSMZ16S:
			return true;
		default:
			return false;
	}
}
