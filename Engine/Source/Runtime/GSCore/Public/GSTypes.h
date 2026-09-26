#pragma once

#include "CoreMinimal.h"

/**
 * The Graphics Synthesizer's general purpose registers and formats (Docs/PS2OFFICIAL/GS_Users_Manual.pdf, chapter 7).
 * Each register is a struct with its fields at their manual names and ranges; Encode packs it into the 64-bit value
 * the GIF writes (A+D) and Decode unpacks one. The fields keep the manual's units: fixed point where the manual has it
 * (12.4 window coordinates, 10.4 texel coordinates), float where it has float (S, T, Q).
 */

/** The address of a general purpose register (GS manual 7.1, the A+D register descriptor). */
enum class EGSRegister : uint8
{
	PRIM = 0x00,
	RGBAQ = 0x01,
	ST = 0x02,
	UV = 0x03,
	XYZF2 = 0x04,
	XYZ2 = 0x05,
	TEX0_1 = 0x06,
	TEX0_2 = 0x07,
	CLAMP_1 = 0x08,
	CLAMP_2 = 0x09,
	FOG = 0x0a,
	XYZF3 = 0x0c,
	XYZ3 = 0x0d,
	TEX1_1 = 0x14,
	TEX1_2 = 0x15,
	TEX2_1 = 0x16,
	TEX2_2 = 0x17,
	XYOFFSET_1 = 0x18,
	XYOFFSET_2 = 0x19,
	PRMODECONT = 0x1a,
	PRMODE = 0x1b,
	TEXCLUT = 0x1c,
	SCANMSK = 0x22,
	MIPTBP1_1 = 0x34,
	MIPTBP1_2 = 0x35,
	MIPTBP2_1 = 0x36,
	MIPTBP2_2 = 0x37,
	TEXA = 0x3b,
	FOGCOL = 0x3d,
	TEXFLUSH = 0x3f,
	SCISSOR_1 = 0x40,
	SCISSOR_2 = 0x41,
	ALPHA_1 = 0x42,
	ALPHA_2 = 0x43,
	DIMX = 0x44,
	DTHE = 0x45,
	COLCLAMP = 0x46,
	TEST_1 = 0x47,
	TEST_2 = 0x48,
	PABE = 0x49,
	FBA_1 = 0x4a,
	FBA_2 = 0x4b,
	FRAME_1 = 0x4c,
	FRAME_2 = 0x4d,
	ZBUF_1 = 0x4e,
	ZBUF_2 = 0x4f,
	BITBLTBUF = 0x50,
	TRXPOS = 0x51,
	TRXREG = 0x52,
	TRXDIR = 0x53,
	HWREG = 0x54,
};

/** Pixel storage formats (TEX0.PSM, FRAME.PSM, BITBLTBUF.SPSM / DPSM; the Z formats' low 4 bits are ZBUF.PSM). */
enum class EGSPixelFormat : uint8
{
	PSMCT32 = 0x00,
	PSMCT24 = 0x01,
	PSMCT16 = 0x02,
	PSMCT16S = 0x0a,
	PSMT8 = 0x13,
	PSMT4 = 0x14,
	PSMT8H = 0x1b,
	PSMT4HL = 0x24,
	PSMT4HH = 0x2c,
	PSMZ32 = 0x30,
	PSMZ24 = 0x31,
	PSMZ16 = 0x32,
	PSMZ16S = 0x3a,
};

/** PRIM.PRIM. */
enum class EGSPrimitive : uint8
{
	Point = 0,
	Line = 1,
	LineStrip = 2,
	Triangle = 3,
	TriangleStrip = 4,
	TriangleFan = 5,
	Sprite = 6,
};

/** TEX0.TFX: how the texture color combines with the vertex color. */
enum class EGSTextureFunction : uint8
{
	Modulate = 0,
	Decal = 1,
	Highlight = 2,
	Highlight2 = 3,
};

/** CLAMP.WMS / WMT. */
enum class EGSWrapMode : uint8
{
	Repeat = 0,
	Clamp = 1,
	RegionClamp = 2,
	RegionRepeat = 3,
};

/** TEX1.MMAG (Nearest, Linear) and TEX1.MMIN (all). */
enum class EGSFilter : uint8
{
	Nearest = 0,
	Linear = 1,
	NearestMipmapNearest = 2,
	NearestMipmapLinear = 3,
	LinearMipmapNearest = 4,
	LinearMipmapLinear = 5,
};

