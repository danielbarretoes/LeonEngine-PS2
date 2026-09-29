#include "GSConformanceScenes.h"

#include "GSTextureLayout.h"

namespace
{

	/**
	 * The environment every scene starts with: frame, Z buffer, no offset, the whole frame as scissor, Z ALWAYS and
	 * the common registers the scenes change back at their defaults. The frame and the Z buffer are cleared to 0 (the
	 * reference's memory starts that way, the GS's holds the previous scene).
	 */
	void SetUpScene(FGSCommandList& List, EGSPixelFormat Format = EGSPixelFormat::PSMCT32, bool bZMask = true)
	{
		List.SetPrimModeFromPrim();
		List.SetFrame(0, GSConformance::MakeFrame(Format));
		List.SetZBuf(0, GSConformance::MakeZBuf(false));
		List.SetXYOffset(0, FGSXYOffset());
		FGSScissor Scissor;
		Scissor.SCAX1 = GSConformance::FrameWidth - 1;
		Scissor.SCAY1 = GSConformance::FrameHeight - 1;
		List.SetScissor(0, Scissor);
		List.SetTest(0, FGSTest());
		List.SetAlpha(0, FGSAlpha());
		List.SetFba(0, false);
		List.SetTex1(0, FGSTex1());
		List.SetClamp(0, FGSClamp());
		List.SetColorClamp(true);
		List.SetDither(false);
		List.SetPixelAlphaBlend(false);
		List.SetTexA(FGSTexA());

		FGSPrim Clear;
		Clear.Type = EGSPrimitive::Sprite;
		List.SetPrim(Clear);
		List.SetRGBAQ(FGSRGBAQ{0, 0, 0, 0});
		List.AddVertex(FGSXYZ{0, 0, 0});
		List.AddVertex(FGSXYZ{
			GSToFixed4(float(GSConformance::FrameWidth), 16), GSToFixed4(float(GSConformance::FrameHeight), 16), 0});
		List.SetZBuf(0, GSConformance::MakeZBuf(bZMask));
	}

	FGSXYZ Vertex(float X, float Y, uint32 Z = 0)
	{
		FGSXYZ Result;
		Result.X = GSToFixed4(X, 16);
		Result.Y = GSToFixed4(Y, 16);
		Result.Z = Z;
		return Result;
	}

	FGSRGBAQ Color(uint8 R, uint8 G, uint8 B, uint8 A = 0x80)
	{
		FGSRGBAQ Result;
		Result.R = R;
		Result.G = G;
		Result.B = B;
		Result.A = A;
		return Result;
	}

	FGSPrim Primitive(EGSPrimitive Type)
	{
		FGSPrim Prim;
		Prim.Type = Type;
		return Prim;
	}

	/** An untextured sprite over the pixels [X0, X1) x [Y0, Y1). */
	void AddSprite(FGSCommandList& List, float X0, float Y0, float X1, float Y1, const FGSRGBAQ& SpriteColor,
		const FGSPrim& Prim = Primitive(EGSPrimitive::Sprite), uint32 Z = 0)
	{
		List.SetPrim(Prim);
		List.SetRGBAQ(SpriteColor);
		List.AddVertex(Vertex(X0, Y0, Z));
		List.AddVertex(Vertex(X1, Y1, Z));
	}

	/** Uploads Texels (PSMCT32, row by row) at TBP of a texture 64 pixels wide. */
	void UploadTexture32(FGSCommandList& List, uint16 TBP, uint16 Width, uint16 Height, const TArray<uint32>& Texels)
	{
		TArray<uint8> Bytes;
		for (const uint32 Texel : Texels)
		{
			for (uint32 Byte = 0; Byte < 4; ++Byte)
			{
				Bytes.Add(uint8(Texel >> (Byte * 8)));
			}
		}
		FGSBitBltBuf Destination;
		Destination.DBP = TBP;
		Destination.DBW = 1;
		Destination.DPSM = EGSPixelFormat::PSMCT32;
		List.UploadImage(Destination, 0, 0, Width, Height, Bytes);
		List.TexFlush();
	}

	/** Additive blending with the source alpha at 0x80 (1.0): each draw adds its color once. */
	void UseAdditive(FGSCommandList& List)
	{
		List.SetAlpha(0, FGSAlpha::Additive());
	}

	FGSPrim Blended(EGSPrimitive Type)
	{
		FGSPrim Prim = Primitive(Type);
		Prim.bAlphaBlend = true;
		return Prim;
	}

} // namespace

FGSFrame GSConformance::MakeFrame(EGSPixelFormat Format)
{
	FGSFrame Frame;
	Frame.FBP = 0;
	Frame.FBW = 1;
	Frame.PSM = Format;
	return Frame;
}

FGSZBuf GSConformance::MakeZBuf(bool bMask)
{
	FGSZBuf ZBuf;
	ZBuf.ZBP = 32;
	ZBuf.PSM = EGSPixelFormat::PSMZ32;
	ZBuf.bMask = bMask;
	return ZBuf;
}

void GSConformance::BuildDrawingRules(FGSCommandList& List)
{
	SetUpScene(List);
	UseAdditive(List);
	List.SetPrim(Blended(EGSPrimitive::Triangle));
	List.SetRGBAQ(Color(0x10, 0x10, 0x10));
	List.AddVertex(Vertex(0, 0));
	List.AddVertex(Vertex(8, 0));
	List.AddVertex(Vertex(8, 8));
	List.AddVertex(Vertex(0, 0));
	List.AddVertex(Vertex(8, 8));
	List.AddVertex(Vertex(0, 8));
	AddSprite(List, 20, 2, 24, 5, Color(0xff, 0, 0));
	// A one-pixel triangle: only the pixel on its top-left corner.
	List.SetPrim(Primitive(EGSPrimitive::Triangle));
	List.SetRGBAQ(Color(0, 0xff, 0));
	List.AddVertex(Vertex(30, 10));
	List.AddVertex(Vertex(31, 10));
	List.AddVertex(Vertex(30, 11));
}

void GSConformance::BuildPrimitives(FGSCommandList& List)
{
	SetUpScene(List);
	UseAdditive(List);
	List.SetRGBAQ(Color(0x10, 0, 0));
	List.SetPrim(Blended(EGSPrimitive::TriangleStrip));
	List.AddVertexNoKick(Vertex(0, 0));
	List.AddVertexNoKick(Vertex(4, 0));
	List.AddVertex(Vertex(0, 4));
	List.AddVertex(Vertex(4, 4));
	List.SetPrim(Blended(EGSPrimitive::TriangleFan));
	List.AddVertex(Vertex(10, 0));
	List.AddVertex(Vertex(14, 0));
	List.AddVertex(Vertex(14, 4));
	List.AddVertex(Vertex(10, 4));
	List.SetPrim(Primitive(EGSPrimitive::Point));
	List.SetRGBAQ(Color(0, 0xff, 0));
	List.AddVertex(Vertex(20.4f, 3.6f));
	List.SetPrim(Primitive(EGSPrimitive::Line));
	List.SetRGBAQ(Color(0, 0, 0xff));
	List.AddVertex(Vertex(30, 1));
	List.AddVertex(Vertex(34, 1));
	// Flat: the second triangle's color is its kicking vertex's.
	List.SetPrim(Primitive(EGSPrimitive::Triangle));
	List.SetRGBAQ(Color(0xff, 0, 0));
	List.AddVertex(Vertex(40, 0));
	List.AddVertex(Vertex(48, 0));
	List.SetRGBAQ(Color(0, 0, 0x40));
	List.AddVertex(Vertex(40, 8));
}

void GSConformance::BuildGouraudAndScissor(FGSCommandList& List)
{
	SetUpScene(List);
	FGSPrim Gouraud = Primitive(EGSPrimitive::Triangle);
	Gouraud.bGouraud = true;
	List.SetPrim(Gouraud);
	List.SetRGBAQ(Color(0, 0, 0));
	List.AddVertex(Vertex(0, 0));
	List.SetRGBAQ(Color(200, 0, 0));
	List.AddVertex(Vertex(20, 0));
	List.SetRGBAQ(Color(0, 0, 0));
	List.AddVertex(Vertex(0, 20));
	FGSScissor Scissor;
	Scissor.SCAX0 = 40;
	Scissor.SCAX1 = 42;
	Scissor.SCAY0 = 1;
	Scissor.SCAY1 = 2;
	List.SetScissor(0, Scissor);
	AddSprite(List, 30, 0, 60, 10, Color(0, 0xff, 0));
}

void GSConformance::BuildTextureSampling(FGSCommandList& List)
{
	SetUpScene(List);
	TArray<uint32> Texels;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (uint32 Column = 0; Column < 8; ++Column)
		{
			Texels.Add((Column * 32) | 0x80000000u);
		}
	}
	UploadTexture32(List, 64, 8, 2, Texels);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.PSM = EGSPixelFormat::PSMCT32;
	Tex0.TW = 3;
	Tex0.TH = 1;
	Tex0.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Tex0);
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;

	const auto AddTexturedRow = [&List, &Textured](float Y, float U0, float U1)
	{
		List.SetPrim(Textured);
		FGSUV UV;
		UV.U = GSToFixed4(U0, 14);
		UV.V = GSToFixed4(0.5f, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(0, Y));
		UV.U = GSToFixed4(U1, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(16, Y + 1));
	};
	// Row 0: point sampling, u = x at pixel x; row 1: u = x + 16 with REPEAT; row 2: CLAMP; row 3: bilinear.
	List.SetTex1(0, FGSTex1());
	AddTexturedRow(0, 0, 16);
	AddTexturedRow(1, 16, 32);
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	List.SetClamp(0, Clamp);
	AddTexturedRow(2, 0, 16);
	List.SetClamp(0, FGSClamp());
	FGSTex1 Bilinear;
	Bilinear.MMAG = EGSFilter::Linear;
	List.SetTex1(0, Bilinear);
	AddTexturedRow(3, 0, 16);
	FGSClamp RegionRepeat;
	RegionRepeat.WMS = EGSWrapMode::RegionRepeat;
	RegionRepeat.MINU = 1;
	RegionRepeat.MAXU = 4;
	List.SetTex1(0, FGSTex1());
	List.SetClamp(0, RegionRepeat);
	AddTexturedRow(4, 0, 16);
}

