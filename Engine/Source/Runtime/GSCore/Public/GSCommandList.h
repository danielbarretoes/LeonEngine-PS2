#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GSTypes.h"

/** One general purpose register write: what the GIF sends in PACKED A+D mode. */
struct FGSRegisterWrite
{
	EGSRegister Register = EGSRegister::PRIM;
	/** The register's value; for HWREG, the index of the image data (FGSCommandList::GetImageData). */
	uint64 Value = 0;
};

/**
 * The Graphics Synthesizer's work for a frame as the GS itself receives it (Leon; UE's counterpart is the RHI command
 * list): register writes in order, a vertex kick on each XYZ2 / XYZF2, and host-to-local image transfers (BITBLTBUF,
 * TRXPOS, TRXREG, TRXDIR, then the pixels through HWREG). The renderer fills it with vertices already transformed,
 * clipped and lit; every backend consumes the same list: the PS2 sends it to the GIF, the Win64 preview emulates the
 * GS, and the reference rasterizer checks both.
 *
 * The setters accept only what the preview reproduces exactly (IsSupported): a check fails on anything else, so a list
 * that records draws the same on every backend. The context registers (the ones with _1 / _2) take the context, 0 or 1.
 */
class GSCORE_API FGSCommandList
{
public:
	// Vertex registers

	void SetPrim(const FGSPrim& Prim);
	void SetRGBAQ(const FGSRGBAQ& Color);
	void SetST(const FGSST& ST);
	void SetUV(const FGSUV& UV);
	void SetFog(const FGSFog& Fog);
	/** XYZ2: a vertex that kicks the drawing. */
	void AddVertex(const FGSXYZ& Vertex);
	/** XYZF2: a vertex with its fog coefficient that kicks the drawing. */
	void AddVertex(const FGSXYZF& Vertex);
	/** XYZ3: a vertex that does not kick the drawing (the first ones of a strip after a PRIM reset). */
	void AddVertexNoKick(const FGSXYZ& Vertex);

	// Context registers

	void SetTex0(uint8 Context, const FGSTex0& Tex0);
	void SetTex1(uint8 Context, const FGSTex1& Tex1);
	void SetClamp(uint8 Context, const FGSClamp& Clamp);
	void SetMipTbp1(uint8 Context, const FGSMipTbp& MipTbp);
	void SetMipTbp2(uint8 Context, const FGSMipTbp& MipTbp);
	void SetXYOffset(uint8 Context, const FGSXYOffset& Offset);
	void SetScissor(uint8 Context, const FGSScissor& Scissor);
	void SetAlpha(uint8 Context, const FGSAlpha& Alpha);
	void SetTest(uint8 Context, const FGSTest& Test);
	/** FBA: the alpha's MSB forced to 1 on write. */
	void SetFba(uint8 Context, bool bForceAlphaMSB);
	void SetFrame(uint8 Context, const FGSFrame& Frame);
	void SetZBuf(uint8 Context, const FGSZBuf& ZBuf);

	// Common registers

	/** PRMODECONT: the attributes come from PRIM (true) or PRMODE. Only PRIM is supported. */
	void SetPrimModeFromPrim();
	void SetFogCol(const FGSFogCol& FogCol);
	void SetDimx(const FGSDimx& Dimx);
	/** DTHE. */
	void SetDither(bool bDither);
	/** COLCLAMP: clamp (true) or wrap the colors to 8 bits. */
	void SetColorClamp(bool bClamp);
	/** PABE: blend only the pixels whose alpha MSB is set. */
	void SetPixelAlphaBlend(bool bPerPixel);
	void SetTexA(const FGSTexA& TexA);
	void SetTexClut(const FGSTexClut& TexClut);
	/** TEXFLUSH: after an upload, before a texture that uses it. */
	void TexFlush();

	/**
	 * Uploads Width x Height pixels of Destination's format (DBP, DBW, DPSM) to (X, Y) of that buffer. Pixels holds
	 * them row by row, packed at the format's bits per pixel; the GIF moves quadwords, so their size is a multiple
	 * of 16.
	 */
	void UploadImage(const FGSBitBltBuf& Destination, uint16 X, uint16 Y, uint16 Width, uint16 Height,
		TArrayView<const uint8> Pixels);

	[[nodiscard]] const TArray<FGSRegisterWrite>& GetWrites() const
	{
		return Writes;
	}
	[[nodiscard]] const TArray<TArray<uint8>>& GetImageData() const
	{
		return ImageData;
	}
	void Reset();

	// The subset every backend reproduces

	/** Not AA1 (antialiasing needs coverage the preview does not emulate). */
	[[nodiscard]] static bool IsSupported(const FGSPrim& Prim);
	/** (Cs - Cd) * C + Cd, (Cs - 0) * C + Cd and (Cs - 0) * C + 0, with C the source alpha or FIX. */
	[[nodiscard]] static bool IsSupported(const FGSAlpha& Alpha);
	/** PSMCT32, PSMCT24, PSMCT16 or a CLUT format (PSMT8, PSMT4) with a PSMCT32 or PSMCT16 CLUT in CSM1; TW, TH <= 10.
	 */
	[[nodiscard]] static bool IsSupported(const FGSTex0& Tex0);
	/** The depth test on (the manual forbids it off). */
	[[nodiscard]] static bool IsSupported(const FGSTest& Test);
	/** A color format (PSMCT32, PSMCT24, PSMCT16, PSMCT16S). */
	[[nodiscard]] static bool IsSupported(const FGSFrame& Frame);
	/** A Z format. */
	[[nodiscard]] static bool IsSupported(const FGSZBuf& ZBuf);

	/** The register of a context pair: Base (the _1 one) for context 0, the next address for context 1. */
	[[nodiscard]] static EGSRegister ContextRegister(EGSRegister Base, uint8 Context);

private:
	void Write(EGSRegister Register, uint64 Value);

	TArray<FGSRegisterWrite> Writes;
	TArray<TArray<uint8>> ImageData;
};