/** ALPHA.A / B / D: the color inputs of Cv = (A - B) * C >> 7 + D. */
enum class EGSBlendColor : uint8
{
	Source = 0,
	Destination = 1,
	Zero = 2,
};

/** ALPHA.C: the alpha input. */
enum class EGSBlendAlpha : uint8
{
	Source = 0,
	Destination = 1,
	Fixed = 2,
};

/** TEST.ATST. */
enum class EGSAlphaTest : uint8
{
	Never = 0,
	Always = 1,
	Less = 2,
	LessEqual = 3,
	Equal = 4,
	GreaterEqual = 5,
	Greater = 6,
	NotEqual = 7,
};

/** TEST.AFAIL: what a pixel that fails the alpha test still updates. */
enum class EGSAlphaFail : uint8
{
	Keep = 0,
	FrameBufferOnly = 1,
	ZBufferOnly = 2,
	RGBOnly = 3,
};

/** TEST.ZTST. */
enum class EGSDepthTest : uint8
{
	Never = 0,
	Always = 1,
	GreaterEqual = 2,
	Greater = 3,
};

/** TRXDIR.XDIR. */
enum class EGSTransferDirection : uint8
{
	HostToLocal = 0,
	LocalToHost = 1,
	LocalToLocal = 2,
	Deactivated = 3,
};

/** Converts window or texel coordinates to the GS's 4-bit fraction fixed point, rounded, clamped to the field. */
[[nodiscard]] GSCORE_API uint16 GSToFixed4(float Value, uint32 FieldBits);

/** PRIM (0x00): the primitive type and its attributes. */
struct GSCORE_API FGSPrim
{
	EGSPrimitive Type = EGSPrimitive::Triangle;
	/** IIP: Gouraud shading (flat otherwise). */
	bool bGouraud = false;
	/** TME. */
	bool bTextured = false;
	/** FGE. */
	bool bFog = false;
	/** ABE. */
	bool bAlphaBlend = false;
	/** AA1: one-pass antialiasing. */
	bool bAntialias = false;
	/** FST: texel coordinates (UV) instead of STQ (no perspective correction). */
	bool bUseUV = false;
	/** CTXT: 0 or 1 (context 1 or 2). */
	uint8 Context = 0;
	/** FIX: fragment values fixed (not interpolated by the DDA). */
	bool bFixFragment = false;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSPrim Decode(uint64 Value);
};

/** RGBAQ (0x01): the vertex color (A: 0x80 is 1.0) and Q. */
struct GSCORE_API FGSRGBAQ
{
	uint8 R = 0;
	uint8 G = 0;
	uint8 B = 0;
	uint8 A = 0x80;
	float Q = 1.0f;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSRGBAQ Decode(uint64 Value);
};

/** ST (0x02): the vertex texture coordinates S and T, pre-divided by Q's reciprocal as the manual describes. */
struct GSCORE_API FGSST
{
	float S = 0.0f;
	float T = 0.0f;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSST Decode(uint64 Value);
};

/** UV (0x03): texel coordinates, 10.4 fixed point. */
struct GSCORE_API FGSUV
{
	uint16 U = 0;
	uint16 V = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSUV Decode(uint64 Value);
};

/** XYZ2 / XYZ3 (0x05 / 0x0d): a vertex, window coordinates in 12.4 fixed point and a 32-bit Z. */
struct GSCORE_API FGSXYZ
{
	uint16 X = 0;
	uint16 Y = 0;
	uint32 Z = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSXYZ Decode(uint64 Value);
};

/** XYZF2 / XYZF3 (0x04 / 0x0c): a vertex with a 24-bit Z and its fog coefficient. */
struct GSCORE_API FGSXYZF
{
	uint16 X = 0;
	uint16 Y = 0;
	/** 24 bits. */
	uint32 Z = 0;
	uint8 F = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSXYZF Decode(uint64 Value);
};