void GSConformance::BuildClutAndFormats(FGSCommandList& List)
{
	SetUpScene(List);

	// The 8-bit CLUT at CBP 128: entry 8 sits at (0, 1), entry 16 at (8, 0).
	TArray<uint32> Clut8;
	Clut8.SetNumZeroed(16 * 16);
	Clut8[(1 * 16) + 0] = 0x800000ffu;
	Clut8[(0 * 16) + 8] = 0x8000ff00u;
	UploadTexture32(List, 128, 16, 16, Clut8);
	TArray<uint8> Indices8 = {8, 16, 8, 16, 8, 16, 8, 16, 8, 16, 8, 16, 8, 16, 8, 16};
	FGSBitBltBuf Buf8;
	Buf8.DBP = 192;
	Buf8.DBW = 2;
	Buf8.DPSM = EGSPixelFormat::PSMT8;
	List.UploadImage(Buf8, 0, 0, 8, 2, Indices8);

	// The 4-bit CLUT at CBP 256 (8 x 2): entry 1 blue.
	TArray<uint32> Clut4;
	Clut4.SetNumZeroed(8 * 2);
	Clut4[1] = 0x80ff0000u;
	UploadTexture32(List, 256, 8, 2, Clut4);
	TArray<uint8> Indices4;
	for (uint32 Index = 0; Index < 16; ++Index)
	{
		Indices4.Add(0x11);
	}
	FGSBitBltBuf Buf4;
	Buf4.DBP = 320;
	Buf4.DBW = 2;
	Buf4.DPSM = EGSPixelFormat::PSMT4;
	List.UploadImage(Buf4, 0, 0, 16, 2, Indices4);

	// A 16-bit texture: white with A = 0, white with A = 1, black with A = 0 (x 2 rows).
	TArray<uint8> Texels16;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (const uint16 Texel : {uint16(0x7fff), uint16(0xffff), uint16(0x0000), uint16(0x0000)})
		{
			Texels16.Add(uint8(Texel));
			Texels16.Add(uint8(Texel >> 8));
		}
	}
	FGSBitBltBuf Buf16;
	Buf16.DBP = 384;
	Buf16.DBW = 1;
	Buf16.DPSM = EGSPixelFormat::PSMCT16;
	List.UploadImage(Buf16, 0, 0, 4, 2, Texels16);
	List.TexFlush();
	FGSTexA TexA;
	TexA.TA0 = 0x20;
	TexA.TA1 = 0x60;
	TexA.bAlphaExpandBlack = true;
	List.SetTexA(TexA);
	List.SetTex1(0, FGSTex1());

	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto AddTexturedRow = [&List, &Textured](const FGSTex0& Tex0, float Y)
	{
		List.SetTex0(0, Tex0);
		List.SetPrim(Textured);
		FGSUV UV;
		UV.V = GSToFixed4(0.5f, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(0, Y));
		UV.U = GSToFixed4(8, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(8, Y + 1));
	};
	FGSTex0 Tex8;
	Tex8.TBP0 = 192;
	Tex8.TBW = 2;
	Tex8.PSM = EGSPixelFormat::PSMT8;
	Tex8.TW = 3;
	Tex8.TH = 1;
	Tex8.TFX = EGSTextureFunction::Decal;
	Tex8.CBP = 128;
	Tex8.CLD = 1;
	AddTexturedRow(Tex8, 0);
	FGSTex0 Tex4 = Tex8;
	Tex4.TBP0 = 320;
	Tex4.PSM = EGSPixelFormat::PSMT4;
	Tex4.TW = 4;
	Tex4.CBP = 256;
	Tex4.CSA = 1;
	AddTexturedRow(Tex4, 1);
	FGSTex0 Tex16 = Tex8;
	Tex16.TBP0 = 384;
	Tex16.TBW = 1;
	Tex16.PSM = EGSPixelFormat::PSMCT16;
	Tex16.TW = 2;
	Tex16.CLD = 0;
	List.SetTex0(0, Tex16);
	List.SetPrim(Textured);
	FGSUV UV16;
	UV16.V = GSToFixed4(0.5f, 14);
	List.SetUV(UV16);
	List.AddVertex(Vertex(0, 2));
	UV16.U = GSToFixed4(4, 14);
	List.SetUV(UV16);
	List.AddVertex(Vertex(4, 3));

	// Rows 4-31 (ps2-shipping N13, what the scene renderer's texture cache does): paletted MIPMAP levels through one
	// CLUT, trilinear with the LOD from Q, and the CLUT loaded by CBP0 / CBP1 only when it changes. CLUT A (PSMT8:
	// entries 1, 2, 3 red, green, blue) at 448, CLUT B (PSMT4: entries 1, 2 yellow, magenta) at 452; texture A is
	// 32 x 32 PSMT8 with levels of index 1, 2 and 3 at 460, 464 and 465, texture B 16 x 16 PSMT4 with levels of index
	// 1 and 2 at 466 and 467 (each from any block: it fits in a page).
	const auto UploadClut = [&List](uint16 CBP, TArray<uint32> Palette)
	{
		TArray<uint8> Image;
		uint16 Width = 0;
		uint16 Height = 0;
		FGSTextureLayout::MakeClutImage(Palette, Image, Width, Height);
		FGSBitBltBuf Destination;
		Destination.DBP = CBP;
		Destination.DBW = 1;
		Destination.DPSM = EGSPixelFormat::PSMCT32;
		List.UploadImage(Destination, 0, 0, Width, Height, Image);
		List.TexFlush();
	};
	TArray<uint32> PaletteA;
	PaletteA.SetNumZeroed(256);
	PaletteA[1] = 0xff0000ffu;
	PaletteA[2] = 0xff00ff00u;
	PaletteA[3] = 0xffff0000u;
	UploadClut(448, PaletteA);
	TArray<uint32> PaletteB;
	PaletteB.SetNumZeroed(16);
	PaletteB[1] = 0xff00ffffu;
	PaletteB[2] = 0xffff00ffu;
	UploadClut(452, PaletteB);
	const auto UploadLevel = [&List](uint16 DBP, EGSPixelFormat Format, uint16 Size, uint8 Index)
	{
		TArray<uint8> Indices;
		const bool bIndex4 = Format == EGSPixelFormat::PSMT4;
		Indices.Init(bIndex4 ? uint8(Index | (Index << 4)) : Index, bIndex4 ? (Size * Size) / 2 : Size * Size);
		FGSBitBltBuf Destination;
		Destination.DBP = DBP;
		Destination.DBW = 2;
		Destination.DPSM = Format;
		List.UploadImage(Destination, 0, 0, Size, Size, Indices);
	};
	UploadLevel(460, EGSPixelFormat::PSMT8, 32, 1);
	UploadLevel(464, EGSPixelFormat::PSMT8, 16, 2);
	UploadLevel(465, EGSPixelFormat::PSMT8, 8, 3);
	UploadLevel(466, EGSPixelFormat::PSMT4, 16, 1);
	UploadLevel(467, EGSPixelFormat::PSMT4, 8, 2);
	List.TexFlush();

	// A texture's TEX0 with its CLUT's load control, its levels (the last repeated) and trilinear up to MXL.
	const auto UseTexture = [&List](bool bIndex8, uint8 CLD)
	{
		FGSTex1 Tex1;
		Tex1.MXL = bIndex8 ? 2 : 1;
		Tex1.MMAG = EGSFilter::Linear;
		Tex1.MMIN = EGSFilter::LinearMipmapLinear;
		List.SetTex1(0, Tex1);
		FGSMipTbp MipTbp1;
		FGSMipTbp MipTbp2;
		for (uint32 Level = 0; Level < 3; ++Level)
		{
			MipTbp1.TBP[Level] = bIndex8 ? uint16(Level == 0 ? 464 : 465) : 467;
			MipTbp1.TBW[Level] = 2;
			MipTbp2.TBP[Level] = bIndex8 ? 465 : 467;
			MipTbp2.TBW[Level] = 2;
		}
		List.SetMipTbp1(0, MipTbp1);
		List.SetMipTbp2(0, MipTbp2);
		FGSTex0 Tex0;
		Tex0.TBP0 = bIndex8 ? 460 : 466;
		Tex0.TBW = 2;
		Tex0.PSM = bIndex8 ? EGSPixelFormat::PSMT8 : EGSPixelFormat::PSMT4;
		Tex0.TW = bIndex8 ? 5 : 4;
		Tex0.TH = Tex0.TW;
		Tex0.TFX = EGSTextureFunction::Decal;
		Tex0.CBP = bIndex8 ? 448 : 452;
		Tex0.CSA = bIndex8 ? 0 : 1;
		Tex0.CLD = CLD;
		List.SetTex0(0, Tex0);
	};
	// Seven rows from x 0 to 63 whose Q falls from 1 to Q1 (the LOD rises from 0 to log2(1 / Q1)); s = t = 0.5.
	const auto AddQRamp = [&List](float Y0, float Q1)
	{
		FGSPrim Strip = Primitive(EGSPrimitive::TriangleStrip);
		Strip.bTextured = true;
		List.SetPrim(Strip);
		const float Corners[4][3] = {{0, Y0, 1.0f}, {63, Y0, Q1}, {0, Y0 + 7, 1.0f}, {63, Y0 + 7, Q1}};
		for (const auto& Corner : Corners)
		{
			List.SetST(FGSST{0.5f * Corner[2], 0.5f * Corner[2]});
			FGSRGBAQ Fragment = Color(0x80, 0x80, 0x80);
			Fragment.Q = Corner[2];
			List.SetRGBAQ(Fragment);
			List.AddVertex(Vertex(Corner[0], Corner[1]));
		}
	};
	// Rows 4-10: A with CLD 2 (loads, CBP0 = A: what the buffer held before is not known), red to blue (the LOD
	// passes MXL 2 before the end).
	UseTexture(true, 2);
	AddQRamp(4, 0.125f);
	// Rows 11-17: B at CSA 1 with CLD 3 (loads its 16 entries at 16..31, CBP1 = B), yellow to magenta.
	UseTexture(false, 3);
	AddQRamp(11, 0.25f);
	// Rows 18-24: A with CLD 4: CBP0 is A, no load; its entries 1..3 are still A's, so the same ramp as rows 4-10.
	UseTexture(true, 4);
	AddQRamp(18, 0.125f);
	// Rows 25-31: CLUT C (white, dark grey) uploaded over B, then B with CLD 3: the CLUT at CBP1 changed, so TEX0
	// loads it anyway (CLD 5 would keep B's), white to grey.
	TArray<uint32> PaletteC;
	PaletteC.SetNumZeroed(16);
	PaletteC[1] = 0xffffffffu;
	PaletteC[2] = 0xff404040u;
	UploadClut(452, PaletteC);
	UseTexture(false, 3);
	AddQRamp(25, 0.25f);
}

