#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSConformanceScenes.h"
#include "GSGifPacket.h"
#include "GSTextureLayout.h"
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
	TestTrue("The pixels, copied",
		List.GetNumImages() == 1 && List.GetImage(0).Num() == Texels.Num() &&
			FMemory::Memcmp(List.GetImage(0).GetData(), Texels.GetData(), Texels.Num()) == 0 &&
			List.GetImage(0).GetData() != Texels.GetData() && !List.IsImageInPlace(0));

	List.Reset();
	TestTrue("Reset", List.GetWrites().Num() == 0 && List.GetNumImages() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreSupportedSubsetTest, "System.GSCore.CommandList.SupportedSubset",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreSupportedSubsetTest::RunTest(const FString& Parameters)
{
	// What the desktop's emulator reproduces and a GSConformance scene checks (Docs/PLANS/ps2-shipping.md D7): the
	// whole blend equation, every primitive but AA1, the CLUT loads, the MIPMAP filters, the wrap modes; the rest
	// refused.
	TestTrue("Translucent", FGSCommandList::IsSupported(FGSAlpha::Translucent()));
	TestTrue("Additive", FGSCommandList::IsSupported(FGSAlpha::Additive()));
	FGSAlpha Subtract;
	Subtract.A = EGSBlendColor::Destination;
	Subtract.B = EGSBlendColor::Source;
	TestTrue("Subtractive", FGSCommandList::IsSupported(Subtract));
	FGSAlpha DestinationAlpha = FGSAlpha::Translucent();
	DestinationAlpha.C = EGSBlendAlpha::Destination;
	TestTrue("Destination alpha as the factor", FGSCommandList::IsSupported(DestinationAlpha));
	FGSAlpha Reserved = FGSAlpha::Translucent();
	Reserved.C = EGSBlendAlpha(3);
	TestFalse("C = 3 (reserved)", FGSCommandList::IsSupported(Reserved));

	FGSPrim Prim;
	TestTrue("A triangle", FGSCommandList::IsSupported(Prim));
	Prim.Type = EGSPrimitive::LineStrip;
	TestTrue("A line strip", FGSCommandList::IsSupported(Prim));
	Prim.bAntialias = true;
	TestFalse("AA1", FGSCommandList::IsSupported(Prim));
	Prim.bAntialias = false;
	Prim.bFixFragment = true;
	TestFalse("FIX", FGSCommandList::IsSupported(Prim));
	Prim.bFixFragment = false;
	Prim.Type = EGSPrimitive(7);
	TestFalse("PRIM 7 (reserved)", FGSCommandList::IsSupported(Prim));

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
	Tex0.PSM = EGSPixelFormat::PSMCT16S;
	TestTrue("PSMCT16S", FGSCommandList::IsSupported(Tex0));
	Tex0.PSM = EGSPixelFormat::PSMCT32;
	Tex0.TW = 11;
	TestFalse("Wider than 1024", FGSCommandList::IsSupported(Tex0));

	Tex0.TW = 8;
	Tex0.CLD = 5;
	TestTrue("CLD comparing CBP1", FGSCommandList::IsSupported(Tex0));
	Tex0.CLD = 6;
	TestFalse("CLD 6 (reserved)", FGSCommandList::IsSupported(Tex0));
	FGSTex1 Tex1;
	Tex1.MMIN = EGSFilter::LinearMipmapLinear;
	Tex1.MXL = 6;
	TestTrue("Trilinear up to level 6", FGSCommandList::IsSupported(Tex1));
	Tex1.MXL = 7;
	TestFalse("MXL 7", FGSCommandList::IsSupported(Tex1));
	Tex1.MXL = 6;
	Tex1.bAutoMipBase = true;
	TestFalse("MTBA", FGSCommandList::IsSupported(Tex1));

	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::RegionRepeat;
	Clamp.MINU = 0x3f0;
	TestTrue("REGION_REPEAT", FGSCommandList::IsSupported(Clamp));
	Clamp.WMS = EGSWrapMode::RegionClamp;
	TestFalse("REGION_CLAMP with MINU above MAXU", FGSCommandList::IsSupported(Clamp));
	Clamp.MAXU = 0x3ff;
	TestTrue("REGION_CLAMP", FGSCommandList::IsSupported(Clamp));

	// The manual's transfer limits (4.1.5).
	TestTrue("32 bits, even width", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMCT32, 1, 2));
	TestFalse("32 bits, odd width", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMCT32, 0, 3));
	TestFalse("16 bits, width 6", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMCT16, 0, 6));
	TestTrue("8 bits at an even X", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMT8, 2, 8));
	TestFalse("8 bits at an odd X", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMT8, 1, 8));
	TestFalse("4 bits at X 2", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMT4, 2, 8));
	TestFalse("24 bits, width 4", FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMCT24, 0, 4));

	FGSTest Test;
	TestTrue("Depth test on", FGSCommandList::IsSupported(Test));
	Test.bAlphaTest = true;
	Test.AFAIL = EGSAlphaFail::RGBOnly;
	Test.bDestinationAlphaTest = true;
	TestTrue("The alpha tests", FGSCommandList::IsSupported(Test));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreGifPacketTest, "System.GSCore.GifPacket.Layout",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreGifPacketTest::RunTest(const FString& Parameters)
{
	// The GIFtag of the EE User's Manual (7.2): NLOOP bits 0-14, EOP bit 15, FLG bits 58-59, NREG bits 60-63, REGS in
	// the high 64 bits (A+D = 0xe).
	TestEqual("PACKED, 3 loops, EOP, 1 register", FGSGifPacket::MakeTag(3, true, EGSGifFormat::Packed, 1),
		uint64(0x1000000000008003ull));
	TestEqual("IMAGE, 2 loops", FGSGifPacket::MakeTag(2, false, EGSGifFormat::Image, 0), uint64(0x0800000000000002ull));

	// PRIM and a 4 x 2 PSMCT32 upload (BITBLTBUF, TRXPOS, TRXREG, TRXDIR in PACKED, the pixels in IMAGE), then
	// TEXFLUSH: three tags, EOP on the last.
	FGSCommandList List;
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Sprite;
	List.SetPrim(Prim);
	TArray<uint8> Pixels;
	for (uint32 Index = 0; Index < 32; ++Index)
	{
		Pixels.Add(uint8(Index));
	}
	List.UploadImage(FGSBitBltBuf(), 0, 0, 4, 2, Pixels);
	List.TexFlush();
	TArray<uint64> Packet;
	FGSGifPacket::Build(List, false, Packet);
	TestEqual("11 quadwords", Packet.Num(), 22);
	if (Packet.Num() != 22)
	{
		return false;
	}
	TestEqual("PACKED tag, 5 writes", Packet[0], FGSGifPacket::MakeTag(5, false, EGSGifFormat::Packed, 1));
	TestEqual("A+D", Packet[1], uint64(0xe));
	TestEqual("PRIM's value", Packet[2], Prim.Encode());
	TestEqual("PRIM's address", Packet[3], uint64(EGSRegister::PRIM));
	TestEqual("TRXDIR last", Packet[11], uint64(EGSRegister::TRXDIR));
	TestEqual("IMAGE tag, 2 quadwords", Packet[12], FGSGifPacket::MakeTag(2, false, EGSGifFormat::Image, 0));
	TestEqual("The first pixels, little endian", Packet[14], uint64(0x0706050403020100ull));
	TestEqual("The last pixels", Packet[17], uint64(0x1f1e1d1c1b1a1918ull));
	TestEqual("TEXFLUSH's tag has EOP", Packet[18], FGSGifPacket::MakeTag(1, true, EGSGifFormat::Packed, 1));
	TestEqual("TEXFLUSH", Packet[21], uint64(EGSRegister::TEXFLUSH));

	// FINISH ends the packet and takes EOP; an empty list without it is no packet at all.
	TArray<uint64> Finished;
	FGSGifPacket::Build(List, true, Finished);
	TestEqual("One more tag and write", Finished.Num(), 26);
	TestEqual("TEXFLUSH's tag without EOP", Finished[18], FGSGifPacket::MakeTag(1, false, EGSGifFormat::Packed, 1));
	TestEqual("FINISH", Finished[25], uint64(EGSRegister::FINISH));
	TestEqual("With EOP", Finished[22], FGSGifPacket::MakeTag(1, true, EGSGifFormat::Packed, 1));
	TArray<uint64> Empty;
	FGSGifPacket::Build(FGSCommandList(), false, Empty);
	TestEqual("Empty", Empty.Num(), 0);

	// NLOOP holds 0x7fff: a longer run takes a second tag.
	FGSCommandList Long;
	for (uint32 Index = 0; Index < FGSGifPacket::MaxLoops + 1; ++Index)
	{
		Long.SetRGBAQ(FGSRGBAQ());
	}
	TArray<uint64> LongPacket;
	FGSGifPacket::Build(Long, false, LongPacket);
	TestEqual(
		"First tag full", LongPacket[0], FGSGifPacket::MakeTag(FGSGifPacket::MaxLoops, false, EGSGifFormat::Packed, 1));
	TestEqual("Second tag", LongPacket[2 + (FGSGifPacket::MaxLoops * 2)],
		FGSGifPacket::MakeTag(1, true, EGSGifFormat::Packed, 1));
	return true;
}

