#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSLocalMemory.h"
#include "GSTexelDecoder.h"
#include "GSTypes.h"
#include "Shader.h"

/**
 * The GS on OpenGL (Docs/PLANS/ps2-gs-parity.md P4, Docs/PLANS/ps2-shipping.md N8): the desktop preview executes the
 * same FGSCommandList the PS2 sends to its GIF, into a 640 x 448 frame it then shows scaled by a whole number, nearest
 * (D4). Parity comes from following the reference rasterizer (the oracle) rule by rule:
 *
 * - Primitives are assembled on the CPU as the GS does (the vertex queue of each type, XYZ3 / XYZF3 advancing it
 *   without a drawing kick, flat colour from the kicking vertex, a sprite's Z, fog, colour and Q from its second
 *   vertex). Triangles and sprites are drawn in window coordinates, interpolated linearly in screen space; a pixel is
 *   covered as by the GS's top-left rule (samples at the pixel center plus 1/256). Lines and points are stepped on the
 *   CPU exactly as the reference steps them and drawn as one-pixel points.
 * - Textures are decoded from an emulated local memory (FGSLocalMemory, with the GS's page, block and column layout;
 *   FGSTexelDecoder: the formats, TEXA and the CLUTs with CLD's load control) into one layer per MIPMAP level
 *   (MIPTBP1 / MIPTBP2), covering what the wrap and region modes reach. The shader computes the GS's LOD per pixel from
 *   the interpolated Q (LOD = (log2(1/|Q|) << L) + K, or K), picks MMAG or MMIN, the level (rounded, or the two around
 *   the LOD blended by its fraction) and samples each level as the GS does (the wrap and region modes, point or
 *   bilinear around texel centers at .5, rounded per channel).
 * - The texture functions, TCC, fog, the alpha test, Z (exact, from the fragment) with its four tests, dithering,
 *   COLCLAMP, FBA and the 16-bit packing in the shader. Draws that read the frame buffer (blending with the whole
 *   (A - B) * C >> 7 + D, the destination alpha test, a FBMSK that splits a channel) or whose alpha test's AFAIL still
 *   writes go primitive group by primitive group: a group's primitives cover no pixel twice, its rectangle of the frame
 *   is copied and the shader reads the destination there, so each pixel sees the one its earlier primitives left, as
 *   on the GS; what fails the alpha test draws in a second pass of the same group.
 *
 * Known differences (the tests' tolerance): the texture coordinates, Q, fog and Z are interpolated in floating point
 * by OpenGL, so a bilinear weight, a LOD at a level's border or a shared edge's Z may round the other way.
 */
class FGSOpenGLEmulator
{
public:
	/** The frame the scene renderer draws into (D4). */
	static constexpr int32 FrameWidth = 640;
	static constexpr int32 FrameHeight = 448;

	FGSOpenGLEmulator();
	~FGSOpenGLEmulator();

	FGSOpenGLEmulator(const FGSOpenGLEmulator&) = delete;
	FGSOpenGLEmulator& operator=(const FGSOpenGLEmulator&) = delete;

	/**
	 * The shaders (gs_emulator.vert / .frag, gs_present.vert / .frag in ShaderDirectory), the frame's color and depth
	 * targets and the vertex buffer; needs a current OpenGL context.
	 */
	bool Initialize(const FString& ShaderDirectory);
	/** Reloads the shaders whose files changed (all of them when forced), keeping the old ones on a failure. */
	[[nodiscard]] EShaderReloadResult ReloadShaders(bool bForce);
	void Shutdown();
	[[nodiscard]] bool IsValid() const
	{
		return Program.Valid() && Framebuffer != 0;
	}

	/**
	 * The environment the scene renderer records against: the PS2's (PSMCT16S frame buffer, dithered, PSMZ24), so the
	 * preview shows the console's 16-bit colours.
	 */
	[[nodiscard]] static FGSDrawEnvironment GetDrawEnvironment();
	/** The emulated local memory left for textures after the PS2's two frame buffers and its Z buffer. */
	static void GetTextureArena(uint32& OutFirstBlock, uint32& OutNumBlocks);

	/** Executes the list's writes in order: draws into the frame, uploads into the emulated local memory. */
	void Execute(const FGSCommandList& List);

	/**
	 * Shows the frame in the window's back buffer at DisplayAspectRatio (the TV's; 0: square pixels): its lines
	 * scaled by a whole number, nearest, and each line stretched to the width that aspect gives, linearly, as a TV
	 * draws the analog line; black around it. A 16-bit frame's texels are truncated to 5 bits first.
	 */
	void Present(int32 WindowWidth, int32 WindowHeight, float DisplayAspectRatio);
	/** Where Present puts the frame in a window: its bottom-left corner and size, in window pixels. */
	static void GetPresentRect(int32 WindowWidth, int32 WindowHeight, float DisplayAspectRatio, int32& OutX,
		int32& OutY, int32& OutWidth, int32& OutHeight);

	/**
	 * Width x Height pixels of the frame from its top-left corner as the GS would hold them in the last FRAME's format
	 * (16-bit: 5 bits a channel shifted left 3, the alpha 0x80 or 0), top row first.
	 */
	[[nodiscard]] TArray<FColor> ReadFrame(int32 Width, int32 Height);

	/** The emulated local memory (the tests upload through lists; read-only here). */
	[[nodiscard]] const FGSLocalMemory& GetMemory() const
	{
		return Memory;
	}

private:
	/** The registers of one drawing context. */
	struct FContext
	{
		FGSTex0 Tex0;
		FGSTex1 Tex1;
		FGSClamp Clamp;
		FGSMipTbp MipTbp1;
		FGSMipTbp MipTbp2;
		FGSXYOffset XYOffset;
		FGSScissor Scissor;
		FGSAlpha Alpha;
		FGSTest Test;
		bool bFba = false;
		FGSFrame Frame;
		FGSZBuf ZBuf;
	};