void GSConformance::BuildTwoPalettes(FGSCommandList& List)
{
	SetUpScene(List);
	// Two 16 x 16 PSMT8 textures in adjacent blocks (192 and 193: one block each) with their CLUTs in adjacent runs of
	// 4 blocks (128 and 132), as the texture cache packs them: on the GS a 16 x 16 PSMCT32 CLUT uploaded with DBW = 1
	// takes blocks CBP to CBP + 3 (manual 8.4), so the second CLUT leaves the first one whole.
	const auto UploadClut = [&List](uint16 CBP, uint32 (*EntryColor)(uint32))
	{
		TArray<uint32> Palette;
		for (uint32 Index = 0; Index < 256; ++Index)
		{
			Palette.Add(EntryColor(Index) | 0xff000000u);
		}
		TArray<uint8> Image;
		uint16 Width = 0;
		uint16 Height = 0;
		FGSTextureLayout::MakeClutImage(Palette, Image, Width, Height);
		FGSBitBltBuf Destination;
		Destination.DBP = CBP;
		Destination.DBW = 1;
		Destination.DPSM = EGSPixelFormat::PSMCT32;
		List.UploadImage(Destination, 0, 0, Width, Height, Image);
	};
	// Red to blue down the first texture, a green ramp down the second (entry = 16 y + x).
	UploadClut(128, [](uint32 Index) { return Index | ((255 - Index) << 16); });
	UploadClut(132, [](uint32 Index) { return 0x20u | (Index << 8) | (0x40u << 16); });
	TArray<uint8> Indices;
	for (uint32 Index = 0; Index < 256; ++Index)
	{
		Indices.Add(uint8(Index));
	}
	for (const uint16 TBP : {uint16(192), uint16(193)})
	{
		FGSBitBltBuf Destination;
		Destination.DBP = TBP;
		Destination.DBW = 2;
		Destination.DPSM = EGSPixelFormat::PSMT8;
		List.UploadImage(Destination, 0, 0, 16, 16, Indices);
	}
	List.TexFlush();
	List.SetTex1(0, FGSTex1());

	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto AddTexture = [&List, &Textured](uint16 TBP, uint16 CBP, float X)
	{
		FGSTex0 Tex0;
		Tex0.TBP0 = TBP;
		Tex0.TBW = 2;
		Tex0.PSM = EGSPixelFormat::PSMT8;
		Tex0.TW = 4;
		Tex0.TH = 4;
		Tex0.TFX = EGSTextureFunction::Decal;
		Tex0.CBP = CBP;
		Tex0.CLD = 1;
		List.SetTex0(0, Tex0);
		List.SetPrim(Textured);
		List.SetUV(FGSUV());
		List.AddVertex(Vertex(X, 0));
		FGSUV Corner;
		Corner.U = GSToFixed4(16, 14);
		Corner.V = GSToFixed4(16, 14);
		List.SetUV(Corner);
		List.AddVertex(Vertex(X + 16, 16));
	};
	AddTexture(192, 128, 0);
	AddTexture(193, 132, 20);
	// The first again, its CLUT loaded after the second's.
	AddTexture(192, 128, 40);
}

void GSConformance::BuildFunctionsFogAndMipmap(FGSCommandList& List)
{
	SetUpScene(List);
	TArray<uint32> Level0;
	Level0.Init(0x80c8c8c8u, 8 * 8);
	UploadTexture32(List, 64, 8, 8, Level0);
	TArray<uint32> Level1;
	Level1.Init(0x80000064u, 4 * 4);
	UploadTexture32(List, 128, 4, 4, Level1);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.TW = 3;
	Tex0.TH = 3;
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;

	const auto AddTexturedPixel = [&List, &Textured](float X, const FGSRGBAQ& Fragment)
	{
		List.SetPrim(Textured);
		List.SetRGBAQ(Fragment);
		FGSUV UV;
		UV.U = GSToFixed4(1, 14);
		UV.V = GSToFixed4(1, 14);
		List.SetUV(UV);
		List.AddVertex(Vertex(X, 0));
		List.AddVertex(Vertex(X + 1, 1));
	};
	List.SetTex1(0, FGSTex1());
	List.SetTex0(0, Tex0);
	AddTexturedPixel(0, Color(0x80, 0x80, 0x80));
	AddTexturedPixel(1, Color(0x40, 0x40, 0x40));
	FGSTex0 Highlight = Tex0;
	Highlight.TFX = EGSTextureFunction::Highlight;
	List.SetTex0(0, Highlight);
	AddTexturedPixel(2, Color(0x40, 0x40, 0x40, 0x10));

	// Fog: F = 0x80, FOGCOL black.
	List.SetFogCol(FGSFogCol());
	FGSPrim Fogged = Primitive(EGSPrimitive::Sprite);
	Fogged.bFog = true;
	List.SetPrim(Fogged);
	List.SetRGBAQ(Color(200, 200, 200));
	FGSXYZF Corner;
	Corner.X = GSToFixed4(4, 16);
	Corner.Y = 0;
	Corner.F = 0x80;
	List.AddVertex(Corner);
	Corner.X = GSToFixed4(5, 16);
	Corner.Y = GSToFixed4(1, 16);
	List.AddVertex(Corner);

	// MIPMAP: LCM = 1, K = 1.0, level 1 at TBP 128.
	FGSTex1 Mip;
	Mip.bFixedLOD = true;
	Mip.K = 16;
	Mip.MXL = 1;
	Mip.MMIN = EGSFilter::NearestMipmapNearest;
	List.SetTex1(0, Mip);
	FGSMipTbp MipTbp;
	MipTbp.TBP[0] = 128;
	MipTbp.TBW[0] = 1;
	List.SetMipTbp1(0, MipTbp);
	FGSTex0 Decal = Tex0;
	Decal.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Decal);
	AddTexturedPixel(6, Color(0x80, 0x80, 0x80));
}

void GSConformance::BuildPixelTests(FGSCommandList& List)
{
	SetUpScene(List, EGSPixelFormat::PSMCT32, false);
	AddSprite(List, 0, 0, 8, 1, Color(0x10, 0, 0), Primitive(EGSPrimitive::Sprite), 100);
	FGSTest AlphaKeep;
	AlphaKeep.bAlphaTest = true;
	AlphaKeep.ATST = EGSAlphaTest::GreaterEqual;
	AlphaKeep.AREF = 0x80;
	AlphaKeep.AFAIL = EGSAlphaFail::Keep;
	List.SetTest(0, AlphaKeep);
	AddSprite(List, 0, 0, 2, 1, Color(0xff, 0, 0, 0x40), Primitive(EGSPrimitive::Sprite), 200);
	FGSTest AlphaFrameOnly = AlphaKeep;
	AlphaFrameOnly.AFAIL = EGSAlphaFail::FrameBufferOnly;
	List.SetTest(0, AlphaFrameOnly);
	AddSprite(List, 2, 0, 4, 1, Color(0xff, 0, 0, 0x40), Primitive(EGSPrimitive::Sprite), 200);
	FGSTest DepthGEqual;
	DepthGEqual.ZTST = EGSDepthTest::GreaterEqual;
	List.SetTest(0, DepthGEqual);
	AddSprite(List, 4, 0, 5, 1, Color(0, 0xff, 0), Primitive(EGSPrimitive::Sprite), 100);
	AddSprite(List, 5, 0, 6, 1, Color(0, 0xff, 0), Primitive(EGSPrimitive::Sprite), 99);
	FGSTest DepthGreater;
	DepthGreater.ZTST = EGSDepthTest::Greater;
	List.SetTest(0, DepthGreater);
	AddSprite(List, 6, 0, 7, 1, Color(0, 0, 0xff), Primitive(EGSPrimitive::Sprite), 100);
}

void GSConformance::BuildBlendAndWrite(FGSCommandList& List)
{
	SetUpScene(List);
	AddSprite(List, 0, 0, 8, 1, Color(100, 100, 100));
	List.SetAlpha(0, FGSAlpha::Translucent());
	AddSprite(List, 0, 0, 1, 1, Color(200, 200, 200, 0x40), Blended(EGSPrimitive::Sprite));
	List.SetAlpha(0, FGSAlpha::Additive());
	AddSprite(List, 1, 0, 2, 1, Color(200, 200, 200, 0x80), Blended(EGSPrimitive::Sprite));
	List.SetColorClamp(false);
	AddSprite(List, 2, 0, 3, 1, Color(200, 200, 200, 0x80), Blended(EGSPrimitive::Sprite));
	List.SetColorClamp(true);
	List.SetPixelAlphaBlend(true);
	AddSprite(List, 3, 0, 4, 1, Color(50, 50, 50, 0x40), Blended(EGSPrimitive::Sprite));
	List.SetPixelAlphaBlend(false);
	List.SetFba(0, true);
	AddSprite(List, 4, 0, 5, 1, Color(10, 10, 10, 0x10));
	List.SetFba(0, false);
	FGSFrame Masked = MakeFrame();
	Masked.FBMSK = 0x0000ff00u;
	List.SetFrame(0, Masked);
	AddSprite(List, 5, 0, 6, 1, Color(1, 2, 3));
}

void GSConformance::BuildDither16(FGSCommandList& List)
{
	SetUpScene(List, EGSPixelFormat::PSMCT16S);
	List.SetDimx(FGSDimx::Default());
	List.SetDither(true);
	AddSprite(List, 0, 0, 2, 1, Color(103, 103, 103));
	List.SetDither(false);
	AddSprite(List, 0, 1, 2, 2, Color(103, 103, 103));
}