/** TEX0_1 / TEX0_2 (0x06 / 0x07): the texture and its CLUT. */
struct GSCORE_API FGSTex0
{
	/** TBP0: base pointer, word address / 64 (14 bits). */
	uint16 TBP0 = 0;
	/** TBW: buffer width in pixels / 64 (6 bits). */
	uint8 TBW = 1;
	EGSPixelFormat PSM = EGSPixelFormat::PSMCT32;
	/** TW / TH: log2 of the width and height (at most 10). */
	uint8 TW = 0;
	uint8 TH = 0;
	/** TCC: the texture's alpha is used (RGBA; RGB otherwise). */
	bool bRGBA = true;
	EGSTextureFunction TFX = EGSTextureFunction::Modulate;
	/** CBP: CLUT base pointer (14 bits). */
	uint16 CBP = 0;
	/** CPSM: the CLUT's format, PSMCT32, PSMCT16 or PSMCT16S. */
	EGSPixelFormat CPSM = EGSPixelFormat::PSMCT32;
	/** CSM: storage mode CSM2 (CSM1 otherwise). */
	bool bCSM2 = false;
	/** CSA: CLUT entry offset (5 bits). */
	uint8 CSA = 0;
	/** CLD: CLUT buffer load control (3 bits). */
	uint8 CLD = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTex0 Decode(uint64 Value);
};

/** TEX1_1 / TEX1_2 (0x14 / 0x15): filtering and the LOD (LOD = (log2(1/|Q|) << L) + K, or K when fixed). */
struct GSCORE_API FGSTex1
{
	/** LCM: the LOD is K (fixed) instead of the formula. */
	bool bFixedLOD = false;
	/** MXL: the maximum MIP level (0 to 6). */
	uint8 MXL = 0;
	/** MMAG: Nearest or Linear. */
	EGSFilter MMAG = EGSFilter::Nearest;
	EGSFilter MMIN = EGSFilter::Nearest;
	/** MTBA: the MIP levels' base pointers are set automatically. */
	bool bAutoMipBase = false;
	/** L (2 bits). */
	uint8 L = 0;
	/** K: signed 7.4 fixed point (12 bits), in sixteenths. */
	int16 K = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTex1 Decode(uint64 Value);
};

/** CLAMP_1 / CLAMP_2 (0x08 / 0x09): the wrap modes and the region parameters (10 bits each). */
struct GSCORE_API FGSClamp
{
	EGSWrapMode WMS = EGSWrapMode::Repeat;
	EGSWrapMode WMT = EGSWrapMode::Repeat;
	uint16 MINU = 0;
	uint16 MAXU = 0;
	uint16 MINV = 0;
	uint16 MAXV = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSClamp Decode(uint64 Value);
};

/** FOG (0x0a): the vertex's fog coefficient (0: fog color only, 255: vertex color only). */
struct GSCORE_API FGSFog
{
	uint8 F = 0xff;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSFog Decode(uint64 Value);
};

/** FOGCOL (0x3d): the fog color. */
struct GSCORE_API FGSFogCol
{
	uint8 R = 0;
	uint8 G = 0;
	uint8 B = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSFogCol Decode(uint64 Value);
};

/** ALPHA_1 / ALPHA_2 (0x42 / 0x43): Cv = (A - B) * C >> 7 + D, C = 0x80 being 1.0. */
struct GSCORE_API FGSAlpha
{
	EGSBlendColor A = EGSBlendColor::Source;
	EGSBlendColor B = EGSBlendColor::Destination;
	EGSBlendAlpha C = EGSBlendAlpha::Source;
	EGSBlendColor D = EGSBlendColor::Destination;
	/** FIX: the fixed alpha when C is Fixed. */
	uint8 FIX = 0x80;

	/** Cs, Cd, As, Cd: the usual translucency. */
	[[nodiscard]] static FGSAlpha Translucent();
	/** Cs, 0, As, Cd: additive, scaled by the source alpha. */
	[[nodiscard]] static FGSAlpha Additive();

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSAlpha Decode(uint64 Value);
};

/** TEST_1 / TEST_2 (0x47 / 0x48): the alpha, destination alpha and depth tests. */
struct GSCORE_API FGSTest
{
	/** ATE. */
	bool bAlphaTest = false;
	EGSAlphaTest ATST = EGSAlphaTest::Always;
	uint8 AREF = 0;
	EGSAlphaFail AFAIL = EGSAlphaFail::Keep;
	/** DATE and DATM. */
	bool bDestinationAlphaTest = false;
	bool bDestinationAlphaOne = false;
	/** ZTE: the manual does not allow it off; the default keeps it on. */
	bool bDepthTest = true;
	EGSDepthTest ZTST = EGSDepthTest::Always;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTest Decode(uint64 Value);
};

/** ZBUF_1 / ZBUF_2 (0x4e / 0x4f): the Z buffer. */
struct GSCORE_API FGSZBuf
{
	/** ZBP: base pointer, word address / 2048 (9 bits). */
	uint16 ZBP = 0;
	/** A PSMZ format (its low 4 bits are the field). */
	EGSPixelFormat PSM = EGSPixelFormat::PSMZ24;
	/** ZMSK: Z is not written. */
	bool bMask = false;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSZBuf Decode(uint64 Value);
};

