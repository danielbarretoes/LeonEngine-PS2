#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The GS contract (Docs/PLANS/ps2-gs-parity.md, P1): the register addresses and encodings of the GS User's Manual
// (chapter 7), and the command list every backend consumes. The expected values are the manual's bit positions applied
// by hand, not the encoder's own arithmetic.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreRegisterAddressesTest, "System.GSCore.Registers.Addresses",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreRegisterAddressesTest::RunTest(const FString& Parameters)
{
	// The manual's general purpose register table (7.1).
	TestEqual("PRIM", uint8(EGSRegister::PRIM), uint8(0x00));
	TestEqual("XYZ2", uint8(EGSRegister::XYZ2), uint8(0x05));
	TestEqual("TEX0_2", uint8(EGSRegister::TEX0_2), uint8(0x07));
	TestEqual("XYZ3", uint8(EGSRegister::XYZ3), uint8(0x0d));
	TestEqual("TEX1_1", uint8(EGSRegister::TEX1_1), uint8(0x14));
	TestEqual("TEXCLUT", uint8(EGSRegister::TEXCLUT), uint8(0x1c));
	TestEqual("FOGCOL", uint8(EGSRegister::FOGCOL), uint8(0x3d));
	TestEqual("ALPHA_2", uint8(EGSRegister::ALPHA_2), uint8(0x43));
	TestEqual("TEST_1", uint8(EGSRegister::TEST_1), uint8(0x47));
	TestEqual("ZBUF_2", uint8(EGSRegister::ZBUF_2), uint8(0x4f));
	TestEqual("HWREG", uint8(EGSRegister::HWREG), uint8(0x54));
	TestTrue("A context pair", FGSCommandList::ContextRegister(EGSRegister::SCISSOR_1, 1) == EGSRegister::SCISSOR_2);
	TestTrue("Context 0", FGSCommandList::ContextRegister(EGSRegister::FRAME_1, 0) == EGSRegister::FRAME_1);

	// The pixel storage formats (TEX0.PSM) and their memory widths.
	TestEqual("PSMCT16S", uint8(EGSPixelFormat::PSMCT16S), uint8(0x0a));
	TestEqual("PSMT8", uint8(EGSPixelFormat::PSMT8), uint8(0x13));
	TestEqual("PSMT4", uint8(EGSPixelFormat::PSMT4), uint8(0x14));
	TestEqual("PSMZ24", uint8(EGSPixelFormat::PSMZ24), uint8(0x31));
	TestEqual("4 bits", GSBitsPerPixel(EGSPixelFormat::PSMT4), 4u);
	TestEqual("16 bits", GSBitsPerPixel(EGSPixelFormat::PSMCT16S), 16u);
	TestEqual("32 bits", GSBitsPerPixel(EGSPixelFormat::PSMCT32), 32u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreRegisterEncodingTest, "System.GSCore.Registers.Encoding",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreRegisterEncodingTest::RunTest(const FString& Parameters)
{
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::TriangleStrip;
	Prim.bGouraud = true;
	Prim.bTextured = true;
	TestEqual("PRIM", Prim.Encode(), uint64(0x1c));
	Prim.Context = 1;
	Prim.bUseUV = true;
	TestEqual("PRIM CTXT and FST", Prim.Encode(), uint64(0x31c));

	FGSRGBAQ Color;
	Color.R = 0x11;
	Color.G = 0x22;
	Color.B = 0x33;
	Color.A = 0x80;
	Color.Q = 1.0f;
	TestEqual("RGBAQ", Color.Encode(), uint64(0x3f80000080332211ull));

	FGSXYZ Vertex;
	Vertex.X = GSToFixed4(2048.0f, 16);
	Vertex.Y = GSToFixed4(2040.0f, 16);
	Vertex.Z = 0x12345678;
	TestEqual("XYZ2", Vertex.Encode(), uint64(0x123456787f808000ull));

	FGSXYZF VertexF;
	VertexF.X = 1;
	VertexF.Y = 2;
	VertexF.Z = 0xabcdef;
	VertexF.F = 0x7f;
	TestEqual("XYZF2", VertexF.Encode(), uint64(0x7fabcdef00020001ull));

	FGSUV UV;
	UV.U = GSToFixed4(16.5f, 14);
	UV.V = GSToFixed4(3.0f, 14);
	TestEqual("UV", UV.Encode(), uint64(0x300108));

	FGSTex0 Tex0;
	Tex0.TBP0 = 0x1234;
	Tex0.TBW = 4;
	Tex0.PSM = EGSPixelFormat::PSMT8;
	Tex0.TW = 8;
	Tex0.TH = 7;
	Tex0.bRGBA = true;
	Tex0.TFX = EGSTextureFunction::Decal;
	Tex0.CBP = 0x100;
	Tex0.CPSM = EGSPixelFormat::PSMCT16;
	Tex0.CLD = 1;
	TestEqual("TEX0", Tex0.Encode(), uint64(0x2010200de1311234ull));

	FGSTex1 Tex1;
	Tex1.MXL = 3;
	Tex1.MMAG = EGSFilter::Linear;
	Tex1.MMIN = EGSFilter::LinearMipmapLinear;
	Tex1.L = 1;
	Tex1.K = -16;
	TestEqual("TEX1 (K = -1.0 in 7.4)", Tex1.Encode(), uint64(0xff00008016cull));

	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	Clamp.WMT = EGSWrapMode::RegionRepeat;
	Clamp.MINU = 0x3ff;
	Clamp.MAXU = 0x155;
	Clamp.MINV = 0x2aa;
	Clamp.MAXV = 0x3ff;
	TestEqual("CLAMP", Clamp.Encode(), uint64(0xffeaa557ffdull));

	FGSAlpha Additive = FGSAlpha::Additive();
	Additive.C = EGSBlendAlpha::Source;
	Additive.FIX = 0x40;
	TestEqual("ALPHA (Cs - 0) * As + Cd", Additive.Encode(), uint64(0x4000000048ull));

	FGSTest Test;
	Test.bAlphaTest = true;
	Test.ATST = EGSAlphaTest::GreaterEqual;
	Test.AREF = 0x80;
	Test.ZTST = EGSDepthTest::GreaterEqual;
	TestEqual("TEST", Test.Encode(), uint64(0x5080b));

	FGSZBuf ZBuf;
	ZBuf.ZBP = 0x46;
	ZBuf.PSM = EGSPixelFormat::PSMZ24;
	ZBuf.bMask = true;
	TestEqual("ZBUF (the PSM field is the Z format's low 4 bits)", ZBuf.Encode(), uint64(0x101000046ull));

	FGSFrame Frame;
	Frame.FBW = 10;
	Frame.PSM = EGSPixelFormat::PSMCT16S;
	Frame.FBMSK = 0xff000000;
	TestEqual("FRAME", Frame.Encode(), uint64(0xff0000000a0a0000ull));

	FGSScissor Scissor;
	Scissor.SCAX1 = 639;
	Scissor.SCAY1 = 447;
	TestEqual("SCISSOR", Scissor.Encode(), uint64(0x1bf0000027f0000ull));

	FGSXYOffset Offset;
	Offset.OFX = GSToFixed4(2048.0f - 320.0f, 16);
	Offset.OFY = GSToFixed4(2048.0f - 224.0f, 16);
	TestEqual("XYOFFSET", Offset.Encode(), uint64(0x720000006c00ull));

	TestEqual("DIMX (the manual's example matrix)", FGSDimx::Default().Encode(), uint64(0x6071243571603524ull));

	FGSBitBltBuf Buf;
	Buf.DBP = 0x1000;
	Buf.DBW = 2;
	Buf.DPSM = EGSPixelFormat::PSMT8;
	TestEqual("BITBLTBUF", Buf.Encode(), uint64(0x1302100000000000ull));

	FGSMipTbp MipTbp;
	MipTbp.TBP[0] = 0x100;
	MipTbp.TBW[0] = 2;
	MipTbp.TBP[1] = 0x200;
	MipTbp.TBW[1] = 1;
	MipTbp.TBP[2] = 0x3fff;
	MipTbp.TBW[2] = 0x3f;
	TestEqual("MIPTBP", MipTbp.Encode(), uint64(0xfffff0420008100ull));

	FGSTexA TexA;
	TexA.bAlphaExpandBlack = true;
	TexA.TA1 = 0x80;
	TestEqual("TEXA", TexA.Encode(), uint64(0x8000008000ull));

	FGSFog Fog;
	Fog.F = 0x40;
	TestEqual("FOG", Fog.Encode(), uint64(0x4000000000000000ull));
	FGSFogCol FogCol;
	FogCol.R = 0x10;
	FogCol.G = 0x20;
	FogCol.B = 0x30;
	TestEqual("FOGCOL", FogCol.Encode(), uint64(0x302010));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreRegisterRoundTripTest, "System.GSCore.Registers.RoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreRegisterRoundTripTest::RunTest(const FString& Parameters)
{
	// Decode(Encode(x)) keeps every field, signed ones included; the fixed point conversion rounds and clamps.
	FGSTex1 Tex1;
	Tex1.bFixedLOD = true;
	Tex1.MMIN = EGSFilter::NearestMipmapLinear;
	Tex1.K = -2047;
	const FGSTex1 Tex1Back = FGSTex1::Decode(Tex1.Encode());
	TestTrue("TEX1", Tex1Back.bFixedLOD && Tex1Back.MMIN == EGSFilter::NearestMipmapLinear && Tex1Back.K == -2047);

	FGSTex0 Tex0;
	Tex0.PSM = EGSPixelFormat::PSMT4;
	Tex0.CPSM = EGSPixelFormat::PSMCT16;
	Tex0.CSA = 31;
	Tex0.CBP = 0x3fff;
	Tex0.TFX = EGSTextureFunction::Highlight2;
	const FGSTex0 Tex0Back = FGSTex0::Decode(Tex0.Encode());
	TestTrue("TEX0",
		Tex0Back.PSM == EGSPixelFormat::PSMT4 && Tex0Back.CPSM == EGSPixelFormat::PSMCT16 && Tex0Back.CSA == 31 &&
			Tex0Back.CBP == 0x3fff && Tex0Back.TFX == EGSTextureFunction::Highlight2);

	FGSRGBAQ Color;
	Color.Q = 0.25f;
	Color.A = 0xff;
	const FGSRGBAQ ColorBack = FGSRGBAQ::Decode(Color.Encode());
	TestTrue("RGBAQ", ColorBack.Q == 0.25f && ColorBack.A == 0xff);

	FGSST ST;
	ST.S = -1.5f;
	ST.T = 3.75f;
	const FGSST STBack = FGSST::Decode(ST.Encode());
	TestTrue("ST", STBack.S == -1.5f && STBack.T == 3.75f);

	const FGSDimx Dimx = FGSDimx::Decode(FGSDimx::Default().Encode());
	TestTrue("DIMX", FMemory::Memcmp(Dimx.M, FGSDimx::Default().M, sizeof(Dimx.M)) == 0);

	const FGSZBuf ZBuf = FGSZBuf::Decode(FGSZBuf().Encode());
	TestTrue("ZBUF's format", ZBuf.PSM == EGSPixelFormat::PSMZ24);

	FGSAlpha Alpha;
	Alpha.C = EGSBlendAlpha::Fixed;
	Alpha.FIX = 0x33;
	const FGSAlpha AlphaBack = FGSAlpha::Decode(Alpha.Encode());
	TestTrue("ALPHA",
		AlphaBack.C == EGSBlendAlpha::Fixed && AlphaBack.FIX == 0x33 && AlphaBack.B == EGSBlendColor::Destination);

	TestEqual("12.4", GSToFixed4(2048.5f, 16), uint16(32776));
	TestEqual("Rounded", GSToFixed4(1.03f, 16), uint16(16));
	TestEqual("Clamped below", GSToFixed4(-1.0f, 16), uint16(0));
	TestEqual("Clamped above", GSToFixed4(5000.0f, 16), uint16(0xffff));
	TestEqual("10.4 clamps at 14 bits", GSToFixed4(1024.0f, 14), uint16(0x3fff));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreCommandListTest, "System.GSCore.CommandList.RecordsWrites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreCommandListTest::RunTest(const FString& Parameters)
{
	// The writes come out in order, at their context's address, and an upload is BITBLTBUF, TRXPOS, TRXREG,
	// TRXDIR and HWREG with its pixels.
	FGSCommandList List;
	List.SetPrimModeFromPrim();
	List.SetAlpha(1, FGSAlpha::Additive());
	FGSTex0 Tex0;
	Tex0.PSM = EGSPixelFormat::PSMT8;
	List.SetTex0(0, Tex0);
	FGSPrim Prim;
	List.SetPrim(Prim);
	FGSXYZ Vertex;
	Vertex.X = 0x8000;
	List.AddVertexNoKick(Vertex);
	List.AddVertex(Vertex);

	const TArray<FGSRegisterWrite>& Writes = List.GetWrites();
	if (!TestEqual("Six writes", Writes.Num(), 6))
	{
		return false;
	}
	TestTrue("PRMODECONT.AC", Writes[0].Register == EGSRegister::PRMODECONT && Writes[0].Value == 1);
	TestTrue("ALPHA_2", Writes[1].Register == EGSRegister::ALPHA_2 && Writes[1].Value == FGSAlpha::Additive().Encode());
	TestTrue("TEX0_1", Writes[2].Register == EGSRegister::TEX0_1 && Writes[2].Value == Tex0.Encode());
	TestTrue("PRIM", Writes[3].Register == EGSRegister::PRIM);
	TestTrue("No kick", Writes[4].Register == EGSRegister::XYZ3);
	TestTrue("Kick", Writes[5].Register == EGSRegister::XYZ2 && Writes[5].Value == Vertex.Encode());

	// 16 x 2 PSMT8 texels: 32 bytes, two quadwords.
	TArray<uint8> Texels;
	for (int32 Index = 0; Index < 32; ++Index)
	{
		Texels.Add(uint8(Index));
	}
	FGSBitBltBuf Destination;
	Destination.DBP = 0x200;
	Destination.DPSM = EGSPixelFormat::PSMT8;
	List.UploadImage(Destination, 4, 8, 16, 2, Texels);
	TestEqual("Upload writes", Writes.Num(), 11);
	TestTrue("BITBLTBUF", Writes[6].Register == EGSRegister::BITBLTBUF && Writes[6].Value == Destination.Encode());
	const FGSTrxPos Position = FGSTrxPos::Decode(Writes[7].Value);
	TestTrue("TRXPOS", Writes[7].Register == EGSRegister::TRXPOS && Position.DSAX == 4 && Position.DSAY == 8);
	const FGSTrxReg Region = FGSTrxReg::Decode(Writes[8].Value);
	TestTrue("TRXREG", Writes[8].Register == EGSRegister::TRXREG && Region.RRW == 16 && Region.RRH == 2);
	TestTrue("TRXDIR host to local", Writes[9].Register == EGSRegister::TRXDIR && Writes[9].Value == 0);
	TestTrue("HWREG names the data", Writes[10].Register == EGSRegister::HWREG && Writes[10].Value == 0);
	TestTrue("The pixels", List.GetImageData().Num() == 1 && List.GetImageData()[0] == Texels);

	List.Reset();
	TestTrue("Reset", List.GetWrites().Num() == 0 && List.GetImageData().Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreSupportedSubsetTest, "System.GSCore.CommandList.SupportedSubset",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreSupportedSubsetTest::RunTest(const FString& Parameters)
{
	// What every backend reproduces exactly (the plan's D5): the blends a GL blend equation expresses, the texture
	// formats the preview samples, and the rest refused.
	TestTrue("Translucent", FGSCommandList::IsSupported(FGSAlpha::Translucent()));
	TestTrue("Additive", FGSCommandList::IsSupported(FGSAlpha::Additive()));
	FGSAlpha Fixed = FGSAlpha::Translucent();
	Fixed.C = EGSBlendAlpha::Fixed;
	TestTrue("Constant alpha", FGSCommandList::IsSupported(Fixed));
	FGSAlpha Scaled = FGSAlpha::Additive();
	Scaled.D = EGSBlendColor::Zero;
	TestTrue("Source scaled", FGSCommandList::IsSupported(Scaled));
	FGSAlpha Subtract;
	Subtract.A = EGSBlendColor::Destination;
	Subtract.B = EGSBlendColor::Source;
	TestFalse("Subtractive", FGSCommandList::IsSupported(Subtract));
	FGSAlpha DestinationAlpha = FGSAlpha::Translucent();
	DestinationAlpha.C = EGSBlendAlpha::Destination;
	TestFalse("Destination alpha as the factor", FGSCommandList::IsSupported(DestinationAlpha));
	FGSAlpha LerpToZero = FGSAlpha::Translucent();
	LerpToZero.D = EGSBlendColor::Zero;
	TestFalse("(Cs - Cd) * As + 0", FGSCommandList::IsSupported(LerpToZero));

	FGSPrim Prim;
	TestTrue("A triangle", FGSCommandList::IsSupported(Prim));
	Prim.bAntialias = true;
	TestFalse("AA1", FGSCommandList::IsSupported(Prim));

	FGSTex0 Tex0;
	Tex0.PSM = EGSPixelFormat::PSMT4;
	TestTrue("PSMT4 with a PSMCT32 CLUT", FGSCommandList::IsSupported(Tex0));
	Tex0.bCSM2 = true;
	TestFalse("CSM2", FGSCommandList::IsSupported(Tex0));
	Tex0.bCSM2 = false;
	Tex0.CPSM = EGSPixelFormat::PSMCT16S;
	TestFalse("A PSMCT16S CLUT", FGSCommandList::IsSupported(Tex0));
	Tex0.PSM = EGSPixelFormat::PSMT8H;
	TestFalse("PSMT8H (in a frame buffer's alpha)", FGSCommandList::IsSupported(Tex0));
	Tex0.PSM = EGSPixelFormat::PSMCT32;
	Tex0.TW = 11;
	TestFalse("Wider than 1024", FGSCommandList::IsSupported(Tex0));

	FGSTest Test;
	TestTrue("Depth test on", FGSCommandList::IsSupported(Test));
	Test.bDepthTest = false;
	TestFalse("Depth test off", FGSCommandList::IsSupported(Test));

	FGSFrame Frame;
	Frame.PSM = EGSPixelFormat::PSMCT16S;
	TestTrue("A 16-bit frame buffer", FGSCommandList::IsSupported(Frame));
	Frame.PSM = EGSPixelFormat::PSMZ24;
	TestFalse("A Z format as the frame", FGSCommandList::IsSupported(Frame));
	FGSZBuf ZBuf;
	TestTrue("PSMZ24", FGSCommandList::IsSupported(ZBuf));
	ZBuf.PSM = EGSPixelFormat::PSMCT32;
	TestFalse("A color format as the Z buffer", FGSCommandList::IsSupported(ZBuf));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
