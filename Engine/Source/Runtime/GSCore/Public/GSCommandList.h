#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GSTypes.h"
#include "GSVertexBatch.h"

struct FGSDrawEnvironment;

/** One general purpose register write: what the GIF sends in PACKED A+D mode. */
struct FGSRegisterWrite
{
	EGSRegister Register = EGSRegister::PRIM;
	/**
	 * The register's value; for HWREG, the index of the image (FGSCommandList::GetImage); for VertexBatch, the index of
	 * the batch (FGSCommandList::GetVertexBatches).
	 */
	uint64 Value = 0;
};

/**
 * The Graphics Synthesizer's work for a frame as the GS itself receives it (Leon; UE's counterpart is the RHI command
 * list): register writes in order, a vertex kick on each XYZ2 / XYZF2, and host-to-local image transfers (BITBLTBUF,
 * TRXPOS, TRXREG, TRXDIR, then the pixels through HWREG). The renderer fills it with vertices already transformed,
 * clipped and lit; every backend consumes the same list: the PS2 sends it to the GIF, the Win64 preview emulates the
 * GS, and the reference rasterizer checks both.
 *
 * The setters accept only what the desktop's GS emulator reproduces and a GSConformance scene checks against the
 * reference rasterizer (IsSupported; Docs/PLANS/ps2-shipping.md D7): a check fails on anything else, so a list that
 * records draws the same on every backend. The context registers (the ones with _1 / _2) take the context, 0 or 1.
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
	/**
	 * XYZ3: a vertex that advances the vertex queue without the drawing kick (manual 3.2.5): the primitive it would
	 * close is not drawn, and a strip or fan goes on from it (the VIF's ADC bit becomes this on the GIF).
	 */
	void AddVertexNoKick(const FGSXYZ& Vertex);
	/** XYZF3: XYZ3 with the vertex's fog coefficient. */
	void AddVertexNoKick(const FGSXYZF& Vertex);

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
	 * them row by row, packed as the manual's transfer format (4.3: 24-bit pixels in 3 bytes, the first 4-bit pixel in
	 * the low nibble); the GIF moves quadwords, so their size is a multiple of 16. The start and width follow
	 * IsSupportedUpload.
	 */
	void UploadImage(const FGSBitBltBuf& Destination, uint16 X, uint16 Y, uint16 Width, uint16 Height,
		TArrayView<const uint8> Pixels);

	/**
	 * UploadImage without the copy (Docs/PLANS/ps2-shipping.md N23): the list keeps a view of Pixels, which must be
	 * quadword aligned and stay unchanged until every list that holds it has been sent (the PS2's DMA chain reads it in
	 * place by a REF tag). For data that outlives the frame: a cooked texture's levels and CLUT, loaded GIF-ready.
	 */
	void UploadImageInPlace(const FGSBitBltBuf& Destination, uint16 X, uint16 Y, uint16 Width, uint16 Height,
		TArrayView<const uint8> Pixels);

	// Vertex batches (Docs/PLANS/ps2-shipping.md N14)

	/** Adds a draw the next vertex batches name (their Draw); returns its index. */
	int32 AddVertexDraw(const FGSVertexDraw& Draw);
	/**
	 * Records a vertex batch of one of the list's draws, a write of EGSRegister::VertexBatch: its GS writes are made
	 * where the list is consumed (the PS2's VU1). The draw must have a program (FGSVertexDraw::GetProgram), and the
	 * batch's streams must outlive the list's consumption (they are not copied). Afterwards PRIM is the batch's and
	 * the GS's vertex queue, RGBAQ and ST are undefined.
	 */
	void DrawVertexBatch(const FGSVertexBatch& Batch);
	/**
	 * Memory for a skinned batch's palette of NumBones skin matrices (FGSVertexBatch::Palette), the list's until Reset
	 * and where it stays while the list grows (the PS2 sends it to VU1 by reference). Batches of one palette share it.
	 * Reset keeps the memory for the next frame.
	 */
	[[nodiscard]] FGSSkinMatrix* AllocateSkinPalette(uint32 NumBones);

	/**
	 * Appends Other's writes, images, draws and batches after this list's: the images Other copied are copied again,
	 * the ones it holds in place stay in place; the skinned batches' palettes are copied into this list's memory.
	 */
	void Append(const FGSCommandList& Other);
	/**
	 * Appends Other with its vertex batches as the GS writes the C++ emitter makes of them (BeginStrip, then
	 * FGSPrimitiveEmitter::AddVertexBatch, against Environment; a batch across a clip plane BeginTriangles, then
	 * AddClippedVertexBatch): the reference of what VU1 draws, for a backend without it (the reference rasterizer in
	 * the tests).
	 */
	void AppendExpanded(const FGSCommandList& Other, const FGSDrawEnvironment& Environment);

	/** Copies the images held in place into the list (their data is about to change or go). */
	void CopyInPlaceImages();

	[[nodiscard]] const TArray<FGSRegisterWrite>& GetWrites() const
	{
		return Writes;
	}
	[[nodiscard]] int32 GetNumImages() const
	{
		return Images.Num();
	}
	/** Image Index's pixels, as an HWREG write names it. */
	[[nodiscard]] TArrayView<const uint8> GetImage(int32 Index) const
	{
		return TArrayView<const uint8>(Images[Index].Data, Images[Index].NumBytes);
	}
	/** Whether image Index is held in place (UploadImageInPlace) rather than copied. */
	[[nodiscard]] bool IsImageInPlace(int32 Index) const
	{
		return Images[Index].CopyIndex == INDEX_NONE;
	}
	[[nodiscard]] const TArray<FGSVertexDraw>& GetVertexDraws() const
	{
		return VertexDraws;
	}
	[[nodiscard]] const TArray<FGSVertexBatch>& GetVertexBatches() const
	{
		return VertexBatches;
	}
	void Reset();

	// The subset every backend reproduces (each part has its GSConformance scene)

	/**
	 * Every primitive type (the manual's seven) but not AA1 (antialiasing needs coverage the emulator does not
	 * emulate) and not FIX (the DDA's fixed fragment values, which no backend models).
	 */
	[[nodiscard]] static bool IsSupported(const FGSPrim& Prim);
	/** The whole blend equation (A - B) * C >> 7 + D: A, B, D in Cs, Cd, 0 and C in As, Ad, FIX (not the reserved 3).
	 */
	[[nodiscard]] static bool IsSupported(const FGSAlpha& Alpha);
	/**
	 * PSMCT32, PSMCT24, PSMCT16, PSMCT16S (a 16-bit frame buffer read back) or a CLUT format (PSMT8, PSMT4) with a
	 * PSMCT32 or PSMCT16 CLUT in CSM1 (CSM2 and TEXCLUT are not emulated); TW, TH <= 10; CLD 0 to 5 (6 and 7 are
	 * reserved).
	 */
	[[nodiscard]] static bool IsSupported(const FGSTex0& Tex0);
	/**
	 * The LOD by the formula or fixed, MXL up to 6, MMAG and MMIN the manual's filters, the MIP levels' base pointers
	 * set by MIPTBP1 / MIPTBP2 (not MTBA's automatic ones).
	 */
	[[nodiscard]] static bool IsSupported(const FGSTex1& Tex1);
	/** Every wrap mode, the region fields within their 10 bits, REGION_CLAMP's MIN not above its MAX. */
	[[nodiscard]] static bool IsSupported(const FGSClamp& Clamp);
	/**
	 * A host to local upload the GS accepts (manual 4.1.5): the width a multiple of 2 (32 bits), 8 (24 bits, 8 and 4
	 * bits) or 4 (16 bits), and the start X a multiple of 2 (8 bits) or 4 (4 bits).
	 */
	[[nodiscard]] static bool IsSupportedUpload(EGSPixelFormat Format, uint16 X, uint16 Width);
	/**
	 * The depth test on (the manual forbids it off); the alpha test with its eight methods and four AFAIL, and the
	 * destination alpha test.
	 */
	[[nodiscard]] static bool IsSupported(const FGSTest& Test);
	/** A color format (PSMCT32, PSMCT24, PSMCT16, PSMCT16S), with any FBMSK. */
	[[nodiscard]] static bool IsSupported(const FGSFrame& Frame);
	/** A Z format. */
	[[nodiscard]] static bool IsSupported(const FGSZBuf& ZBuf);

	/** The register of a context pair: Base (the _1 one) for context 0, the next address for context 1. */
	[[nodiscard]] static EGSRegister ContextRegister(EGSRegister Base, uint8 Context);