	/** A vertex as the GS latches it: window coordinates in sixteenths, then its attributes. */
	struct FGSVertex
	{
		int32 X = 0;
		int32 Y = 0;
		uint32 Z = 0;
		uint8 F = 0;
		FGSRGBAQ Color;
		FGSST ST;
		FGSUV UV;
	};

	/** A vertex of an OpenGL batch (13 floats). */
	struct FBatchVertex
	{
		/** Window pixel coordinates, depth in 0..1 and fog 0..255. */
		float X, Y, Depth, Fog;
		/** 0..255. */
		float R, G, B, A;
		float S, T, Q;
		/** Texels. */
		float U, V;
	};

	/** One primitive of the pending batch: its vertices and what it may cover, for the groups of Flush. */
	struct FBatchPrimitive
	{
		int32 FirstVertex = 0;
		int32 NumVertices = 0;
		/** The pixels it may cover, both corners included. */
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = -1;
		int32 MaxY = -1;
		/** A triangle's or sprite's corners in sixteenths of a pixel (0 for lines and points: their pixels only). */
		int32 NumCorners = 0;
		int32 CornerX[4] = {};
		int32 CornerY[4] = {};
	};

	/** The local memory blocks [FirstBlock, EndBlock) a texture was decoded from (EndBlock may pass the end). */
	struct FBlockRange
	{
		uint32 FirstBlock = 0;
		uint32 EndBlock = 0;
	};

	/** A decoded texture: a layer per MIPMAP level, for invalidation by uploads. */
	struct FTextureEntry
	{
		uint32 Texture = 0;
		TArray<FBlockRange> Ranges;
	};

	/** The levels a draw samples: how many, and each one's size (the wrap modes' size, not the decoded layer's). */
	struct FTextureLevels
	{
		int32 NumLevels = 1;
		int32 Width[7] = {};
		int32 Height[7] = {};
	};

	void WriteRegister(const FGSRegisterWrite& Write, const FGSCommandList& List);
	void AddVertex(uint16 X, uint16 Y, uint32 Z, uint8 F, bool bKick);
	void AddTriangle(const FGSVertex& V0, const FGSVertex& V1, const FGSVertex& V2);
	void AddSprite(const FGSVertex& V0, const FGSVertex& V1);
	void AddLine(const FGSVertex& From, const FGSVertex& To);
	void AddPoint(const FGSVertex& Vertex);
	/** A one-pixel fragment the CPU stepped (lines and points), as a point of the batch's current primitive. */
	void AddPixel(int32 X, int32 Y, double Z, double F, double R, double G, double B, double A, double S, double T,
		double Q, double U, double V);
	/** Starts a primitive of the batch in Mode; its vertices follow. */
	FBatchPrimitive& BeginPrimitive(uint32 Mode);
	[[nodiscard]] FBatchVertex ToBatchVertex(const FGSVertex& Vertex) const;

	/** Draws the pending batch with the current state. */
	void Flush();
	/** Whether Primitive may cover a pixel one of Batch primitives [First, End) covers. */
	[[nodiscard]] bool Overlaps(int32 First, int32 End, const FBatchPrimitive& Primitive) const;
	/** The texture the current draw samples (decoded and cached), or 0; OutLevels describes its levels. */
	[[nodiscard]] uint32 BindTexture(const FContext& Context, FTextureLevels& OutLevels);
	/** Forgets the decoded textures that read the blocks [FirstBlock, EndBlock) (wrapping at the end of memory). */
	void InvalidateTextures(uint32 FirstBlock, uint32 EndBlock);
	/** Drops the texture cache. */
	void ClearTextures();

	[[nodiscard]] const FContext& GetContext() const
	{
		return Contexts[Prim.Context];
	}

	FShader Program;
	FShader PresentProgram;
	uint32 Framebuffer = 0;
	uint32 ColorTexture = 0;
	uint32 DepthTexture = 0;
	/** A copy of the frame's colour, which the shader reads as the destination. */
	uint32 DestinationTexture = 0;
	uint32 VertexArray = 0;
	uint32 VertexBuffer = 0;
	uint32 PresentVertexArray = 0;

	FGSLocalMemory Memory;
	FContext Contexts[2];
	FGSPrim Prim;
	FGSRGBAQ RGBAQ;
	FGSST ST;
	FGSUV UV;
	FGSFog Fog;
	FGSFogCol FogCol;
	FGSDimx Dimx;
	bool bDither = false;
	bool bColorClamp = true;
	bool bPixelAlphaBlend = false;
	FGSTexA TexA;
	FGSBitBltBuf BitBltBuf;
	FGSTrxPos TrxPos;
	FGSTrxReg TrxReg;
	FGSClutBuffer Clut;
	/** A hash of the CLUT buffer, part of a CLUT texture's cache key. */
	uint32 ClutHash = 0;

	TArray<FGSVertex> Queue;
	FGSVertex FanFirst;
	int32 NumVertices = 0;

	/** The pending batch: triangles or points, drawn with the state at Flush. */
	TArray<FBatchVertex> Batch;
	TArray<FBatchPrimitive> BatchPrimitives;
	uint32 BatchMode = 0;

	TMap<FString, FTextureEntry> Textures;
	/** The last frame format drawn to, for ReadFrame and Present. */
	EGSPixelFormat FrameFormat = EGSPixelFormat::PSMCT16S;
};