void GSConformance::BuildMipmapLod(FGSCommandList& List)
{
	SetUpScene(List);
	// Texture A: 16 x 16 with a solid colour per level (red, green, blue, yellow, magenta), levels in blocks 64..71.
	static constexpr uint32 LevelColors[5] = {0x800000ffu, 0x8000ff00u, 0x80ff0000u, 0x8000ffffu, 0x80ff00ffu};
	uint16 SolidLevels[5] = {};
	uint16 Next = 64;
	for (uint32 Level = 0; Level < 5; ++Level)
	{
		// A 1 x 1 level goes up as 2 x 2 (the GS moves 32-bit images two pixels at a time).
		const uint16 Size = uint16(FMath::Max(16u >> Level, 2u));
		TArray<uint32> Texels;
		Texels.Init(LevelColors[Level], Size * Size);
		UploadTexture32(List, Next, Size, Size, Texels);
		SolidLevels[Level] = Next;
		Next = uint16(Next + FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, Size, Size));
	}
	// Texture B: 32 x 32 with a pattern per level (levels 0 to 2: 32, 16 and 8 texels, as the manual asks of bilinear
	// levels), in blocks 96 on.
	uint16 PatternLevels[3] = {};
	Next = 96;
	for (uint32 Level = 0; Level < 3; ++Level)
	{
		const uint32 Size = 32u >> Level;
		TArray<uint32> Texels;
		for (uint32 V = 0; V < Size; ++V)
		{
			for (uint32 U = 0; U < Size; ++U)
			{
				const uint32 Step = 8u << Level;
				const uint32 Colors[3] = {(U * Step) | ((V * Step) << 8) | (0x40u << 16),
					(255 - (U * Step)) | ((V * Step) << 8) | (0xc0u << 16),
					(U * Step) | ((255 - (V * Step)) << 8) | (0x20u << 16)};
				Texels.Add(Colors[Level] | 0x80000000u);
			}
		}
		UploadTexture32(List, Next, uint16(Size), uint16(Size), Texels);
		PatternLevels[Level] = Next;
		Next = uint16(Next + FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, Size, Size));
	}

	const auto UseTexture = [&List](const uint16* Levels, uint32 NumLevels, const FGSTex1& Tex1)
	{
		// Texture A is 16 x 16 (5 levels), B 32 x 32 (3 levels).
		const uint8 SizeLog2 = NumLevels == 5 ? 4 : 5;
		FGSTex0 Tex0;
		Tex0.TBP0 = Levels[0];
		Tex0.TBW = 1;
		Tex0.TW = SizeLog2;
		Tex0.TH = SizeLog2;
		Tex0.TFX = EGSTextureFunction::Decal;
		List.SetTex1(0, Tex1);
		List.SetTex0(0, Tex0);
		FGSMipTbp MipTbp1;
		FGSMipTbp MipTbp2;
		for (uint32 Level = 1; Level <= 6; ++Level)
		{
			FGSMipTbp& MipTbp = Level <= 3 ? MipTbp1 : MipTbp2;
			MipTbp.TBP[(Level - 1) % 3] = Levels[FMath::Min(Level, NumLevels - 1)];
			MipTbp.TBW[(Level - 1) % 3] = 1;
		}
		List.SetMipTbp1(0, MipTbp1);
		List.SetMipTbp2(0, MipTbp2);
	};
	const auto MakeTex1 = [](EGSFilter MMIN, uint8 MXL, uint8 L, int16 K, bool bFixed)
	{
		FGSTex1 Tex1;
		Tex1.bFixedLOD = bFixed;
		Tex1.MXL = MXL;
		Tex1.MMIN = MMIN;
		Tex1.L = L;
		Tex1.K = K;
		return Tex1;
	};
	// A quad from x 0 to 63 whose Q falls from Q0 to Q1 (the LOD rises with log2(1 / Q)); s = t = 0.5.
	const auto AddQRamp = [&List](float Y0, float Y1, float Q0, float Q1)
	{
		FGSPrim Strip = Primitive(EGSPrimitive::TriangleStrip);
		Strip.bTextured = true;
		List.SetPrim(Strip);
		const float Corners[4][3] = {{0, Y0, Q0}, {63, Y0, Q1}, {0, Y1, Q0}, {63, Y1, Q1}};
		for (const auto& Corner : Corners)
		{
			List.SetST(FGSST{0.5f * Corner[2], 0.5f * Corner[2]});
			FGSRGBAQ Fragment = Color(0x80, 0x80, 0x80);
			Fragment.Q = Corner[2];
			List.SetRGBAQ(Fragment);
			List.AddVertex(Vertex(Corner[0], Corner[1]));
		}
	};

	// Rows 0-11: the LOD from Q (LCM = 0) with NEAREST_MIPMAP_NEAREST (the level rounded), NEAREST_MIPMAP_LINEAR
	// (two levels blended by the LOD's fraction), and LINEAR_MIPMAP_LINEAR with L = 1, K = -0.5 up to MXL 3.
	UseTexture(SolidLevels, 5, MakeTex1(EGSFilter::NearestMipmapNearest, 4, 0, 0, false));
	AddQRamp(0, 4, 1.0f, 0.0625f);
	UseTexture(SolidLevels, 5, MakeTex1(EGSFilter::NearestMipmapLinear, 4, 0, 0, false));
	AddQRamp(4, 8, 1.0f, 0.0625f);
	UseTexture(SolidLevels, 5, MakeTex1(EGSFilter::LinearMipmapLinear, 3, 1, -8, false));
	AddQRamp(8, 12, 1.0f, 0.0625f);

	// Rows 12-15: a fixed LOD (LCM = 1) per 8-pixel cell: K <= 0 takes MMAG and level 0, then the MMIN filters.
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto AddUVSprite = [&List, &Textured](
								 float X0, float Y0, float X1, float Y1, float U0, float V0, float U1, float V1)
	{
		List.SetPrim(Textured);
		List.SetUV(FGSUV{GSToFixed4(U0, 14), GSToFixed4(V0, 14)});
		List.AddVertex(Vertex(X0, Y0));
		List.SetUV(FGSUV{GSToFixed4(U1, 14), GSToFixed4(V1, 14)});
		List.AddVertex(Vertex(X1, Y1));
	};
	struct FFixedCell
	{
		int16 K;
		EGSFilter MMIN;
		uint8 MXL;
	};
	static constexpr FFixedCell Cells[8] = {{-16, EGSFilter::NearestMipmapNearest, 4},
		{0, EGSFilter::NearestMipmapLinear, 4}, {7, EGSFilter::NearestMipmapNearest, 4},
		{9, EGSFilter::NearestMipmapNearest, 4}, {28, EGSFilter::NearestMipmapNearest, 4},
		{36, EGSFilter::NearestMipmapLinear, 4}, {52, EGSFilter::LinearMipmapLinear, 3},
		{80, EGSFilter::NearestMipmapNearest, 4}};
	for (uint32 Cell = 0; Cell < 8; ++Cell)
	{
		UseTexture(SolidLevels, 5, MakeTex1(Cells[Cell].MMIN, Cells[Cell].MXL, 0, Cells[Cell].K, true));
		AddUVSprite(float(Cell * 8), 12, float(Cell * 8) + 8, 16, 8, 8, 8, 8);
	}

	// Rows 16-23: the pattern's levels, bilinear within level 1, point in level 1, trilinear between 1 and 2 at
	// LOD 1.25, and a LOD below 0 from Q = 2 (both vertices) that takes MMAG (bilinear) on level 0, magnified.
	UseTexture(PatternLevels, 3, MakeTex1(EGSFilter::LinearMipmapNearest, 2, 0, 16, true));
	AddUVSprite(0, 16, 16, 24, 0, 0, 8, 4);
	UseTexture(PatternLevels, 3, MakeTex1(EGSFilter::NearestMipmapNearest, 2, 0, 16, true));
	AddUVSprite(16, 16, 32, 24, 0, 0, 8, 4);
	UseTexture(PatternLevels, 3, MakeTex1(EGSFilter::LinearMipmapLinear, 2, 0, 20, true));
	AddUVSprite(32, 16, 48, 24, 0, 0, 16, 8);
	FGSTex1 Magnified = MakeTex1(EGSFilter::NearestMipmapNearest, 2, 0, 0, false);
	Magnified.MMAG = EGSFilter::Linear;
	UseTexture(PatternLevels, 3, Magnified);
	FGSPrim SpriteSTQ = Primitive(EGSPrimitive::Sprite);
	SpriteSTQ.bTextured = true;
	List.SetPrim(SpriteSTQ);
	FGSRGBAQ Near = Color(0x80, 0x80, 0x80);
	Near.Q = 2.0f;
	List.SetRGBAQ(Near);
	List.SetST(FGSST{0.0f, 0.0f});
	List.AddVertex(Vertex(48, 16));
	List.SetST(FGSST{0.5f, 0.25f});
	List.AddVertex(Vertex(64, 24));

	// Rows 24-31: a floor in perspective (Q from 0.25 at the top to 1 at the bottom), trilinear with bilinear levels.
	UseTexture(PatternLevels, 3, MakeTex1(EGSFilter::LinearMipmapLinear, 2, 0, 0, false));
	FGSPrim Floor = Primitive(EGSPrimitive::TriangleStrip);
	Floor.bTextured = true;
	List.SetPrim(Floor);
	const float FloorCorners[4][4] = {{0, 24, 0, 0.25f}, {63, 24, 1, 0.25f}, {0, 31, 0, 1}, {63, 31, 1, 1}};
	for (const auto& Corner : FloorCorners)
	{
		const float Q = Corner[3];
		// t from 1/16 (level 2's first texel center: no bilinear across the wrap there) to 1.
		List.SetST(FGSST{Corner[2] * Q, (0.0625f + (0.9375f * (Corner[1] - 24.0f) / 7.0f)) * Q});
		FGSRGBAQ Fragment = Color(0x80, 0x80, 0x80);
		Fragment.Q = Q;
		List.SetRGBAQ(Fragment);
		List.AddVertex(Vertex(Corner[0], Corner[1]));
	}
}

void GSConformance::BuildAlphaTest(FGSCommandList& List)
{
	SetUpScene(List, EGSPixelFormat::PSMCT32, false);
	// A 16 x 1 texture whose texel x has alpha 8 + 16 x (texel 7 is AREF, 0x78) and red 16 x.
	TArray<uint32> Ramp;
	for (uint32 X = 0; X < 16; ++X)
	{
		Ramp.Add((X * 16) | (0x80u << 8) | (0x40u << 16) | ((8 + (X * 16)) << 24));
	}
	UploadTexture32(List, 64, 16, 1, Ramp);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.TW = 4;
	Tex0.TH = 0;
	Tex0.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Tex0);
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	// Rows 0-15: the eight methods against AREF 0x78, two rows each, what fails not drawn (KEEP).
	for (uint8 Method = 0; Method < 8; ++Method)
	{
		FGSTest Test;
		Test.bAlphaTest = true;
		Test.ATST = EGSAlphaTest(Method);
		Test.AREF = 0x78;
		List.SetTest(0, Test);
		List.SetPrim(Textured);
		List.SetUV(FGSUV{0, GSToFixed4(0.5f, 14)});
		List.AddVertex(Vertex(0, float(Method * 2)));
		List.SetUV(FGSUV{GSToFixed4(16, 14), GSToFixed4(0.5f, 14)});
		List.AddVertex(Vertex(16, float(Method * 2) + 2));
	}

	// Columns 20-59: what AFAIL still writes for a pixel that fails (NEVER), over a background at Z 100. A green
	// sprite at Z 150 with GEQUAL over the lower half then shows where the failing pixel wrote Z 200.
	List.SetTest(0, FGSTest());
	AddSprite(List, 20, 0, 60, 16, Color(0x20, 0x20, 0x20, 0x10), Primitive(EGSPrimitive::Sprite), 100);
	for (uint8 Fail = 0; Fail < 4; ++Fail)
	{
		FGSTest Test;
		Test.bAlphaTest = true;
		Test.ATST = EGSAlphaTest::Never;
		Test.AFAIL = EGSAlphaFail(Fail);
		List.SetTest(0, Test);
		const float Left = 20.0f + (float(Fail) * 10.0f);
		AddSprite(List, Left, 0, Left + 10, 16, Color(0xff, 0, 0, 0x60), Primitive(EGSPrimitive::Sprite), 200);
	}
	FGSTest GreaterEqual;
	GreaterEqual.ZTST = EGSDepthTest::GreaterEqual;
	List.SetTest(0, GreaterEqual);
	AddSprite(List, 20, 8, 60, 16, Color(0, 0xff, 0, 0x70), Primitive(EGSPrimitive::Sprite), 150);

	// Rows 16-31: a Gouraud alpha ramp tested GREATER 0x40 with FB_ONLY over a blue floor: the failing half writes its
	// colour but not Z, so a later sprite at the same Z (GREATER) covers only it.
	List.SetTest(0, FGSTest());
	AddSprite(List, 0, 16, 64, 32, Color(0, 0, 0x80, 0x80), Primitive(EGSPrimitive::Sprite), 10);
	FGSTest Ramped;
	Ramped.bAlphaTest = true;
	Ramped.ATST = EGSAlphaTest::Greater;
	Ramped.AREF = 0x40;
	Ramped.AFAIL = EGSAlphaFail::FrameBufferOnly;
	List.SetTest(0, Ramped);
	FGSPrim Gouraud = Primitive(EGSPrimitive::TriangleStrip);
	Gouraud.bGouraud = true;
	List.SetPrim(Gouraud);
	const float RampCorners[4][3] = {{0, 16, 0}, {63, 16, 0x80}, {0, 24, 0}, {63, 24, 0x80}};
	for (const auto& Corner : RampCorners)
	{
		List.SetRGBAQ(Color(0xc0, 0x60, 0x20, uint8(Corner[2])));
		List.AddVertex(Vertex(Corner[0], Corner[1], 50));
	}
	FGSTest Greater;
	Greater.ZTST = EGSDepthTest::Greater;
	List.SetTest(0, Greater);
	AddSprite(List, 0, 16, 64, 24, Color(0x10, 0xe0, 0x10, 0x80), Primitive(EGSPrimitive::Sprite), 50);
}