private:
	void Write(EGSRegister Register, uint64 Value);
	/** The transfer's register writes and its HWREG, for Pixels' image (already in Images). */
	void WriteUpload(const FGSBitBltBuf& Destination, uint16 X, uint16 Y, uint16 Width, uint16 Height,
		TArrayView<const uint8> Pixels);
	/** Adds an image: a copy of Pixels (bCopy) or a view of them. */
	void AddImage(TArrayView<const uint8> Pixels, bool bCopy);

	/** An image's pixels: in Copies[CopyIndex], or held in place (INDEX_NONE). */
	struct FImage
	{
		const uint8* Data = nullptr;
		int32 NumBytes = 0;
		int32 CopyIndex = INDEX_NONE;
	};

	TArray<FGSRegisterWrite> Writes;
	TArray<FImage> Images;
	/** The copied images' bytes (each its own allocation, so Images' pointers survive the array's growth). */
	TArray<TArray<uint8>> Copies;
	TArray<FGSVertexDraw> VertexDraws;
	TArray<FGSVertexBatch> VertexBatches;
	/**
	 * The skinned batches' palettes: blocks of PaletteBlockBytes (each its own allocation, so the palettes stay where
	 * they are while the list grows), the first NumPaletteBlocks in use, PaletteBlockUsed bytes of the last one.
	 */
	static constexpr uint32 PaletteBlockBytes = 16 * 1024;
	TArray<TArray<uint8>> PaletteBlocks;
	int32 NumPaletteBlocks = 0;
	uint32 PaletteBlockUsed = 0;
};