namespace
{

	/** A DMAtag's fields by the manual's bit positions (EE User's Manual 5.6). */
	[[nodiscard]] uint32 DmaTagQuadwords(uint64 Tag)
	{
		return uint32(Tag & 0xffff);
	}

	[[nodiscard]] uint32 DmaTagId(uint64 Tag)
	{
		return uint32((Tag >> 28) & 7);
	}

	[[nodiscard]] uint32 DmaTagAddress(uint64 Tag)
	{
		return uint32((Tag >> 32) & 0x7fffffff);
	}

	/**
	 * Whether a DMAtag's VIFcodes (its high 64 bits, TTE on) are a NOP or a FLUSH, then a DIRECT of the tag's
	 * quadwords (a NOP for none); bOutFlush says which the first was.
	 */
	[[nodiscard]] bool IsDirectTag(uint64 Codes, uint32 Quadwords, bool& bOutFlush)
	{
		const uint32 First = uint32(Codes);
		const uint32 Second = uint32(Codes >> 32);
		bOutFlush = First == FGSGifPacket::MakeVifCode(EGSVifCommand::Flush);
		const bool bFirst = bOutFlush || First == FGSGifPacket::MakeVifCode(EGSVifCommand::Nop);
		const uint32 Expected = Quadwords > 0 ? FGSGifPacket::MakeVifCode(EGSVifCommand::Direct, 0, Quadwords)
											  : FGSGifPacket::MakeVifCode(EGSVifCommand::Nop);
		return bFirst && Second == Expected;
	}