void GSConformance::BuildFog(FGSCommandList& List)
{
	SetUpScene(List);
	FGSFogCol FogCol;
	FogCol.R = 0x20;
	FogCol.G = 0x40;
	FogCol.B = 0xc0;
	List.SetFogCol(FogCol);
	const auto FoggedVertex = [](float X, float Y, uint8 F)
	{
		FGSXYZF Result;
		Result.X = GSToFixed4(X, 16);
		Result.Y = GSToFixed4(Y, 16);
		Result.F = F;
		return Result;
	};

	// Rows 0-7: a flat quad, F from 0 at x 0 to 255 at x 63 (the coefficient is interpolated whatever IIP says).
	FGSPrim Strip = Primitive(EGSPrimitive::TriangleStrip);
	Strip.bFog = true;
	List.SetPrim(Strip);
	List.SetRGBAQ(Color(200, 100, 50));
	List.AddVertex(FoggedVertex(0, 0, 0));
	List.AddVertex(FoggedVertex(63, 0, 255));
	List.AddVertex(FoggedVertex(0, 8, 0));
	List.AddVertex(FoggedVertex(63, 8, 255));

	// Rows 8-15: a textured (MODULATE) quad with F from 255 down to 0.
	TArray<uint32> Texels;
	for (uint32 V = 0; V < 8; ++V)
	{
		for (uint32 U = 0; U < 8; ++U)
		{
			Texels.Add((U * 32) | ((V * 32) << 8) | (0xa0u << 16) | 0x80000000u);
		}
	}
	UploadTexture32(List, 64, 8, 8, Texels);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.TW = 3;
	Tex0.TH = 3;
	List.SetTex0(0, Tex0);
	FGSPrim Textured = Strip;
	Textured.bTextured = true;
	Textured.bUseUV = true;
	List.SetPrim(Textured);
	List.SetRGBAQ(Color(0x80, 0x80, 0x80));
	const float TexturedCorners[4][4] = {{0, 8, 0, 255}, {63, 8, 8, 0}, {0, 16, 0, 255}, {63, 16, 8, 0}};
	for (const auto& Corner : TexturedCorners)
	{
		List.SetUV(FGSUV{GSToFixed4(Corner[2], 14), GSToFixed4(Corner[1] - 8.0f, 14)});
		List.AddVertex(FoggedVertex(Corner[0], Corner[1], uint8(Corner[3])));
	}

	// Rows 16-19: sprites take F from their second vertex: 0x60, then 0 (the fog colour).
	FGSPrim Sprite = Primitive(EGSPrimitive::Sprite);
	Sprite.bFog = true;
	List.SetPrim(Sprite);
	List.SetRGBAQ(Color(250, 250, 250));
	List.AddVertex(FoggedVertex(0, 16, 0));
	List.AddVertex(FoggedVertex(32, 20, 0x60));
	List.AddVertex(FoggedVertex(32, 16, 255));
	List.AddVertex(FoggedVertex(64, 20, 0));

	// Rows 20-23: XYZ2 takes F from the FOG register as it is at the vertex.
	List.SetPrim(Strip);
	List.SetRGBAQ(Color(40, 220, 90));
	const float RegisterCorners[4][3] = {{0, 20, 0x10}, {63, 20, 0xf0}, {0, 24, 0x10}, {63, 24, 0xf0}};
	for (const auto& Corner : RegisterCorners)
	{
		List.SetFog(FGSFog{uint8(Corner[2])});
		List.AddVertex(Vertex(Corner[0], Corner[1]));
	}

	// Rows 24-31: a Gouraud strip whose fourth vertex is XYZF3: its triangle (2, 3, 4) is not drawn.
	FGSPrim GouraudStrip = Strip;
	GouraudStrip.bGouraud = true;
	List.SetPrim(GouraudStrip);
	const float BandCorners[8][2] = {{0, 24}, {0, 31}, {21, 24}, {21, 31}, {42, 24}, {42, 31}, {63, 24}, {63, 31}};
	for (int32 Index = 0; Index < 8; ++Index)
	{
		List.SetRGBAQ(Color(uint8(40 + (Index * 25)), 180, uint8(220 - (Index * 25))));
		const FGSXYZF Corner = FoggedVertex(BandCorners[Index][0], BandCorners[Index][1], uint8(255 - (Index * 30)));
		if (Index == 3)
		{
			List.AddVertexNoKick(Corner);
		}
		else
		{
			List.AddVertex(Corner);
		}
	}
}

void GSConformance::BuildTexAAndFunctions(FGSCommandList& List)
{
	SetUpScene(List);
	// A 4 x 2 PSMCT16 texture: white A = 0, red A = 1, black A = 0, black A = 1.
	TArray<uint8> Texels16;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (const uint16 Texel : {uint16(0x7fff), uint16(0x801f), uint16(0x0000), uint16(0x8000)})
		{
			Texels16.Add(uint8(Texel));
			Texels16.Add(uint8(Texel >> 8));
		}
	}
	FGSBitBltBuf Buf16;
	Buf16.DBP = 64;
	Buf16.DBW = 1;
	Buf16.DPSM = EGSPixelFormat::PSMCT16;
	List.UploadImage(Buf16, 0, 0, 4, 2, Texels16);
	// An 8 x 2 PSMCT24 texture: white, black, a blue grey, black, then the same again.
	TArray<uint8> Texels24;
	for (uint32 Index = 0; Index < 16; ++Index)
	{
		static constexpr uint32 Colors[4] = {0xffffffu, 0x000000u, 0xc08040u, 0x000000u};
		const uint32 Texel = Colors[Index % 4];
		Texels24.Add(uint8(Texel));
		Texels24.Add(uint8(Texel >> 8));
		Texels24.Add(uint8(Texel >> 16));
	}
	FGSBitBltBuf Buf24;
	Buf24.DBP = 96;
	Buf24.DBW = 1;
	Buf24.DPSM = EGSPixelFormat::PSMCT24;
	List.UploadImage(Buf24, 0, 0, 8, 2, Texels24);
	// A 4 x 1 PSMCT32 texture, (200, 100, 50) with alpha 0, 0x40, 0x80 and 0xff.
	TArray<uint32> Texels32;
	for (const uint32 Alpha : {0x00u, 0x40u, 0x80u, 0xffu})
	{
		Texels32.Add(200u | (100u << 8) | (50u << 16) | (Alpha << 24));
	}
	UploadTexture32(List, 128, 4, 1, Texels32);

	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto AddTextured = [&List, &Textured](uint16 TBP, EGSPixelFormat PSM, uint8 TW, EGSTextureFunction TFX,
								 bool bRGBA, float X0, float Y0, float X1, float Y1, const FGSRGBAQ& Fragment)
	{
		FGSTex0 Tex0;
		Tex0.TBP0 = TBP;
		Tex0.TBW = 1;
		Tex0.PSM = PSM;
		Tex0.TW = TW;
		Tex0.TH = 1;
		Tex0.TFX = TFX;
		Tex0.bRGBA = bRGBA;
		List.SetTex0(0, Tex0);
		List.SetPrim(Textured);
		List.SetRGBAQ(Fragment);
		List.SetUV(FGSUV{0, GSToFixed4(0.5f, 14)});
		List.AddVertex(Vertex(X0, Y0));
		List.SetUV(FGSUV{GSToFixed4(float(1 << TW), 14), GSToFixed4(0.5f, 14)});
		List.AddVertex(Vertex(X1, Y1));
	};
	const auto MakeTexA = [](uint8 TA0, uint8 TA1, bool bAEM)
	{
		FGSTexA TexA;
		TexA.TA0 = TA0;
		TexA.TA1 = TA1;
		TexA.bAlphaExpandBlack = bAEM;
		return TexA;
	};
	const FGSRGBAQ Plain = Color(0x80, 0x80, 0x80);

	// Rows 0-3: TEXA's alpha (DECAL, RGBA): 16 bits with AEM off and on, 24 bits with AEM off and on.
	List.SetTexA(MakeTexA(0x30, 0xa0, false));
	AddTextured(64, EGSPixelFormat::PSMCT16, 2, EGSTextureFunction::Decal, true, 0, 0, 16, 4, Plain);
	List.SetTexA(MakeTexA(0x30, 0xa0, true));
	AddTextured(64, EGSPixelFormat::PSMCT16, 2, EGSTextureFunction::Decal, true, 16, 0, 32, 4, Plain);
	List.SetTexA(MakeTexA(0x50, 0xa0, false));
	AddTextured(96, EGSPixelFormat::PSMCT24, 3, EGSTextureFunction::Decal, true, 32, 0, 48, 4, Plain);
	List.SetTexA(MakeTexA(0x50, 0xa0, true));
	AddTextured(96, EGSPixelFormat::PSMCT24, 3, EGSTextureFunction::Decal, true, 48, 0, 64, 4, Plain);

	// Rows 4-19: every texture function with TCC RGB and RGBA, two rows each, on the 32-bit texture (a dim fragment,
	// then a white one that clamps), the 16-bit one (TEXA's alpha) and the 24-bit one.
	List.SetTexA(MakeTexA(0x30, 0xa0, true));
	for (uint8 Function = 0; Function < 4; ++Function)
	{
		for (uint8 RGBA = 0; RGBA < 2; ++RGBA)
		{
			const float Y = 4.0f + float(((Function * 2) + RGBA) * 2);
			const EGSTextureFunction TFX = EGSTextureFunction(Function);
			AddTextured(
				128, EGSPixelFormat::PSMCT32, 2, TFX, RGBA != 0, 0, Y, 16, Y + 2, Color(0x60, 0x80, 0xa0, 0x50));
			AddTextured(
				128, EGSPixelFormat::PSMCT32, 2, TFX, RGBA != 0, 16, Y, 32, Y + 2, Color(0xff, 0xff, 0xff, 0xff));
			AddTextured(
				64, EGSPixelFormat::PSMCT16, 2, TFX, RGBA != 0, 32, Y, 48, Y + 2, Color(0x60, 0x80, 0xa0, 0x50));
			AddTextured(
				96, EGSPixelFormat::PSMCT24, 3, TFX, RGBA != 0, 48, Y, 64, Y + 2, Color(0x60, 0x80, 0xa0, 0x50));
		}
	}

	// Rows 20-31: TEXA changed between draws of the same texture (the alpha follows the register).
	List.SetTexA(MakeTexA(0x00, 0xff, false));
	AddTextured(64, EGSPixelFormat::PSMCT16, 2, EGSTextureFunction::Modulate, true, 0, 20, 32, 32, Plain);
	List.SetTexA(MakeTexA(0x7f, 0x01, true));
	AddTextured(64, EGSPixelFormat::PSMCT16, 2, EGSTextureFunction::Modulate, true, 32, 20, 64, 32, Plain);
}

