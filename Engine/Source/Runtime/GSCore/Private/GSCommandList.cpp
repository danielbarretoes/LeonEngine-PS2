#include "GSCommandList.h"

#include "GSPrimitiveEmitter.h"
#include "HAL/LowLevelMemTracker.h"

void FGSCommandList::Write(EGSRegister Register, uint64 Value)
{
	LLM_SCOPE(ELLMTag::RenderLists);
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

void FGSCommandList::AddVertexNoKick(const FGSXYZF& Vertex)
{
	Write(EGSRegister::XYZF3, Vertex.Encode());
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
	check(IsSupported(Clamp));
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
	WriteUpload(Destination, X, Y, Width, Height, Pixels);
	AddImage(Pixels, true);
}

void FGSCommandList::UploadImageInPlace(
	const FGSBitBltBuf& Destination, uint16 X, uint16 Y, uint16 Width, uint16 Height, TArrayView<const uint8> Pixels)
{
	check((UPTRINT(Pixels.GetData()) & 15) == 0);
	WriteUpload(Destination, X, Y, Width, Height, Pixels);
	AddImage(Pixels, false);
}

void FGSCommandList::WriteUpload(
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
	Write(EGSRegister::HWREG, uint64(Images.Num()));
}

void FGSCommandList::AddImage(TArrayView<const uint8> Pixels, bool bCopy)
{
	LLM_SCOPE(ELLMTag::RenderLists);
	FImage& Image = Images.AddDefaulted_GetRef();
	Image.NumBytes = Pixels.Num();
	if (bCopy)
	{
		Image.CopyIndex = Copies.Num();
		TArray<uint8>& Copy = Copies.Emplace_GetRef(Pixels.GetData(), Pixels.Num());
		Image.Data = Copy.GetData();
	}
	else
	{
		Image.Data = Pixels.GetData();
	}
}

int32 FGSCommandList::AddVertexDraw(const FGSVertexDraw& Draw)
{
	LLM_SCOPE(ELLMTag::RenderLists);
	return VertexDraws.Add(Draw);
}

void FGSCommandList::DrawVertexBatch(const FGSVertexBatch& Batch)
{
	check(VertexDraws.IsValidIndex(Batch.Draw));
	check(Batch.NumVertices >= 3 && Batch.NumVertices <= FGSVertexBatch::MaxVertices);
	check(Batch.Positions != nullptr && Batch.Normals != nullptr && Batch.Colors != nullptr &&
		Batch.TexCoords != nullptr);
	check(Batch.IsSkinned() == VertexDraws[Batch.Draw].bSkinned);
	check(!Batch.IsSkinned() ||
		(Batch.NumVertices <= FGSVertexBatch::MaxSkinnedVertices && Batch.Palette != nullptr && Batch.NumBones > 0 &&
			Batch.NumBones <= FGSVertexBatch::MaxBones));
	EGSVertexProgram Program = EGSVertexProgram::StaticUnlit;
	check(VertexDraws[Batch.Draw].GetProgram(Program));
	(void)Program;
	LLM_SCOPE(ELLMTag::RenderLists);
	Write(EGSRegister::VertexBatch, uint64(VertexBatches.Add(Batch)));
}

FGSSkinMatrix* FGSCommandList::AllocateSkinPalette(uint32 NumBones)
{
	check(NumBones > 0 && NumBones <= FGSVertexBatch::MaxBones);
	LLM_SCOPE(ELLMTag::RenderLists);
	const uint32 NumBytes = NumBones * uint32(sizeof(FGSSkinMatrix));
	if (NumPaletteBlocks == 0 || PaletteBlockUsed + NumBytes > PaletteBlockBytes)
	{
		if (NumPaletteBlocks == PaletteBlocks.Num())
		{
			// A quadword more, to align its start.
			PaletteBlocks.AddDefaulted_GetRef().SetNumUninitialized(int32(PaletteBlockBytes + 15), false);
		}
		++NumPaletteBlocks;
		PaletteBlockUsed = 0;
	}
	uint8* Block = PaletteBlocks[NumPaletteBlocks - 1].GetData();
	uint8* Palette = reinterpret_cast<uint8*>((UPTRINT(Block) + 15) & ~UPTRINT(15)) + PaletteBlockUsed;
	PaletteBlockUsed += NumBytes;
	return reinterpret_cast<FGSSkinMatrix*>(Palette);
}

void FGSCommandList::Append(const FGSCommandList& Other)
{
	LLM_SCOPE(ELLMTag::RenderLists);
	const uint64 FirstImage = uint64(Images.Num());
	const uint64 FirstBatch = uint64(VertexBatches.Num());
	Writes.Reserve(Writes.Num() + Other.Writes.Num());
	for (const FGSRegisterWrite& OtherWrite : Other.Writes)
	{
		uint64 Value = OtherWrite.Value;
		if (OtherWrite.Register == EGSRegister::HWREG)
		{
			Value += FirstImage;
		}
		else if (OtherWrite.Register == EGSRegister::VertexBatch)
		{
			Value += FirstBatch;
		}
		Writes.Add({OtherWrite.Register, Value});
	}
	Images.Reserve(Images.Num() + Other.Images.Num());
	for (int32 Index = 0; Index < Other.Images.Num(); ++Index)
	{
		AddImage(Other.GetImage(Index), !Other.IsImageInPlace(Index));
	}
	const int32 FirstDraw = VertexDraws.Num();
	VertexDraws.Append(Other.VertexDraws);
	VertexBatches.Reserve(VertexBatches.Num() + Other.VertexBatches.Num());
	// Other's palettes are reused with its next frame; this list's copies last until its own Reset (once each: the
	// batches of a palette follow one another).
	const FGSSkinMatrix* LastSource = nullptr;
	const FGSSkinMatrix* LastCopy = nullptr;
	for (const FGSVertexBatch& OtherBatch : Other.VertexBatches)
	{
		FGSVertexBatch& Batch = VertexBatches.Add_GetRef(OtherBatch);
		Batch.Draw += FirstDraw;
		if (Batch.IsSkinned())
		{
			if (Batch.Palette != LastSource)
			{
				FGSSkinMatrix* Copy = AllocateSkinPalette(Batch.NumBones);
				FMemory::Memcpy(Copy, Batch.Palette, Batch.NumBones * sizeof(FGSSkinMatrix));
				LastSource = Batch.Palette;
				LastCopy = Copy;
			}
			Batch.Palette = LastCopy;
		}
	}
}

void FGSCommandList::AppendExpanded(const FGSCommandList& Other, const FGSDrawEnvironment& Environment)
{
	check(&Other != this);
	// The emitter's scratch is on the scratchpad (the frame's stack what does not fit).
	FMemMark Mark(FMemStack::Get());
	FScratchpadMark ScratchpadMark;
	FGSPrimitiveEmitter Emitter(Environment, *this);
	const uint64 FirstImage = uint64(Images.Num());
	for (const FGSRegisterWrite& OtherWrite : Other.Writes)
	{
		if (OtherWrite.Register == EGSRegister::VertexBatch)
		{
			const FGSVertexBatch& Batch = Other.VertexBatches[int32(OtherWrite.Value)];
			const FGSVertexDraw& Draw = Other.VertexDraws[Batch.Draw];
			Emitter.SetFog(Draw.Fog);
			Emitter.BeginStrip(Draw.bTextured, Draw.bBlend, true);
			Emitter.AddVertexBatch(Draw, Batch);
			continue;
		}
		const bool bImage = OtherWrite.Register == EGSRegister::HWREG;
		Write(OtherWrite.Register, bImage ? FirstImage + OtherWrite.Value : OtherWrite.Value);
	}
	Images.Reserve(Images.Num() + Other.Images.Num());
	for (int32 Index = 0; Index < Other.Images.Num(); ++Index)
	{
		AddImage(Other.GetImage(Index), !Other.IsImageInPlace(Index));
	}
}

void FGSCommandList::CopyInPlaceImages()
{
	LLM_SCOPE(ELLMTag::RenderLists);
	for (FImage& Image : Images)
	{
		if (Image.CopyIndex == INDEX_NONE)
		{
			Image.CopyIndex = Copies.Num();
			Image.Data = Copies.Emplace_GetRef(Image.Data, Image.NumBytes).GetData();
		}
	}
}

void FGSCommandList::Reset()
{
	Writes.Reset();
	Images.Reset();
	Copies.Reset();
	VertexDraws.Reset();
	VertexBatches.Reset();
	NumPaletteBlocks = 0;
	PaletteBlockUsed = 0;
}

bool FGSCommandList::IsSupported(const FGSPrim& Prim)
{
	return !Prim.bAntialias && !Prim.bFixFragment && uint8(Prim.Type) <= uint8(EGSPrimitive::Sprite) &&
		Prim.Context <= 1;
}

bool FGSCommandList::IsSupported(const FGSAlpha& Alpha)
{
	const auto IsColor = [](EGSBlendColor Color) { return uint8(Color) <= uint8(EGSBlendColor::Zero); };
	return IsColor(Alpha.A) && IsColor(Alpha.B) && IsColor(Alpha.D) && uint8(Alpha.C) <= uint8(EGSBlendAlpha::Fixed);
}

bool FGSCommandList::IsSupported(const FGSTex0& Tex0)
{
	if (Tex0.TW > 10 || Tex0.TH > 10 || Tex0.CLD > 5 || uint8(Tex0.TFX) > uint8(EGSTextureFunction::Highlight2))
	{
		return false;
	}
	switch (Tex0.PSM)
	{
		case EGSPixelFormat::PSMCT32:
		case EGSPixelFormat::PSMCT24:
		case EGSPixelFormat::PSMCT16:
		case EGSPixelFormat::PSMCT16S:
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
	return !Tex1.bAutoMipBase && Tex1.MXL <= 6 && uint8(Tex1.MMAG) <= uint8(EGSFilter::Linear) &&
		uint8(Tex1.MMIN) <= uint8(EGSFilter::LinearMipmapLinear) && Tex1.L <= 3 && Tex1.K >= -2048 && Tex1.K <= 2047;
}

bool FGSCommandList::IsSupported(const FGSClamp& Clamp)
{
	// REGION_CLAMP's range must not be empty (the manual gives no result for MIN above MAX).
	const bool bRangeU = Clamp.WMS != EGSWrapMode::RegionClamp || Clamp.MINU <= Clamp.MAXU;
	const bool bRangeV = Clamp.WMT != EGSWrapMode::RegionClamp || Clamp.MINV <= Clamp.MAXV;
	return Clamp.MINU < 1024 && Clamp.MAXU < 1024 && Clamp.MINV < 1024 && Clamp.MAXV < 1024 && bRangeU && bRangeV;
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
	return Test.bDepthTest && uint8(Test.ATST) <= uint8(EGSAlphaTest::NotEqual) &&
		uint8(Test.AFAIL) <= uint8(EGSAlphaFail::RGBOnly) && uint8(Test.ZTST) <= uint8(EGSDepthTest::Greater);
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