	/**
	 * What the DMAC and VIF1 send the GIF by PATH2 for Chain (CNT, REF and END tags, TTE on, each with a DIRECT of its
	 * quadwords), the REF'd data found among List's images by address; false on a tag or VIFcode it does not expect or
	 * an address of none of them. OutFlushes counts the tags with a FLUSH.
	 */
	[[nodiscard]] bool FollowChain(const FGSCommandList& List, const uint64* Chain, uint32 NumQuadwords,
		TArray<uint64>& OutSent, int32* OutFlushes = nullptr)
	{
		uint32 Index = 0;
		while (Index < NumQuadwords)
		{
			const uint64 Tag = Chain[Index * 2];
			const uint32 Quadwords = DmaTagQuadwords(Tag);
			const uint32 Id = DmaTagId(Tag);
			bool bFlush = false;
			if (!IsDirectTag(Chain[(Index * 2) + 1], Quadwords, bFlush))
			{
				return false;
			}
			if (OutFlushes != nullptr)
			{
				*OutFlushes += bFlush ? 1 : 0;
			}
			if (Id == uint32(EGSDmaTag::Ref))
			{
				const uint32 Address = DmaTagAddress(Tag);
				const uint8* Data = nullptr;
				for (int32 ImageIndex = 0; ImageIndex < List.GetNumImages(); ++ImageIndex)
				{
					const TArrayView<const uint8> Image = List.GetImage(ImageIndex);
					const uint32 First = FGSGifPacket::GetDmaAddress(Image.GetData());
					if (Address >= First && Address + (Quadwords * 16) <= First + uint32(Image.Num()))
					{
						Data = Image.GetData() + (Address - First);
					}
				}
				if (Data == nullptr)
				{
					return false;
				}
				const int32 Start = OutSent.Num();
				OutSent.AddUninitialized(int32(Quadwords) * 2);
				FMemory::Memcpy(&OutSent[Start], Data, SIZE_T(Quadwords) * 16);
				++Index;
				continue;
			}
			if (Id != uint32(EGSDmaTag::Cnt) && Id != uint32(EGSDmaTag::End))
			{
				return false;
			}
			for (uint32 Quadword = Index + 1; Quadword <= Index + Quadwords; ++Quadword)
			{
				OutSent.Add(Chain[Quadword * 2]);
				OutSent.Add(Chain[(Quadword * 2) + 1]);
			}
			Index += 1 + Quadwords;
			if (Id == uint32(EGSDmaTag::End))
			{
				return Index == NumQuadwords;
			}
		}
		return false;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreGifChainTest, "System.GSCore.GifPacket.Chain",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreGifChainTest::RunTest(const FString& Parameters)
{
	// A DMAtag (EE User's Manual 5.6): QWC bits 0-15, ID bits 28-30, ADDR bits 32-62.
	TestEqual("CNT, 7 quadwords", FGSGifPacket::MakeDmaTag(7, EGSDmaTag::Cnt), uint64(0x0000000010000007ull));
	TestEqual("REF, 2 quadwords at 0x123450", FGSGifPacket::MakeDmaTag(2, EGSDmaTag::Ref, 0x123450u),
		uint64(0x0012345030000002ull));
	TestEqual("END, 4 quadwords", FGSGifPacket::MakeDmaTag(4, EGSDmaTag::End), uint64(0x0000000070000004ull));
	TestEqual("The segment bits cleared",
		FGSGifPacket::GetDmaAddress(reinterpret_cast<const void*>(UPTRINT(0x30123450u))), 0x00123450u);

	// Build's packet for PRIM, a 4 x 2 PSMCT32 upload and TEXFLUSH, with FINISH: PACKED(5), IMAGE(2), PACKED(1),
	// PACKED(1), 13 quadwords. As a chain: CNT with the first tag, the 5 writes and the IMAGE tag; a REF to the list's
	// own pixels; END with the last two tags and writes.
	FGSCommandList List;
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Sprite;
	List.SetPrim(Prim);
	TArray<uint8> Pixels;
	for (uint32 Index = 0; Index < 32; ++Index)
	{
		Pixels.Add(uint8(Index));
	}
	List.UploadImage(FGSBitBltBuf(), 0, 0, 4, 2, Pixels);
	List.TexFlush();
	TArray<uint64> Packet;
	FGSGifPacket::Build(List, true, Packet);
	const uint32 Capacity = FGSGifPacket::GetChainCapacity(List, true);
	TArray<uint64> Chain;
	Chain.SetNumZeroed(int32(Capacity) * 2);
	const uint32 Written = FGSGifPacket::BuildChain(List, true, Chain.GetData(), Capacity);
	TestEqual("14 quadwords", Written, 14u);
	TestTrue("Within the capacity", Written <= Capacity);
	if (Packet.Num() != 26 || Written != 14)
	{
		return false;
	}
	TestEqual("CNT tag", Chain[0], FGSGifPacket::MakeDmaTag(7, EGSDmaTag::Cnt));
	TestEqual("CNT tag's VIFcodes: NOP, DIRECT 7", Chain[1],
		FGSGifPacket::MakeVifCodes(0, FGSGifPacket::MakeVifCode(EGSVifCommand::Direct, 0, 7)));
	TestEqual("DIRECT", FGSGifPacket::MakeVifCode(EGSVifCommand::Direct, 0, 7), 0x50000007u);
	bool bSame = true;
	for (int32 Index = 0; Index < 14; ++Index)
	{
		bSame &= Chain[2 + Index] == Packet[Index];
	}
	TestTrue("The CNT carries the packet's first 7 quadwords", bSame);
	const TArrayView<const uint8> Uploaded = List.GetImage(0);
	TestEqual("REF tag", Chain[16],
		FGSGifPacket::MakeDmaTag(2, EGSDmaTag::Ref, FGSGifPacket::GetDmaAddress(Uploaded.GetData())));
	TestEqual("REF's quadwords", DmaTagQuadwords(Chain[16]), 2u);
	TestEqual("REF's ID", DmaTagId(Chain[16]), 3u);
	TestTrue("The pixels are quadword aligned: the DMA reads them in place", (UPTRINT(Uploaded.GetData()) & 15) == 0);
	TestTrue("The REF'd pixels are the packet's IMAGE data", FMemory::Memcmp(Uploaded.GetData(), &Packet[14], 32) == 0);
	TestEqual("END tag", Chain[18], FGSGifPacket::MakeDmaTag(4, EGSDmaTag::End));
	bSame = true;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		bSame &= Chain[20 + Index] == Packet[18 + Index];
	}
	TestTrue("The END carries the last 4 quadwords, FINISH's tag with EOP", bSame);
	TArray<uint64> Sent;
	TestTrue("The DMAC follows the chain", FollowChain(List, Chain.GetData(), Written, Sent));
	TestTrue("The GIF receives Build's packet", Sent == Packet);

	// An empty list is an END of nothing; an upload last ends with an empty END.
	uint64 Empty[4] = {1, 1, 1, 1};
	TestEqual("Empty: one quadword", FGSGifPacket::BuildChain(FGSCommandList(), false, Empty, 4), 1u);
	TestEqual("Empty: END of 0", Empty[0], FGSGifPacket::MakeDmaTag(0, EGSDmaTag::End));
	TestEqual("Empty: two NOPs", Empty[1], uint64(0));
	FGSCommandList UploadLast;
	UploadLast.UploadImage(FGSBitBltBuf(), 0, 0, 4, 2, Pixels);
	TArray<uint64> UploadChain;
	UploadChain.SetNumZeroed(int32(FGSGifPacket::GetChainCapacity(UploadLast, false)) * 2);
	const uint32 UploadWritten = FGSGifPacket::BuildChain(
		UploadLast, false, UploadChain.GetData(), FGSGifPacket::GetChainCapacity(UploadLast, false));
	TestEqual("CNT, REF, END", UploadWritten, 9u);
	TestEqual("The empty END", UploadChain[16], FGSGifPacket::MakeDmaTag(0, EGSDmaTag::End));
	TArray<uint64> UploadPacket;
	FGSGifPacket::Build(UploadLast, false, UploadPacket);
	TArray<uint64> UploadSent;
	TestTrue(
		"The upload's chain is followed", FollowChain(UploadLast, UploadChain.GetData(), UploadWritten, UploadSent));
	TestTrue("The upload's packet, EOP on its IMAGE tag", UploadSent == UploadPacket);

	// QWC holds 0xffff: a longer section goes on in a second CNT (0x10000 writes, 3 PACKED tags, FINISH: 0x10005
	// quadwords of GIF data).
	FGSCommandList Long;
	for (uint32 Index = 0; Index < 0x10000; ++Index)
	{
		Long.SetRGBAQ(FGSRGBAQ());
	}
	const uint32 LongCapacity = FGSGifPacket::GetChainCapacity(Long, true);
	TArray<uint64> LongChain;
	LongChain.SetNumZeroed(int32(LongCapacity) * 2);
	const uint32 LongWritten = FGSGifPacket::BuildChain(Long, true, LongChain.GetData(), LongCapacity);
	TestEqual("Two sections", LongWritten, 0x10005u + 2u);
	TestEqual("A full CNT", LongChain[0], FGSGifPacket::MakeDmaTag(0xffff, EGSDmaTag::Cnt));
	TestEqual("The END after it", LongChain[2 * 0x10000], FGSGifPacket::MakeDmaTag(6, EGSDmaTag::End));
	TestEqual("FINISH last", LongChain[(2 * (LongWritten - 1)) + 1], uint64(EGSRegister::FINISH));
	return true;
}

namespace
{