void GSConformance::BuildClampModes(FGSCommandList& List)
{
	SetUpScene(List);
	// A 16 x 16 texture: texel (u, v) is (16 u + 8, 16 v + 8) with a checker in blue; level 1 (8 x 8) at block 68.
	TArray<uint32> Level0;
	for (uint32 V = 0; V < 16; ++V)
	{
		for (uint32 U = 0; U < 16; ++U)
		{
			Level0.Add(
				((U * 16) + 8) | (((V * 16) + 8) << 8) | ((0x80u + (((U + V) & 1) * 0x40u)) << 16) | 0x80000000u);
		}
	}
	UploadTexture32(List, 64, 16, 16, Level0);
	TArray<uint32> Level1;
	for (uint32 V = 0; V < 8; ++V)
	{
		for (uint32 U = 0; U < 8; ++U)
		{
			Level1.Add(((U * 32) + 16) | ((0xffu - (V * 32)) << 8) | (0x20u << 16) | 0x80000000u);
		}
	}
	UploadTexture32(List, 68, 8, 8, Level1);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.TW = 4;
	Tex0.TH = 4;
	Tex0.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Tex0);
	FGSMipTbp MipTbp;
	MipTbp.TBP[0] = 68;
	MipTbp.TBW[0] = 1;
	List.SetMipTbp1(0, MipTbp);

	const auto MakeClamp = [](EGSWrapMode Mode, uint16 MinU, uint16 MaxU, uint16 MinV, uint16 MaxV)
	{
		FGSClamp Clamp;
		Clamp.WMS = Mode;
		Clamp.WMT = Mode;
		Clamp.MINU = MinU;
		Clamp.MAXU = MaxU;
		Clamp.MINV = MinV;
		Clamp.MAXV = MaxV;
		return Clamp;
	};
	// Per row: REPEAT, CLAMP, REGION_CLAMP and REGION_REPEAT (UMSK / VMSK then UFIX / VFIX in MIN / MAX).
	const auto RowClamps = [&MakeClamp](uint16 ClampMinU, uint16 ClampMaxU, uint16 ClampMinV, uint16 ClampMaxV,
							   uint16 MaskU, uint16 FixU, uint16 MaskV, uint16 FixV)
	{
		TArray<FGSClamp> Clamps;
		Clamps.Add(MakeClamp(EGSWrapMode::Repeat, 0, 0, 0, 0));
		Clamps.Add(MakeClamp(EGSWrapMode::Clamp, 0, 0, 0, 0));
		Clamps.Add(MakeClamp(EGSWrapMode::RegionClamp, ClampMinU, ClampMaxU, ClampMinV, ClampMaxV));
		Clamps.Add(MakeClamp(EGSWrapMode::RegionRepeat, MaskU, FixU, MaskV, FixV));
		return Clamps;
	};
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto AddUVSprite = [&List, &Textured](float X0, float Y0, float U0, float V0, float U1, float V1)
	{
		List.SetPrim(Textured);
		List.SetUV(FGSUV{GSToFixed4(U0, 14), GSToFixed4(V0, 14)});
		List.AddVertex(Vertex(X0, Y0));
		List.SetUV(FGSUV{GSToFixed4(U1, 14), GSToFixed4(V1, 14)});
		List.AddVertex(Vertex(X0 + 16, Y0 + 8));
	};

	// Row 0-7: point sampling over two texture widths (u 0..32, v 0..32).
	List.SetTex1(0, FGSTex1());
	const TArray<FGSClamp> Point = RowClamps(3, 12, 2, 9, 7, 8, 3, 4);
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		List.SetClamp(0, Point[Mode]);
		AddUVSprite(float(Mode * 16), 0, 0, 0, 32, 32);
	}
	// Rows 8-15: bilinear, magnified, across the edges (u 10.25..18.25, v 5.25..9.25: half a texel a pixel).
	FGSTex1 Bilinear;
	Bilinear.MMAG = EGSFilter::Linear;
	List.SetTex1(0, Bilinear);
	const TArray<FGSClamp> Linear = RowClamps(12, 14, 6, 8, 3, 4, 3, 4);
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		List.SetClamp(0, Linear[Mode]);
		AddUVSprite(float(Mode * 16), 8, 10.25f, 5.25f, 18.25f, 9.25f);
	}
	// Rows 16-23: MIPMAP level 1 (a fixed LOD of 1): REGION_CLAMP's range shifts right by the level.
	FGSTex1 Level1Fixed;
	Level1Fixed.bFixedLOD = true;
	Level1Fixed.K = 16;
	Level1Fixed.MXL = 1;
	Level1Fixed.MMIN = EGSFilter::NearestMipmapNearest;
	List.SetTex1(0, Level1Fixed);
	const TArray<FGSClamp> Mip = RowClamps(4, 11, 2, 13, 3, 4, 1, 2);
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		List.SetClamp(0, Mip[Mode]);
		AddUVSprite(float(Mode * 16), 16, 0, 0, 32, 32);
	}
	// Rows 24-31: STQ from s = -1 to 1 (negative texel coordinates), point sampling.
	List.SetTex1(0, FGSTex1());
	FGSPrim SpriteSTQ = Primitive(EGSPrimitive::Sprite);
	SpriteSTQ.bTextured = true;
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		List.SetClamp(0, Point[Mode]);
		List.SetPrim(SpriteSTQ);
		List.SetRGBAQ(Color(0x80, 0x80, 0x80));
		List.SetST(FGSST{-1.0f, -0.5f});
		List.AddVertex(Vertex(float(Mode * 16), 24));
		List.SetST(FGSST{1.0f, 0.5f});
		List.AddVertex(Vertex(float(Mode * 16) + 16, 32));
	}
}

void GSConformance::BuildStripsAndSprites(FGSCommandList& List)
{
	SetUpScene(List);
	// Rows 0-7, columns 0-20: the manual's strip (3.2.5) with its fourth vertex through XYZ3: triangle (2, 3, 4) is
	// not drawn, the strip goes on from it.
	FGSPrim Strip = Primitive(EGSPrimitive::TriangleStrip);
	Strip.bGouraud = true;
	List.SetPrim(Strip);
	const float StripCorners[6][2] = {{0, 0}, {0, 8}, {10, 0}, {10, 8}, {21, 0}, {21, 8}};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		List.SetRGBAQ(Color(uint8(250 - (Index * 40)), uint8(Index * 45), uint8(60 + (Index * 30))));
		if (Index == 3)
		{
			List.AddVertexNoKick(Vertex(StripCorners[Index][0], StripCorners[Index][1]));
		}
		else
		{
			List.AddVertex(Vertex(StripCorners[Index][0], StripCorners[Index][1]));
		}
	}
	// Columns 24-47: a flat fan whose fourth vertex is XYZ3: triangle (1, 3, 4) is not drawn.
	List.SetPrim(Primitive(EGSPrimitive::TriangleFan));
	const float FanCorners[6][2] = {{36, 4}, {26, 0}, {46, 0}, {47, 8}, {36, 8.5f}, {24, 8}};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		List.SetRGBAQ(Color(uint8(Index * 50), 200, uint8(250 - (Index * 50))));
		if (Index == 3)
		{
			List.AddVertexNoKick(Vertex(FanCorners[Index][0], FanCorners[Index][1]));
		}
		else
		{
			List.AddVertex(Vertex(FanCorners[Index][0], FanCorners[Index][1]));
		}
	}

	// A 16 x 8 texture for the sprites: (16 u, 32 v, 0x80).
	TArray<uint32> Texels;
	for (uint32 V = 0; V < 8; ++V)
	{
		for (uint32 U = 0; U < 16; ++U)
		{
			Texels.Add((U * 16) | ((V * 32) << 8) | (0x80u << 16) | 0x80000000u);
		}
	}
	UploadTexture32(List, 64, 16, 8, Texels);
	FGSTex0 Tex0;
	Tex0.TBP0 = 64;
	Tex0.TBW = 1;
	Tex0.TW = 4;
	Tex0.TH = 3;
	Tex0.TFX = EGSTextureFunction::Decal;
	List.SetTex0(0, Tex0);
	List.SetTex1(0, FGSTex1());
	// Rows 8-15, columns 0-15: a sprite given from its bottom-right corner (UV 0.25, 0.25) to its top-left (16.25,
	// 8.25): the texture is mirrored both ways (a quarter texel in, off the texel borders).
	FGSPrim UVSprite = Primitive(EGSPrimitive::Sprite);
	UVSprite.bTextured = true;
	UVSprite.bUseUV = true;
	List.SetPrim(UVSprite);
	List.SetUV(FGSUV{GSToFixed4(0.25f, 14), GSToFixed4(0.25f, 14)});
	List.AddVertex(Vertex(16, 16));
	List.SetUV(FGSUV{GSToFixed4(16.25f, 14), GSToFixed4(8.25f, 14)});
	List.AddVertex(Vertex(0, 8));
	// Columns 16-31: a sprite with STQ: S, T span it and Q is the second vertex's (2), so s runs 0..1 and t 0..0.5.
	FGSPrim STQSprite = Primitive(EGSPrimitive::Sprite);
	STQSprite.bTextured = true;
	List.SetPrim(STQSprite);
	FGSRGBAQ First = Color(0x80, 0x80, 0x80);
	First.Q = 1.0f;
	List.SetRGBAQ(First);
	List.SetST(FGSST{0.0f, 0.0f});
	List.AddVertex(Vertex(16, 8));
	FGSRGBAQ Second = Color(0x80, 0x80, 0x80);
	Second.Q = 2.0f;
	List.SetRGBAQ(Second);
	List.SetST(FGSST{2.0f, 1.0f});
	List.AddVertex(Vertex(32, 16));

	// Rows 16-31: lines and points: shallow, steep, right to left, Gouraud, a strip, points off the pixel centers.
	FGSPrim Line = Primitive(EGSPrimitive::Line);
	List.SetPrim(Line);
	List.SetRGBAQ(Color(0xff, 0xff, 0));
	List.AddVertex(Vertex(33.3f, 17.2f));
	List.AddVertex(Vertex(60.6f, 21.9f));
	List.SetRGBAQ(Color(0, 0xff, 0xff));
	List.AddVertex(Vertex(35.5f, 18.4f));
	List.AddVertex(Vertex(38.2f, 31.0f));
	List.SetRGBAQ(Color(0xff, 0x80, 0));
	List.AddVertex(Vertex(62.0f, 24.3f));
	List.AddVertex(Vertex(40.1f, 28.8f));
	FGSPrim GouraudLine = Line;
	GouraudLine.bGouraud = true;
	List.SetPrim(GouraudLine);
	List.SetRGBAQ(Color(0xff, 0, 0));
	List.AddVertex(Vertex(34.0f, 30.0f));
	List.SetRGBAQ(Color(0, 0, 0xff));
	List.AddVertex(Vertex(62.0f, 26.0f));
	List.SetPrim(Primitive(EGSPrimitive::LineStrip));
	List.SetRGBAQ(Color(0xff, 0xff, 0xff));
	const float StripPoints[4][2] = {{1.0f, 17.0f}, {12.6f, 20.2f}, {14.1f, 30.7f}, {29.5f, 31.2f}};
	for (const auto& Point : StripPoints)
	{
		List.AddVertex(Vertex(Point[0], Point[1]));
	}
	List.SetPrim(Primitive(EGSPrimitive::Point));
	List.SetRGBAQ(Color(0xff, 0, 0xff));
	const float Points[4][2] = {{20.5f, 18.5f}, {24.2f, 20.7f}, {22.49f, 26.51f}, {27.0f, 24.0f}};
	for (const auto& Point : Points)
	{
		List.AddVertex(Vertex(Point[0], Point[1]));
	}
}