/** FRAME_1 / FRAME_2 (0x4c / 0x4d): the frame buffer. */
struct GSCORE_API FGSFrame
{
	/** FBP: base pointer, word address / 2048 (9 bits). */
	uint16 FBP = 0;
	/** FBW: width in pixels / 64 (6 bits). */
	uint8 FBW = 10;
	EGSPixelFormat PSM = EGSPixelFormat::PSMCT32;
	/** FBMSK: the bits not written. */
	uint32 FBMSK = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSFrame Decode(uint64 Value);
};

/** SCISSOR_1 / SCISSOR_2 (0x40 / 0x41): the drawing area in window pixels, both corners included (11 bits). */
struct GSCORE_API FGSScissor
{
	uint16 SCAX0 = 0;
	uint16 SCAX1 = 0;
	uint16 SCAY0 = 0;
	uint16 SCAY1 = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSScissor Decode(uint64 Value);
};

/** XYOFFSET_1 / XYOFFSET_2 (0x18 / 0x19): subtracted from the primitive coordinates, 12.4 fixed point. */
struct GSCORE_API FGSXYOffset
{
	uint16 OFX = 0;
	uint16 OFY = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSXYOffset Decode(uint64 Value);
};

/** DIMX (0x44): the 4x4 dither matrix, entries -4 to 3, added to RGB as M[Y % 4][X % 4] (manual 3.9.1). */
struct GSCORE_API FGSDimx
{
	int8 M[4][4] = {};

	/** The manual's example (3.9.1): -4, 2, -3, 3 / 0, -2, 1, -1 / -3, 3, -4, 2 / 1, -1, 0, -2. */
	[[nodiscard]] static FGSDimx Default();

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSDimx Decode(uint64 Value);
};

/** TEXA (0x3b): the alpha of RGB24 and RGBA16 texels. */
struct GSCORE_API FGSTexA
{
	uint8 TA0 = 0;
	/** AEM: a black RGB16 texel is transparent. */
	bool bAlphaExpandBlack = false;
	uint8 TA1 = 0x80;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTexA Decode(uint64 Value);
};

/** TEXCLUT (0x1c): the CLUT's position for CSM2. */
struct GSCORE_API FGSTexClut
{
	uint8 CBW = 0;
	uint8 COU = 0;
	uint16 COV = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTexClut Decode(uint64 Value);
};

/** MIPTBP1 / MIPTBP2 (0x34 to 0x37): the base pointers and widths of MIP levels 1 to 3 (or 4 to 6). */
struct GSCORE_API FGSMipTbp
{
	uint16 TBP[3] = {};
	uint8 TBW[3] = {};

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSMipTbp Decode(uint64 Value);
};

/** BITBLTBUF (0x50): the buffers of a transfer. */
struct GSCORE_API FGSBitBltBuf
{
	uint16 SBP = 0;
	uint8 SBW = 0;
	EGSPixelFormat SPSM = EGSPixelFormat::PSMCT32;
	/** DBP: word address / 64 (14 bits). */
	uint16 DBP = 0;
	/** DBW: width in pixels / 64 (6 bits). */
	uint8 DBW = 1;
	EGSPixelFormat DPSM = EGSPixelFormat::PSMCT32;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSBitBltBuf Decode(uint64 Value);
};

/** TRXPOS (0x51): the transfer rectangles' upper-left corners (11 bits) and the pixel order (local to local). */
struct GSCORE_API FGSTrxPos
{
	uint16 SSAX = 0;
	uint16 SSAY = 0;
	uint16 DSAX = 0;
	uint16 DSAY = 0;
	uint8 DIR = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTrxPos Decode(uint64 Value);
};

/** TRXREG (0x52): the transfer's width and height in pixels (12 bits). */
struct GSCORE_API FGSTrxReg
{
	uint16 RRW = 0;
	uint16 RRH = 0;

	[[nodiscard]] uint64 Encode() const;
	[[nodiscard]] static FGSTrxReg Decode(uint64 Value);
};

/** The bits per pixel of a storage format (4, 8, 16, 24 or 32; PSMCT24 and PSMZ24 take 32 in memory). */
[[nodiscard]] GSCORE_API uint32 GSBitsPerPixel(EGSPixelFormat Format);
