#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSLocalMemory.h"
#include "GSTexelDecoder.h"
#include "GSTypes.h"
#include "Shader.h"

/**
 * The GS on OpenGL (Docs/PLANS/ps2-gs-parity.md P4): the desktop preview executes the same FGSCommandList the PS2 sends
 * to its GIF, into a 640 x 448 frame it then shows scaled by a whole number, nearest (D4). Parity comes from following
 * the reference rasterizer (the oracle) rule by rule:
 *
 * - Primitives are assembled on the CPU as the GS does (the vertex queue of each type, XYZ3 without a kick, flat colour
 *   from the kicking vertex, a sprite's Z, fog and colour from its second vertex) and drawn in window coordinates,
 *   interpolated linearly in screen space; a pixel is covered as by the GS's top-left rule (samples at the pixel
 *   center plus 1/256).
 * - Textures are decoded from an emulated local memory (FGSLocalMemory, FGSTexelDecoder: the formats, TEXA and the
 *   CLUTs) level by level, and the shader samples them as the GS does (the wrap and region modes, point or bilinear
 *   around texel centers at .5, rounded per channel); a MIPMAP level is picked on the CPU for a fixed LOD (LCM = 1),
 *   level 0 otherwise.
 * - The texture functions, TCC, fog, the alpha test with every AFAIL (a second pass draws what fails), Z (exact, from
 *   the fragment) with its four tests, FBA, and blending by dual-source blend factors for the subset FGSCommandList
 *   accepts (C = As or FIX).
 * - A 16-bit frame is dithered (DIMX) and truncated to 5 bits per channel in the shader when a draw does not blend;
 *   blended pixels are truncated when the frame is read or shown.
 *
 * Known differences (the tests' tolerance): blending rounds in floating point instead of (A - B) * C >> 7 and clamps
 * the factor at 1.0 (As above 0x80), blended pixels of a 16-bit frame are not dithered (one 5-bit step), COLCLAMP off
 * (wrapping) is not emulated, PABE with additive blending blends, lines and points follow OpenGL's rules, and the
 * texture coordinates and Z are interpolated in floating point (a bilinear weight or a shared edge's Z may round the
 * other way).
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

	/** Shows the frame in the bound window's framebuffer, scaled by the largest whole number that fits, centered. */
	void Present(int32 WindowWidth, int32 WindowHeight);

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

	/** A decoded texture level the shader samples. */
	struct FTextureEntry
	{
		uint32 Texture = 0;
		/** The local memory words it was decoded from, for invalidation by uploads. */
		uint32 FirstWord = 0;
		uint32 EndWord = 0;
	};

	void WriteRegister(const FGSRegisterWrite& Write, const FGSCommandList& List);
	void AddVertex(uint16 X, uint16 Y, uint32 Z, uint8 F, bool bKick);
	void AddTriangle(const FGSVertex& V0, const FGSVertex& V1, const FGSVertex& V2);
	void AddSprite(const FGSVertex& V0, const FGSVertex& V1);
	void AddLine(const FGSVertex& From, const FGSVertex& To);
	void AddPoint(const FGSVertex& Vertex);
	[[nodiscard]] FBatchVertex ToBatchVertex(const FGSVertex& Vertex) const;

	/** Draws the pending batch with the current state. */
	void Flush();
	/** The texture level the current draw samples (decoded and cached), or 0; OutLevel and OutLevelSize describe it. */
	[[nodiscard]] uint32 BindTexture(const FContext& Context, uint32& OutLevel, int32& OutWidth, int32& OutHeight);
	/** Forgets the decoded textures that read the words [FirstWord, EndWord). */
	void InvalidateTextures(uint32 FirstWord, uint32 EndWord);
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

	/** The pending batch: triangles, lines or points, drawn with the state at Flush. */
	TArray<FBatchVertex> Batch;
	uint32 BatchMode = 0;

	TMap<FString, FTextureEntry> Textures;
	/** The last frame format drawn to, for ReadFrame and Present. */
	EGSPixelFormat FrameFormat = EGSPixelFormat::PSMCT16S;
};