	/** A stand-in for VU1's encoder: a CNT of nothing with an STCYCL first, and a CNT of nothing with MSCAL a batch. */
	class FTestBatchEncoder final : public IGSVertexBatchEncoder
	{
	public:
		[[nodiscard]] uint32 GetPrologueQuadwords() const override
		{
			return 1;
		}
		[[nodiscard]] uint32 GetMaxBatchQuadwords() const override
		{
			return 1;
		}
		uint64* WritePrologue(uint64* Out) override
		{
			Out[0] = FGSGifPacket::MakeDmaTag(0, EGSDmaTag::Cnt);
			Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::StCycl, 0, 0x0101), 0);
			return Out + 2;
		}
		uint64* WriteBatch(const FGSCommandList& List, const FGSVertexBatch& Batch, uint64* Out) override
		{
			(void)List;
			Out[0] = FGSGifPacket::MakeDmaTag(0, EGSDmaTag::Cnt);
			Out[1] =
				FGSGifPacket::MakeVifCodes(0, FGSGifPacket::MakeVifCode(EGSVifCommand::MsCal, 0, Batch.NumVertices));
			return Out + 2;
		}
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreGifChainBatchesTest, "System.GSCore.GifPacket.ChainBatches",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreGifChainBatchesTest::RunTest(const FString& Parameters)
{
	// PRIM, two vertex batches, TEXFLUSH and FINISH (Docs/PLANS/ps2-shipping.md N14): the encoder's prologue first,
	// PRIM's packet ends (EOP) before the batches, the empty section between the batches is dropped, and the section
	// after them flushes (VU1's packets reach the GS before the writes that follow them).
	alignas(16) int16 Positions[16] = {};
	alignas(16) int8 Normals[16] = {};
	alignas(16) uint8 Colors[16] = {};
	alignas(16) int16 TexCoords[8] = {};
	FGSCommandList List;
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::TriangleStrip;
	List.SetPrim(Prim);
	FGSVertexBatch Batch;
	Batch.Draw = List.AddVertexDraw(FGSVertexDraw());
	Batch.Positions = Positions;
	Batch.Normals = Normals;
	Batch.Colors = Colors;
	Batch.TexCoords = TexCoords;
	Batch.NumVertices = 3;
	List.DrawVertexBatch(Batch);
	Batch.NumVertices = 4;
	List.DrawVertexBatch(Batch);
	List.TexFlush();
	TestEqual("Two batch commands", List.GetVertexBatches().Num(), 2);
	TestTrue("A write each", List.GetWrites()[1].Register == EGSRegister::VertexBatch);

	FTestBatchEncoder Encoder;
	const uint32 Capacity = FGSGifPacket::GetChainCapacity(List, true, &Encoder);
	TArray<uint64> Chain;
	Chain.SetNumZeroed(int32(Capacity) * 2);
	const uint32 NumQuadwords = FGSGifPacket::BuildChain(List, true, Chain.GetData(), Capacity, &Encoder);
	TestEqual("Prologue, CNT of PRIM, two batches, END", NumQuadwords, 11u);
	TestTrue("Within the capacity", NumQuadwords <= Capacity);
	if (NumQuadwords != 11)
	{
		return false;
	}
	TestEqual("The prologue first", Chain[1], FGSGifPacket::MakeVifCodes(0x01000101u, 0));
	TestEqual("PRIM's CNT", Chain[2], FGSGifPacket::MakeDmaTag(2, EGSDmaTag::Cnt));
	TestEqual("PRIM's GIFtag ends its packet", Chain[4], FGSGifPacket::MakeTag(1, true, EGSGifFormat::Packed, 1));
	TestEqual("The first batch", Chain[9],
		FGSGifPacket::MakeVifCodes(0, FGSGifPacket::MakeVifCode(EGSVifCommand::MsCal, 0, 3)));
	TestEqual("The second, right after", Chain[11],
		FGSGifPacket::MakeVifCodes(0, FGSGifPacket::MakeVifCode(EGSVifCommand::MsCal, 0, 4)));
	TestEqual("The END", Chain[12], FGSGifPacket::MakeDmaTag(4, EGSDmaTag::End));
	TestEqual("It flushes, then DIRECT", Chain[13],
		FGSGifPacket::MakeVifCodes(
			FGSGifPacket::MakeVifCode(EGSVifCommand::Flush), FGSGifPacket::MakeVifCode(EGSVifCommand::Direct, 0, 4)));
	TestEqual("TEXFLUSH's GIFtag", Chain[14], FGSGifPacket::MakeTag(1, false, EGSGifFormat::Packed, 1));
	TestEqual("FINISH's ends the packet", Chain[18], FGSGifPacket::MakeTag(1, true, EGSGifFormat::Packed, 1));

	// Appended, the batches name their draws and are named by their writes in the new list.
	FGSCommandList Frame;
	Batch.Draw = Frame.AddVertexDraw(FGSVertexDraw());
	Frame.DrawVertexBatch(Batch);
	Frame.Append(List);
	TestEqual("Three batches", Frame.GetVertexBatches().Num(), 3);
	TestEqual("Two draws", Frame.GetVertexDraws().Num(), 2);
	TestEqual("The appended batch's draw", Frame.GetVertexBatches()[2].Draw, 1);
	TestEqual("The appended batch's write", Frame.GetWrites()[3].Value, uint64(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreCommandListAppendTest, "System.GSCore.CommandList.Append",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreCommandListAppendTest::RunTest(const FString& Parameters)
{
	// Appending keeps the order and points the appended HWREG writes at their own image data.
	TArray<uint8> First;
	First.Init(1, 16);
	TArray<uint8> Second;
	Second.Init(2, 16);
	FGSCommandList List;
	List.UploadImage(FGSBitBltBuf(), 0, 0, 2, 2, First);
	FGSCommandList Other;
	Other.TexFlush();
	Other.UploadImage(FGSBitBltBuf(), 0, 0, 2, 2, Second);
	List.Append(Other);
	TestEqual("Writes", List.GetWrites().Num(), 11);
	TestEqual("TEXFLUSH after the first upload", List.GetWrites()[5].Register, EGSRegister::TEXFLUSH);
	TestEqual("The appended HWREG", List.GetWrites()[10].Value, uint64(1));
	TestEqual("Its data", List.GetImage(1)[0], uint8(2));

	// An image held in place stays in place through Append (the PS2's chain REFs the cooked texture itself), until
	// CopyInPlaceImages copies it (its texture is going away while a list still holds it).
	alignas(16) static const uint8 Cooked[16] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
	FGSCommandList InPlace;
	InPlace.UploadImageInPlace(FGSBitBltBuf(), 0, 0, 2, 2, TArrayView<const uint8>(Cooked, 16));
	TestTrue("Held in place", InPlace.IsImageInPlace(0) && InPlace.GetImage(0).GetData() == Cooked);
	List.Append(InPlace);
	TestTrue("Still in place once appended", List.IsImageInPlace(2) && List.GetImage(2).GetData() == Cooked);
	TestTrue("The copies copied again",
		!List.IsImageInPlace(0) && List.GetImage(1).GetData() != Other.GetImage(0).GetData());
	List.CopyInPlaceImages();
	TestTrue("Copied on request",
		!List.IsImageInPlace(2) && List.GetImage(2).GetData() != Cooked && List.GetImage(2)[5] == 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreConformanceScenesTest, "System.GSCore.ConformanceScenes.Record",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreConformanceScenesTest::RunTest(const FString& Parameters)
{
	// Every scene records within the supported subset (the setters check it) and starts by pointing FRAME_1 at its
	// frame buffer; the reference rasterizer's tests check what they draw.
	const TArrayView<const FGSConformanceScene> Scenes = GSConformance::GetScenes();
	TestEqual("Twenty-one scenes", Scenes.Num(), 21);
	for (const FGSConformanceScene& Scene : Scenes)
	{
		FGSCommandList List;
		Scene.Build(List);
		bool bSetsFrame = false;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			if (Write.Register == EGSRegister::FRAME_1)
			{
				bSetsFrame = FGSFrame::Decode(Write.Value).PSM == Scene.FrameFormat;
				break;
			}
		}
		TestTrue(*FString::Printf(TEXT("%s sets its frame buffer"), Scene.Name), bSetsFrame);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCoreTextureLayoutTest, "System.GSCore.TextureLayout",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCoreTextureLayoutTest::RunTest(const FString& Parameters)
{
	// Pages by format (manual 8.1), TBW over whole pages (even for the indexed formats), and the blocks a texture takes
	// in the GS's layout (8.3, 8.4): a texture inside a page takes the blocks it reaches, a larger one whole pages.
	uint32 Width = 0;
	uint32 Height = 0;
	FGSTextureLayout::GetPageSize(EGSPixelFormat::PSMT8, Width, Height);
	TestTrue("PSMT8 page 128 x 64", Width == 128 && Height == 64);
	FGSTextureLayout::GetPageSize(EGSPixelFormat::PSMT4, Width, Height);
	TestTrue("PSMT4 page 128 x 128", Width == 128 && Height == 128);
	TestEqual("PSMCT32 8 wide: TBW 1", int32(FGSTextureLayout::GetBufferWidth(EGSPixelFormat::PSMCT32, 8)), 1);
	TestEqual("PSMCT32 256 wide: TBW 4", int32(FGSTextureLayout::GetBufferWidth(EGSPixelFormat::PSMCT32, 256)), 4);
	TestEqual("PSMT8 16 wide: TBW 2", int32(FGSTextureLayout::GetBufferWidth(EGSPixelFormat::PSMT8, 16)), 2);
	TestEqual("PSMT4 256 wide: TBW 4", int32(FGSTextureLayout::GetBufferWidth(EGSPixelFormat::PSMT4, 256)), 4);
	TestEqual("PSMCT32 64 x 64: 2 pages", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMCT32, 64, 64), 64u);
	TestEqual("PSMT8 256 x 256: 8 pages", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMT8, 256, 256), 256u);
	TestEqual("PSMT4 8 x 8: 1 block", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMT4, 8, 8), 1u);
	TestEqual("PSMT4 32 x 32: blocks 0 and 1", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMT4, 32, 32), 2u);
	TestEqual("PSMT4 64 x 64: blocks 0 to 7", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMT4, 64, 64), 8u);
	TestEqual("PSMT8 16 x 16: 1 block", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMT8, 16, 16), 1u);
	TestEqual("PSMT8 64 x 64: half a page", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMT8, 64, 64), 16u);
	TestEqual("PSMCT32 16 x 16: 4 blocks", FGSTextureLayout::GetNumBlocks(EGSPixelFormat::PSMCT32, 16, 16), 4u);
	TestEqual("PSMCT32 8 x 8 at any block", FGSTextureLayout::GetBaseAlignment(EGSPixelFormat::PSMCT32, 8, 8), 1u);
	TestEqual("PSMCT32 64 x 32 (a page) at any block",
		FGSTextureLayout::GetBaseAlignment(EGSPixelFormat::PSMCT32, 64, 32), 1u);
	TestEqual("PSMCT32 64 x 64 on a page", FGSTextureLayout::GetBaseAlignment(EGSPixelFormat::PSMCT32, 64, 64), 32u);
	TestEqual("PSMT4 256 x 256 on a page", FGSTextureLayout::GetBaseAlignment(EGSPixelFormat::PSMT4, 256, 256), 32u);
	TestEqual("PSMT8 CLUT: 4 blocks", FGSTextureLayout::GetClutBlocks(EGSPixelFormat::PSMT8), 4u);
	TestEqual("PSMT4 CLUT: 1 block", FGSTextureLayout::GetClutBlocks(EGSPixelFormat::PSMT4), 1u);
	TestEqual("No CLUT", FGSTextureLayout::GetClutBlocks(EGSPixelFormat::PSMCT32), 0u);