void GSConformance::BuildBlendEquation(FGSCommandList& List)
{
	SetUpScene(List);
	// Four destinations, a row of 8 pixels each (their alpha is the frame's: Ad).
	const FGSRGBAQ Destinations[4] = {
		Color(40, 120, 200, 0x40), Color(220, 160, 30, 0x80), Color(100, 100, 100, 0xff), Color(10, 250, 128, 0x00)};
	for (int32 Row = 0; Row < 4; ++Row)
	{
		AddSprite(List, 0, float(Row * 8), 64, float(Row * 8) + 8, Destinations[Row]);
	}
	// Sixteen columns of 4 pixels, each a blend of (180, 60, 90) over the four destinations.
	struct FBlendColumn
	{
		EGSBlendColor A;
		EGSBlendColor B;
		EGSBlendAlpha C;
		EGSBlendColor D;
		uint8 FIX;
		uint8 SourceAlpha;
		bool bClamp;
	};
	constexpr EGSBlendColor Cs = EGSBlendColor::Source;
	constexpr EGSBlendColor Cd = EGSBlendColor::Destination;
	constexpr EGSBlendColor Zero = EGSBlendColor::Zero;
	constexpr EGSBlendAlpha As = EGSBlendAlpha::Source;
	constexpr EGSBlendAlpha Ad = EGSBlendAlpha::Destination;
	constexpr EGSBlendAlpha Fix = EGSBlendAlpha::Fixed;
	static constexpr FBlendColumn Columns[16] = {
		{Cs, Cd, As, Cd, 0, 0x60, true}, // the usual translucency
		{Cs, Zero, As, Cd, 0, 0x60, true}, // additive
		{Cd, Cs, Fix, Zero, 0x80, 0x60, true}, // Cd - Cs, clamped at 0
		{Cd, Zero, As, Cd, 0, 0x60, true}, // Cd x (0x80 + As)
		{Zero, Cd, As, Cd, 0, 0x60, true}, // Cd x (0x80 - As)
		{Cs, Cd, Ad, Cd, 0, 0x60, true}, // by the destination's alpha
		{Cs, Cd, Fix, Cd, 0xff, 0x60, true}, // a factor above 1.0 overshoots Cs
		{Cs, Cd, As, Cd, 0, 0xc0, true}, // a source alpha above 1.0
		{Cs, Zero, Fix, Zero, 0x40, 0x60, true},
		{Cd, Cs, As, Cs, 0, 0x60, true},
		{Zero, Cs, Fix, Cd, 0x80, 0x60, true}, // Cd - Cs again
		{Cs, Cs, As, Cd, 0, 0x60, true}, // nothing but Cd
		{Cd, Cd, As, Cs, 0, 0x60, true}, // nothing but Cs
		{Cs, Cd, Ad, Zero, 0, 0x60, true},
		{Cd, Cs, Fix, Zero, 0x80, 0x60, false}, // Cd - Cs wrapped (COLCLAMP off)
		{Cs, Zero, As, Cd, 0, 0xff, false}, // an overflow wrapped
	};
	for (int32 Column = 0; Column < 16; ++Column)
	{
		const FBlendColumn& Blend = Columns[Column];
		FGSAlpha Alpha;
		Alpha.A = Blend.A;
		Alpha.B = Blend.B;
		Alpha.C = Blend.C;
		Alpha.D = Blend.D;
		Alpha.FIX = Blend.FIX;
		List.SetAlpha(0, Alpha);
		List.SetColorClamp(Blend.bClamp);
		AddSprite(List, float(Column * 4), 0, float(Column * 4) + 4, 32, Color(180, 60, 90, Blend.SourceAlpha),
			Blended(EGSPrimitive::Sprite));
	}
	List.SetColorClamp(true);
}

void GSConformance::BuildPabeFbaDate(FGSCommandList& List)
{
	SetUpScene(List);
	// Columns 0-15: PABE blends only a source alpha with its MSB set (0x7f, 0x80, 0xff, 0x00), additive in rows 0-15
	// and by FIX 0x40 toward the destination in rows 16-31.
	AddSprite(List, 0, 0, 16, 32, Color(100, 100, 100, 0x80));
	List.SetPixelAlphaBlend(true);
	static constexpr uint8 SourceAlphas[4] = {0x7f, 0x80, 0xff, 0x00};
	FGSAlpha ByFix = FGSAlpha::Translucent();
	ByFix.C = EGSBlendAlpha::Fixed;
	ByFix.FIX = 0x40;
	for (int32 Column = 0; Column < 4; ++Column)
	{
		const float Left = float(Column * 4);
		List.SetAlpha(0, FGSAlpha::Additive());
		AddSprite(List, Left, 0, Left + 4, 16, Color(60, 90, 30, SourceAlphas[Column]), Blended(EGSPrimitive::Sprite));
		List.SetAlpha(0, ByFix);
		AddSprite(
			List, Left, 16, Left + 4, 32, Color(250, 10, 90, SourceAlphas[Column]), Blended(EGSPrimitive::Sprite));
	}
	List.SetPixelAlphaBlend(false);

	// Columns 16-23: FBA sets the written alpha's MSB (0x10 -> 0x90, 0x90 stays, a blended 0x40 -> 0xc0); off below.
	List.SetFba(0, true);
	AddSprite(List, 16, 0, 24, 8, Color(30, 60, 90, 0x10));
	AddSprite(List, 16, 8, 24, 16, Color(30, 60, 90, 0x90));
	List.SetAlpha(0, FGSAlpha::Translucent());
	AddSprite(List, 16, 16, 24, 24, Color(230, 60, 90, 0x40), Blended(EGSPrimitive::Sprite));
	List.SetFba(0, false);
	AddSprite(List, 16, 24, 24, 32, Color(30, 60, 90, 0x10));

	// Columns 24-39: the destination alpha test over alphas 0x00, 0x80, 0x7f, 0xff: DATM 0 passes bit 7 clear (red,
	// rows 0-15), DATM 1 bit 7 set (green, rows 16-31).
	static constexpr uint8 DestinationAlphas[4] = {0x00, 0x80, 0x7f, 0xff};
	for (int32 Column = 0; Column < 4; ++Column)
	{
		const float Left = 24.0f + float(Column * 4);
		AddSprite(List, Left, 0, Left + 4, 32, Color(50, 50, 50, DestinationAlphas[Column]));
	}
	FGSTest DestinationAlpha;
	DestinationAlpha.bDestinationAlphaTest = true;
	DestinationAlpha.bDestinationAlphaOne = false;
	List.SetTest(0, DestinationAlpha);
	AddSprite(List, 24, 0, 40, 16, Color(0xff, 0, 0, 0x33));
	DestinationAlpha.bDestinationAlphaOne = true;
	List.SetTest(0, DestinationAlpha);
	AddSprite(List, 24, 16, 40, 32, Color(0, 0xff, 0, 0x44));
	List.SetTest(0, FGSTest());

	// Columns 40-63: FBMSK bit by bit: 0x0f0ff00f over a plain write, 0x000000ff over a blend, 0x80000000 over alpha.
	AddSprite(List, 40, 0, 64, 32, Color(0x5a, 0xa5, 0x3c, 0x81));
	FGSFrame Masked = MakeFrame();
	Masked.FBMSK = 0x0f0ff00fu;
	List.SetFrame(0, Masked);
	AddSprite(List, 40, 0, 48, 32, Color(0xff, 0x00, 0xff, 0x00));
	Masked.FBMSK = 0x000000ffu;
	List.SetFrame(0, Masked);
	AddSprite(List, 48, 0, 56, 32, Color(0xff, 0x00, 0xff, 0x40), Blended(EGSPrimitive::Sprite));
	Masked.FBMSK = 0x80000000u;
	List.SetFrame(0, Masked);
	AddSprite(List, 56, 0, 64, 32, Color(0x11, 0x22, 0x33, 0x00));
	List.SetFrame(0, MakeFrame());
}