	// CSM1: entries 8..15 and 16..23 of each 32 trade places in the 16 x 16 rectangle; alpha 255 is the GS's 0x80.
	TArray<uint32> Palette;
	for (uint32 Index = 0; Index < 256; ++Index)
	{
		Palette.Add(Index | 0xff000000u);
	}
	TArray<uint8> Image;
	uint16 ClutWidth = 0;
	uint16 ClutHeight = 0;
	FGSTextureLayout::MakeClutImage(Palette, Image, ClutWidth, ClutHeight);
	TestTrue("16 x 16", ClutWidth == 16 && ClutHeight == 16 && Image.Num() == 16 * 16 * 4);
	TestEqual("Entry 8 at (0, 1)", int32(Image[(16 * 1 + 0) * 4]), 8);
	TestEqual("Entry 16 at (8, 0)", int32(Image[(0 * 16 + 8) * 4]), 16);
	TestEqual("Entry 48 at (8, 2)", int32(Image[(16 * 2 + 8) * 4]), 48);
	TestEqual("Alpha 0x80", int32(Image[3]), 0x80);
	FGSTextureLayout::MakeClutImage(TArrayView<const uint32>(Palette.GetData(), 16), Image, ClutWidth, ClutHeight);
	TestTrue("16 entries: 8 x 2, in order", ClutWidth == 8 && ClutHeight == 2 && Image[9 * 4] == 9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