void GSConformance::BuildDither16Blend(FGSCommandList& List)
{
	SetUpScene(List, EGSPixelFormat::PSMCT16S);
	List.SetDimx(FGSDimx::Default());
	List.SetDither(true);
	// A red ramp from x 0 to 63 over the whole frame, dithered; its alpha bit set but for rows 24-27 right of x 32.
	FGSPrim Ramp = Primitive(EGSPrimitive::TriangleStrip);
	Ramp.bGouraud = true;
	List.SetPrim(Ramp);
	const float RampCorners[4][3] = {{0, 0, 0}, {63, 0, 255}, {0, 32, 0}, {63, 32, 255}};
	for (const auto& Corner : RampCorners)
	{
		List.SetRGBAQ(Color(uint8(Corner[2]), 100, 200, 0x80));
		List.AddVertex(Vertex(Corner[0], Corner[1]));
	}
	AddSprite(List, 32, 24, 64, 28, Color(90, 140, 20, 0x00));

	// Rows 8-15: translucent, dithered after the blend against the 16-bit destination; rows 16-19 without DTHE.
	List.SetAlpha(0, FGSAlpha::Translucent());
	AddSprite(List, 0, 8, 64, 16, Color(250, 20, 120, 0x50), Blended(EGSPrimitive::Sprite));
	List.SetDither(false);
	AddSprite(List, 0, 16, 64, 20, Color(250, 20, 120, 0x50), Blended(EGSPrimitive::Sprite));
	List.SetDither(true);
	// Rows 20-23: additive with COLCLAMP off: what passes 255 wraps, after the dither.
	List.SetAlpha(0, FGSAlpha::Additive());
	List.SetColorClamp(false);
	AddSprite(List, 0, 20, 64, 24, Color(200, 200, 10, 0x80), Blended(EGSPrimitive::Sprite));
	List.SetColorClamp(true);
	// Rows 24-27: Cs x Ad + Cd, Ad the 16-bit alpha bit (0x80 or 0).
	FGSAlpha ByDestination;
	ByDestination.A = EGSBlendColor::Source;
	ByDestination.B = EGSBlendColor::Zero;
	ByDestination.C = EGSBlendAlpha::Destination;
	ByDestination.D = EGSBlendColor::Destination;
	List.SetAlpha(0, ByDestination);
	AddSprite(List, 0, 24, 64, 28, Color(60, 30, 20, 0x10), Blended(EGSPrimitive::Sprite));
	// Rows 28-31: FBMSK keeps the top 3 of red's and blue's 5 bits, and the alpha bit.
	FGSFrame Masked = MakeFrame(EGSPixelFormat::PSMCT16S);
	Masked.FBMSK = 0x80e000e0u;
	List.SetFrame(0, Masked);
	AddSprite(List, 0, 28, 64, 32, Color(0x1f, 0xff, 0x07, 0x00));
	List.SetFrame(0, MakeFrame(EGSPixelFormat::PSMCT16S));
}

void GSConformance::BuildClutLoads(FGSCommandList& List)
{
	SetUpScene(List);
	const auto UploadClut = [&List](uint16 CBP, uint32 NumEntries, uint32 (*EntryColor)(uint32))
	{
		TArray<uint32> Palette;
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			Palette.Add(EntryColor(Index) | 0xff000000u);
		}
		TArray<uint8> Image;
		uint16 Width = 0;
		uint16 Height = 0;
		FGSTextureLayout::MakeClutImage(Palette, Image, Width, Height);
		FGSBitBltBuf Destination;
		Destination.DBP = CBP;
		Destination.DBW = 1;
		Destination.DPSM = EGSPixelFormat::PSMCT32;
		List.UploadImage(Destination, 0, 0, Width, Height, Image);
		List.TexFlush();
	};
	// CLUT A (reds) at 128, B (greens) at 132; C (blues) replaces B later.
	UploadClut(128, 256, [](uint32 Index) { return Index | (0x20u << 16); });
	UploadClut(132, 256, [](uint32 Index) { return 0x20u | (Index << 8); });
	// A 16 x 2 PSMT8 texture whose texel x is entry 16 x.
	TArray<uint8> Indices;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (uint32 X = 0; X < 16; ++X)
		{
			Indices.Add(uint8(X * 16));
		}
	}
	FGSBitBltBuf Buf8;
	Buf8.DBP = 192;
	Buf8.DBW = 2;
	Buf8.DPSM = EGSPixelFormat::PSMT8;
	List.UploadImage(Buf8, 0, 0, 16, 2, Indices);
	List.TexFlush();
	List.SetTex1(0, FGSTex1());

	FGSTex0 Tex8;
	Tex8.TBP0 = 192;
	Tex8.TBW = 2;
	Tex8.PSM = EGSPixelFormat::PSMT8;
	Tex8.TW = 4;
	Tex8.TH = 1;
	Tex8.TFX = EGSTextureFunction::Decal;
	FGSPrim Textured = Primitive(EGSPrimitive::Sprite);
	Textured.bTextured = true;
	Textured.bUseUV = true;
	const auto SetClut = [&List, &Tex8](uint16 CBP, uint8 CLD)
	{
		FGSTex0 Tex0 = Tex8;
		Tex0.CBP = CBP;
		Tex0.CLD = CLD;
		List.SetTex0(0, Tex0);
	};
	const auto DrawRow = [&List, &Textured](float Y, float Height)
	{
		List.SetPrim(Textured);
		List.SetUV(FGSUV{0, GSToFixed4(0.5f, 14)});
		List.AddVertex(Vertex(0, Y));
		List.SetUV(FGSUV{GSToFixed4(16, 14), GSToFixed4(0.5f, 14)});
		List.AddVertex(Vertex(64, Y + Height));
	};
	// Rows of 3 pixels, each after the TEX0 writes named (manual 3.4.7):
	// 0: CLD 2 at A: loads A, CBP0 = A (red).
	SetClut(128, 2);
	DrawRow(0, 3);
	// 1: CLD 4 at B: CBP0 differs, loads B, CBP0 = B (green).
	SetClut(132, 4);
	DrawRow(3, 3);
	// 2: C uploaded over B, then CLD 4 at B: CBP0 is B, no load: still green.
	UploadClut(132, 256, [](uint32 Index) { return (0x20u << 8) | (Index << 16); });
	SetClut(132, 4);
	DrawRow(6, 3);
	// 3: CLD 1 at B: loads (blue).
	SetClut(132, 1);
	DrawRow(9, 3);
	// 4: CLD 0 at A: no load: still blue.
	SetClut(128, 0);
	DrawRow(12, 3);
	// 5: CLD 3 at A: loads A, CBP1 = A (red).
	SetClut(128, 3);
	DrawRow(15, 3);
	// 6: CLD 1 at B (blue), then CLD 5 at A: CBP1 is A, no load: blue.
	SetClut(132, 1);
	SetClut(128, 5);
	DrawRow(18, 3);
	// 7: CLD 3 at B (CBP1 = B), then CLD 5 at A: CBP1 differs, loads A (red).
	SetClut(132, 3);
	SetClut(128, 5);
	DrawRow(21, 3);

	// Rows 24-31: two PSMT4 CLUTs side by side in the temporary buffer (CSA 0 and 1), loaded first and drawn with
	// CLD 0.
	UploadClut(136, 16, [](uint32 Index) { return (Index * 16) | (0x80u << 8); });
	UploadClut(137, 16, [](uint32 Index) { return ((Index * 16) << 8) | (0x80u << 16); });
	TArray<uint8> Indices4;
	for (uint32 Row = 0; Row < 2; ++Row)
	{
		for (uint32 X = 0; X < 16; X += 2)
		{
			Indices4.Add(uint8(X | ((X + 1) << 4)));
		}
	}
	FGSBitBltBuf Buf4;
	Buf4.DBP = 224;
	Buf4.DBW = 2;
	Buf4.DPSM = EGSPixelFormat::PSMT4;
	List.UploadImage(Buf4, 0, 0, 16, 2, Indices4);
	List.TexFlush();
	FGSTex0 Tex4 = Tex8;
	Tex4.TBP0 = 224;
	Tex4.PSM = EGSPixelFormat::PSMT4;
	Tex4.CBP = 136;
	Tex4.CSA = 0;
	Tex4.CLD = 1;
	List.SetTex0(0, Tex4);
	Tex4.CBP = 137;
	Tex4.CSA = 1;
	List.SetTex0(0, Tex4);
	Tex4.CLD = 0;
	Tex4.CSA = 0;
	List.SetTex0(0, Tex4);
	DrawRow(24, 4);
	Tex4.CSA = 1;
	List.SetTex0(0, Tex4);
	DrawRow(28, 4);
}

TArrayView<const FGSConformanceScene> GSConformance::GetScenes()
{
	static const FGSConformanceScene Scenes[] = {
		{"DrawingRules", EGSPixelFormat::PSMCT32, &BuildDrawingRules},
		{"Primitives", EGSPixelFormat::PSMCT32, &BuildPrimitives},
		{"GouraudAndScissor", EGSPixelFormat::PSMCT32, &BuildGouraudAndScissor},
		{"TextureSampling", EGSPixelFormat::PSMCT32, &BuildTextureSampling},
		{"ClutAndFormats", EGSPixelFormat::PSMCT32, &BuildClutAndFormats},
		{"TwoPalettes", EGSPixelFormat::PSMCT32, &BuildTwoPalettes},
		{"FunctionsFogAndMipmap", EGSPixelFormat::PSMCT32, &BuildFunctionsFogAndMipmap},
		{"PixelTests", EGSPixelFormat::PSMCT32, &BuildPixelTests},
		{"BlendAndWrite", EGSPixelFormat::PSMCT32, &BuildBlendAndWrite},
		{"Dither16", EGSPixelFormat::PSMCT16S, &BuildDither16},
		{"MipmapLod", EGSPixelFormat::PSMCT32, &BuildMipmapLod},
		{"AlphaTest", EGSPixelFormat::PSMCT32, &BuildAlphaTest},
		{"Fog", EGSPixelFormat::PSMCT32, &BuildFog},
		{"TexAAndFunctions", EGSPixelFormat::PSMCT32, &BuildTexAAndFunctions},
		{"ClampModes", EGSPixelFormat::PSMCT32, &BuildClampModes},
		{"StripsAndSprites", EGSPixelFormat::PSMCT32, &BuildStripsAndSprites},
		{"BlendEquation", EGSPixelFormat::PSMCT32, &BuildBlendEquation},
		{"PabeFbaDate", EGSPixelFormat::PSMCT32, &BuildPabeFbaDate},
		{"Dither16Blend", EGSPixelFormat::PSMCT16S, &BuildDither16Blend},
		{"ClutLoads", EGSPixelFormat::PSMCT32, &BuildClutLoads},
	};
	return MakeArrayView(Scenes, UE_ARRAY_COUNT(Scenes));
}
