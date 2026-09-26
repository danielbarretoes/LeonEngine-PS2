#include "GSOpenGLEmulator.h"

#include "Misc/Paths.h"
#include "RendererLog.h"

#include <glad/glad.h>

namespace
{

	[[nodiscard]] uint32 MaxZ(EGSPixelFormat Format)
	{
		switch (Format)
		{
			case EGSPixelFormat::PSMZ24:
				return 0xffffffu;
			case EGSPixelFormat::PSMZ16:
			case EGSPixelFormat::PSMZ16S:
				return 0xffffu;
			default:
				return 0xffffffffu;
		}
	}

	[[nodiscard]] bool IsColor16(EGSPixelFormat Format)
	{
		return Format == EGSPixelFormat::PSMCT16 || Format == EGSPixelFormat::PSMCT16S;
	}

	[[nodiscard]] bool IsClutFormat(EGSPixelFormat Format)
	{
		return Format == EGSPixelFormat::PSMT8 || Format == EGSPixelFormat::PSMT4;
	}

	[[nodiscard]] uint32 FloatBitsHash(const uint32* Values, int32 Count)
	{
		uint32 Hash = 2166136261u;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Hash = (Hash ^ Values[Index]) * 16777619u;
		}
		return Hash;
	}

	/** The registers whose writes do not change the draw state (the vertex's attributes and kicks). */
	[[nodiscard]] bool IsVertexRegister(EGSRegister Register)
	{
		switch (Register)
		{
			case EGSRegister::RGBAQ:
			case EGSRegister::ST:
			case EGSRegister::UV:
			case EGSRegister::FOG:
			case EGSRegister::XYZ2:
			case EGSRegister::XYZF2:
			case EGSRegister::XYZ3:
			case EGSRegister::XYZF3:
				return true;
			default:
				return false;
		}
	}

} // namespace

FGSOpenGLEmulator::FGSOpenGLEmulator() = default;

FGSOpenGLEmulator::~FGSOpenGLEmulator()
{
	Shutdown();
}

bool FGSOpenGLEmulator::Initialize(const FString& ShaderDirectory)
{
	if (!Program.LoadFromFiles(FPaths::Combine(ShaderDirectory, TEXT("gs_emulator.vert")),
			FPaths::Combine(ShaderDirectory, TEXT("gs_emulator.frag"))) ||
		!PresentProgram.LoadFromFiles(FPaths::Combine(ShaderDirectory, TEXT("gs_present.vert")),
			FPaths::Combine(ShaderDirectory, TEXT("gs_present.frag"))))
	{
		UE_LOG(LogRenderer, Error, "GS emulator: the shaders in %s do not compile", *ShaderDirectory);
		Shutdown();
		return false;
	}

	glGenTextures(1, &ColorTexture);
	glBindTexture(GL_TEXTURE_2D, ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FrameWidth, FrameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenTextures(1, &DepthTexture);
	glBindTexture(GL_TEXTURE_2D, DepthTexture);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, FrameWidth, FrameHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &Framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);
	const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (Status != GL_FRAMEBUFFER_COMPLETE)
	{
		UE_LOG(LogRenderer, Error, "GS emulator: the frame's framebuffer is incomplete (0x%x)", uint32(Status));
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		Shutdown();
		return false;
	}
	glViewport(0, 0, FrameWidth, FrameHeight);
	glDisable(GL_SCISSOR_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenVertexArrays(1, &VertexArray);
	glGenBuffers(1, &VertexBuffer);
	glBindVertexArray(VertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
	constexpr GLsizei Stride = sizeof(FBatchVertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<const void*>(offsetof(FBatchVertex, X)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<const void*>(offsetof(FBatchVertex, R)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<const void*>(offsetof(FBatchVertex, S)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<const void*>(offsetof(FBatchVertex, U)));
	glBindVertexArray(0);
	glGenVertexArrays(1, &PresentVertexArray);
	return true;
}

EShaderReloadResult FGSOpenGLEmulator::ReloadShaders(bool bForce)
{
	const auto Reload = [bForce](FShader& Shader)
	{ return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged(); };
	return MergeShaderReload(Reload(Program), Reload(PresentProgram));
}

void FGSOpenGLEmulator::Shutdown()
{
	ClearTextures();
	if (Framebuffer != 0)
	{
		glDeleteFramebuffers(1, &Framebuffer);
		Framebuffer = 0;
	}
	for (uint32* Texture : {&ColorTexture, &DepthTexture})
	{
		if (*Texture != 0)
		{
			glDeleteTextures(1, Texture);
			*Texture = 0;
		}
	}
	if (VertexBuffer != 0)
	{
		glDeleteBuffers(1, &VertexBuffer);
		VertexBuffer = 0;
	}
	for (uint32* Array : {&VertexArray, &PresentVertexArray})
	{
		if (*Array != 0)
		{
			glDeleteVertexArrays(1, Array);
			*Array = 0;
		}
	}
	Program.Destroy();
	PresentProgram.Destroy();
}

FGSDrawEnvironment FGSOpenGLEmulator::GetDrawEnvironment()
{
	// The PS2's first frame buffer (70 pages of PSMCT16S) and its Z buffer after both frame buffers.
	FGSDrawEnvironment Environment;
	Environment.Frame.FBP = 0;
	Environment.Frame.FBW = FrameWidth / 64;
	Environment.Frame.PSM = EGSPixelFormat::PSMCT16S;
	Environment.ZBuf.ZBP = 140;
	Environment.ZBuf.PSM = EGSPixelFormat::PSMZ24;
	Environment.Width = FrameWidth;
	Environment.Height = FrameHeight;
	return Environment;
}

void FGSOpenGLEmulator::GetTextureArena(uint32& OutFirstBlock, uint32& OutNumBlocks)
{
	// After the Z buffer (pages 140 to 279), as on the PS2: pages 280 to 511.
	OutFirstBlock = FGSDrawEnvironment::TextureArenaFirstBlock;
	OutNumBlocks = FGSDrawEnvironment::TextureArenaBlocks;
}

void FGSOpenGLEmulator::Execute(const FGSCommandList& List)
{
	for (const FGSRegisterWrite& Write : List.GetWrites())
	{
		WriteRegister(Write, List);
	}
	Flush();
}

void FGSOpenGLEmulator::WriteRegister(const FGSRegisterWrite& Write, const FGSCommandList& List)
{
	const uint64 Value = Write.Value;
	if (!IsVertexRegister(Write.Register))
	{
		// Everything else may change how the pending batch draws.
		Flush();
	}
	switch (Write.Register)
	{
		case EGSRegister::PRIM:
			Prim = FGSPrim::Decode(Value);
			Queue.Reset();
			NumVertices = 0;
			break;
		case EGSRegister::RGBAQ:
			RGBAQ = FGSRGBAQ::Decode(Value);
			break;
		case EGSRegister::ST:
			ST = FGSST::Decode(Value);
			break;
		case EGSRegister::UV:
			UV = FGSUV::Decode(Value);
			break;
		case EGSRegister::FOG:
			Fog = FGSFog::Decode(Value);
			break;
		case EGSRegister::XYZ2:
		case EGSRegister::XYZ3:
		{
			const FGSXYZ Vertex = FGSXYZ::Decode(Value);
			AddVertex(Vertex.X, Vertex.Y, Vertex.Z, Fog.F, Write.Register == EGSRegister::XYZ2);
			break;
		}
		case EGSRegister::XYZF2:
		case EGSRegister::XYZF3:
		{
			const FGSXYZF Vertex = FGSXYZF::Decode(Value);
			AddVertex(Vertex.X, Vertex.Y, Vertex.Z, Vertex.F, Write.Register == EGSRegister::XYZF2);
			break;
		}
		case EGSRegister::TEX0_1:
		case EGSRegister::TEX0_2:
		{
			const FGSTex0 Tex0 = FGSTex0::Decode(Value);
			Contexts[uint8(Write.Register) - uint8(EGSRegister::TEX0_1)].Tex0 = Tex0;
			if (Tex0.CLD == 1 && IsClutFormat(Tex0.PSM))
			{
				Clut.Load(Memory, Tex0);
				ClutHash = FloatBitsHash(Clut.Entries, 512);
			}
			break;
		}
		case EGSRegister::TEX1_1:
		case EGSRegister::TEX1_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::TEX1_1)].Tex1 = FGSTex1::Decode(Value);
			break;
		case EGSRegister::CLAMP_1:
		case EGSRegister::CLAMP_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::CLAMP_1)].Clamp = FGSClamp::Decode(Value);
			break;
		case EGSRegister::MIPTBP1_1:
		case EGSRegister::MIPTBP1_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::MIPTBP1_1)].MipTbp1 = FGSMipTbp::Decode(Value);
			break;
		case EGSRegister::MIPTBP2_1:
		case EGSRegister::MIPTBP2_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::MIPTBP2_1)].MipTbp2 = FGSMipTbp::Decode(Value);
			break;
		case EGSRegister::XYOFFSET_1:
		case EGSRegister::XYOFFSET_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::XYOFFSET_1)].XYOffset = FGSXYOffset::Decode(Value);
			break;
		case EGSRegister::SCISSOR_1:
		case EGSRegister::SCISSOR_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::SCISSOR_1)].Scissor = FGSScissor::Decode(Value);
			break;
		case EGSRegister::ALPHA_1:
		case EGSRegister::ALPHA_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::ALPHA_1)].Alpha = FGSAlpha::Decode(Value);
			break;
		case EGSRegister::TEST_1:
		case EGSRegister::TEST_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::TEST_1)].Test = FGSTest::Decode(Value);
			break;
		case EGSRegister::FBA_1:
		case EGSRegister::FBA_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::FBA_1)].bFba = (Value & 1) != 0;
			break;
		case EGSRegister::FRAME_1:
		case EGSRegister::FRAME_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::FRAME_1)].Frame = FGSFrame::Decode(Value);
			break;
		case EGSRegister::ZBUF_1:
		case EGSRegister::ZBUF_2:
			Contexts[uint8(Write.Register) - uint8(EGSRegister::ZBUF_1)].ZBuf = FGSZBuf::Decode(Value);
			break;
		case EGSRegister::FOGCOL:
			FogCol = FGSFogCol::Decode(Value);
			break;
		case EGSRegister::DIMX:
			Dimx = FGSDimx::Decode(Value);
			break;
		case EGSRegister::DTHE:
			bDither = (Value & 1) != 0;
			break;
		case EGSRegister::COLCLAMP:
			bColorClamp = (Value & 1) != 0;
			break;
		case EGSRegister::PABE:
			bPixelAlphaBlend = (Value & 1) != 0;
			break;
		case EGSRegister::TEXA:
			TexA = FGSTexA::Decode(Value);
			break;
		case EGSRegister::BITBLTBUF:
			BitBltBuf = FGSBitBltBuf::Decode(Value);
			break;
		case EGSRegister::TRXPOS:
			TrxPos = FGSTrxPos::Decode(Value);
			break;
		case EGSRegister::TRXREG:
			TrxReg = FGSTrxReg::Decode(Value);
			break;
		case EGSRegister::HWREG:
		{
			Memory.Transfer(BitBltBuf, TrxPos, TrxReg, List.GetImageData()[int32(Value)]);
			const uint32 Bits = FGSLocalMemory::StorageBits(BitBltBuf.DPSM);
			const uint32 FirstWord = uint32(BitBltBuf.DBP) * 64;
			const uint64 Pixels = uint64(BitBltBuf.DBW) * 64 * (uint64(TrxPos.DSAY) + TrxReg.RRH);
			InvalidateTextures(FirstWord, FirstWord + uint32((Pixels * Bits + 31) / 32));
			break;
		}
		default:
			// PRMODECONT, TEXCLUT, TEXFLUSH, TRXDIR and FINISH change nothing the emulator draws.
			break;
	}
}

void FGSOpenGLEmulator::AddVertex(uint16 X, uint16 Y, uint32 Z, uint8 F, bool bKick)
{
	const FContext& Context = GetContext();
	FGSVertex Vertex;
	Vertex.X = int32(X) - int32(Context.XYOffset.OFX);
	Vertex.Y = int32(Y) - int32(Context.XYOffset.OFY);
	Vertex.Z = Z;
	Vertex.F = F;
	Vertex.Color = RGBAQ;
	Vertex.ST = ST;
	Vertex.UV = UV;
	++NumVertices;

	switch (Prim.Type)
	{
		case EGSPrimitive::Point:
			if (bKick)
			{
				AddPoint(Vertex);
			}
			break;
		case EGSPrimitive::Line:
		case EGSPrimitive::Sprite:
			Queue.Add(Vertex);
			if (Queue.Num() == 2)
			{
				if (bKick)
				{
					Prim.Type == EGSPrimitive::Line ? AddLine(Queue[0], Queue[1]) : AddSprite(Queue[0], Queue[1]);
				}
				Queue.Reset();
			}
			break;
		case EGSPrimitive::LineStrip:
			Queue.Add(Vertex);
			if (Queue.Num() == 2)
			{
				if (bKick)
				{
					AddLine(Queue[0], Queue[1]);
				}
				Queue.RemoveAt(0);
			}
			break;
		case EGSPrimitive::Triangle:
			Queue.Add(Vertex);
			if (Queue.Num() == 3)
			{
				if (bKick)
				{
					AddTriangle(Queue[0], Queue[1], Queue[2]);
				}
				Queue.Reset();
			}
			break;
		case EGSPrimitive::TriangleStrip:
			Queue.Add(Vertex);
			if (Queue.Num() == 3)
			{
				if (bKick)
				{
					AddTriangle(Queue[0], Queue[1], Queue[2]);
				}
				Queue.RemoveAt(0);
			}
			break;
		case EGSPrimitive::TriangleFan:
			if (NumVertices == 1)
			{
				FanFirst = Vertex;
				break;
			}
			Queue.Add(Vertex);
			if (Queue.Num() == 2)
			{
				if (bKick)
				{
					AddTriangle(FanFirst, Queue[0], Queue[1]);
				}
				Queue.RemoveAt(0);
			}
			break;
	}
}

FGSOpenGLEmulator::FBatchVertex FGSOpenGLEmulator::ToBatchVertex(const FGSVertex& Vertex) const
{
	const uint32 ZMax = MaxZ(GetContext().ZBuf.PSM);
	FBatchVertex Out;
	Out.X = float(Vertex.X) / 16.0f;
	Out.Y = float(Vertex.Y) / 16.0f;
	Out.Depth = float(double(FMath::Min(Vertex.Z, ZMax)) / double(ZMax));
	Out.Fog = float(Vertex.F);
	Out.R = float(Vertex.Color.R);
	Out.G = float(Vertex.Color.G);
	Out.B = float(Vertex.Color.B);
	Out.A = float(Vertex.Color.A);
	Out.S = Vertex.ST.S;
	Out.T = Vertex.ST.T;
	Out.Q = Vertex.Color.Q;
	Out.U = float(Vertex.UV.U) / 16.0f;
	Out.V = float(Vertex.UV.V) / 16.0f;
	return Out;
}

void FGSOpenGLEmulator::AddTriangle(const FGSVertex& V0, const FGSVertex& V1, const FGSVertex& V2)
{
	// The kicking vertex comes last: OpenGL's provoking vertex, the flat colour's (manual 3.2).
	BatchMode = GL_TRIANGLES;
	Batch.Add(ToBatchVertex(V0));
	Batch.Add(ToBatchVertex(V1));
	Batch.Add(ToBatchVertex(V2));
}

void FGSOpenGLEmulator::AddSprite(const FGSVertex& V0, const FGSVertex& V1)
{
	// Z, fog and colour are the second vertex's; the texture coordinates span the rectangle (manual 3.2.8).
	const FBatchVertex First = ToBatchVertex(V0);
	const FBatchVertex Second = ToBatchVertex(V1);
	const auto Corner = [&First, &Second](bool bRight, bool bBottom)
	{
		FBatchVertex Out = Second;
		Out.X = bRight ? Second.X : First.X;
		Out.Y = bBottom ? Second.Y : First.Y;
		Out.S = bRight ? Second.S : First.S;
		Out.T = bBottom ? Second.T : First.T;
		Out.U = bRight ? Second.U : First.U;
		Out.V = bBottom ? Second.V : First.V;
		return Out;
	};
	BatchMode = GL_TRIANGLES;
	const FBatchVertex TopLeft = Corner(false, false);
	const FBatchVertex TopRight = Corner(true, false);
	const FBatchVertex BottomLeft = Corner(false, true);
	const FBatchVertex BottomRight = Corner(true, true);
	Batch.Append({TopLeft, TopRight, BottomRight, TopLeft, BottomRight, BottomLeft});
}

void FGSOpenGLEmulator::AddLine(const FGSVertex& From, const FGSVertex& To)
{
	BatchMode = GL_LINES;
	Batch.Add(ToBatchVertex(From));
	Batch.Add(ToBatchVertex(To));
}

void FGSOpenGLEmulator::AddPoint(const FGSVertex& Vertex)
{
	BatchMode = GL_POINTS;
	Batch.Add(ToBatchVertex(Vertex));
}

void FGSOpenGLEmulator::Flush()
{
	if (Batch.Num() == 0)
	{
		return;
	}
	const FContext& Context = GetContext();
	FrameFormat = Context.Frame.PSM;
	glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
	glViewport(0, 0, FrameWidth, FrameHeight);
	Program.Bind();
	Program.SetVec2("uTargetSize", float(FrameWidth), float(FrameHeight));
	Program.SetInt("uTargetHeight", FrameHeight);
	Program.SetInt("uGouraud", Prim.bGouraud ? 1 : 0);
	Program.SetInt("uFog", Prim.bFog ? 1 : 0);
	Program.SetIVec3("uFogColor", FogCol.R, FogCol.G, FogCol.B);

	// The texture.
	uint32 Texture = 0;
	uint32 Level = 0;
	int32 LevelWidth = 1;
	int32 LevelHeight = 1;
	if (Prim.bTextured)
	{
		Texture = BindTexture(Context, Level, LevelWidth, LevelHeight);
	}
	Program.SetInt("uTextured", Texture != 0 ? 1 : 0);
	if (Texture != 0)
	{
		const FGSTex0& Tex0 = Context.Tex0;
		const FGSTex1& Tex1 = Context.Tex1;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		Program.SetInt("uTexture", 0);
		Program.SetInt("uUseUV", Prim.bUseUV ? 1 : 0);
		Program.SetInt("uTFX", int32(Tex0.TFX));
		Program.SetInt("uTCC", Tex0.bRGBA ? 1 : 0);
		Program.SetIVec2("uBaseSize", 1 << Tex0.TW, 1 << Tex0.TH);
		Program.SetIVec2("uLevelSize", LevelWidth, LevelHeight);
		Program.SetInt("uLevel", int32(Level));
		// The level's filter: MMAG under LOD 0, MMIN's filter within the level otherwise (manual 3.4.11).
		const double Lod = Tex1.bFixedLOD ? double(Tex1.K) / 16.0 : 0.0;
		const EGSFilter Filter = Lod <= 0.0 ? Tex1.MMAG : Tex1.MMIN;
		const bool bBilinear = Filter == EGSFilter::Linear || Filter == EGSFilter::LinearMipmapNearest ||
			Filter == EGSFilter::LinearMipmapLinear;
		Program.SetInt("uBilinear", bBilinear ? 1 : 0);
		Program.SetIVec2("uWrap", int32(Context.Clamp.WMS), int32(Context.Clamp.WMT));
		Program.SetIVec4("uRegion", Context.Clamp.MINU, Context.Clamp.MAXU, Context.Clamp.MINV, Context.Clamp.MAXV);
	}

	// The frame buffer's format and dithering, FBA.
	const bool bFrame16 = IsColor16(Context.Frame.PSM);
	Program.SetInt("uFrame16", bFrame16 ? 1 : 0);
	Program.SetInt("uDither", bDither ? 1 : 0);
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			const FString Name = FString::Printf("uDimx[%d]", (Row * 4) + Column);
			Program.SetInt(*Name, Dimx.M[Row][Column]);
		}
	}
	Program.SetInt("uFBA", Context.bFba ? 1 : 0);

	// Blending: (Cs - B) * C + D by dual-source factors (C = As or FIX), the alpha written as it is.
	const FGSAlpha& Alpha = Context.Alpha;
	Program.SetInt("uBlend", Prim.bAlphaBlend ? 1 : 0);
	Program.SetInt("uBlendFixed", Alpha.C == EGSBlendAlpha::Fixed ? 1 : 0);
	Program.SetInt("uFix", Alpha.FIX);
	Program.SetInt("uPABE", bPixelAlphaBlend ? 1 : 0);
	if (Prim.bAlphaBlend)
	{
		const GLenum Destination = Alpha.B == EGSBlendColor::Destination ? GL_ONE_MINUS_SRC1_ALPHA
			: Alpha.D == EGSBlendColor::Destination                      ? GL_ONE
																		 : GL_ZERO;
		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC1_ALPHA, Destination, GL_ONE, GL_ZERO);
	}
	else
	{
		glDisable(GL_BLEND);
	}

	// The depth test and write.
	const FGSTest& Test = Context.Test;
	glEnable(GL_DEPTH_TEST);
	static constexpr GLenum DepthFunctions[4] = {GL_NEVER, GL_ALWAYS, GL_GEQUAL, GL_GREATER};
	glDepthFunc(DepthFunctions[int32(Test.ZTST) & 3]);
	const bool bWriteZ = !Context.ZBuf.bMask;

	// The frame buffer mask, channel by channel (a channel is kept when its mask bits are all set).
	const uint32 Mask = Context.Frame.FBMSK;
	const bool bWriteChannel[4] = {(Mask & 0xffu) != 0xffu, (Mask & 0xff00u) != 0xff00u,
		(Mask & 0xff0000u) != 0xff0000u, (Mask & 0xff000000u) != 0xff000000u};

	// The scissor, inclusive (the frame's top row is the target's top).
	const FGSScissor& Scissor = Context.Scissor;
	glEnable(GL_SCISSOR_TEST);
	const int32 ScissorWidth = FMath::Max(0, int32(Scissor.SCAX1) - int32(Scissor.SCAX0) + 1);
	const int32 ScissorHeight = FMath::Max(0, int32(Scissor.SCAY1) - int32(Scissor.SCAY0) + 1);
	glScissor(Scissor.SCAX0, FrameHeight - 1 - int32(Scissor.SCAY1), ScissorWidth, ScissorHeight);

	glBindVertexArray(VertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(Batch.Num() * sizeof(FBatchVertex)), Batch.GetData(), GL_STREAM_DRAW);

	// First what passes the alpha test (everything without one), then what fails it and AFAIL still writes.
	Program.SetInt("uAlphaTest", Test.bAlphaTest ? 1 : 0);
	Program.SetInt("uAlphaTestMode", int32(Test.ATST));
	Program.SetInt("uAlphaRef", Test.AREF);
	Program.SetInt("uAlphaPass", 0);
	glColorMask(bWriteChannel[0], bWriteChannel[1], bWriteChannel[2], bWriteChannel[3]);
	glDepthMask(bWriteZ ? GL_TRUE : GL_FALSE);
	glDrawArrays(BatchMode, 0, GLsizei(Batch.Num()));
	if (Test.bAlphaTest && Test.AFAIL != EGSAlphaFail::Keep)
	{
		Program.SetInt("uAlphaPass", 1);
		const bool bColor = Test.AFAIL != EGSAlphaFail::ZBufferOnly;
		const bool bAlphaChannel =
			Test.AFAIL == EGSAlphaFail::FrameBufferOnly || (Test.AFAIL == EGSAlphaFail::RGBOnly && bFrame16);
		glColorMask(bColor && bWriteChannel[0], bColor && bWriteChannel[1], bColor && bWriteChannel[2],
			bAlphaChannel && bWriteChannel[3]);
		glDepthMask(Test.AFAIL == EGSAlphaFail::ZBufferOnly && bWriteZ ? GL_TRUE : GL_FALSE);
		glDrawArrays(BatchMode, 0, GLsizei(Batch.Num()));
	}
	glBindVertexArray(0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	Batch.Reset();
}

uint32 FGSOpenGLEmulator::BindTexture(const FContext& Context, uint32& OutLevel, int32& OutWidth, int32& OutHeight)
{
	const FGSTex0& Tex0 = Context.Tex0;
	const FGSTex1& Tex1 = Context.Tex1;
	// The level: a fixed LOD's (LCM = 1), rounded for the MIPMAP_NEAREST filters; level 0 otherwise.
	uint32 Level = 0;
	if (Tex1.bFixedLOD && Tex1.K > 0)
	{
		const double Lod = double(Tex1.K) / 16.0;
		const bool bNearestLevel =
			Tex1.MMIN == EGSFilter::NearestMipmapNearest || Tex1.MMIN == EGSFilter::LinearMipmapNearest;
		const bool bLinearLevels =
			Tex1.MMIN == EGSFilter::NearestMipmapLinear || Tex1.MMIN == EGSFilter::LinearMipmapLinear;
		if (bNearestLevel || bLinearLevels)
		{
			Level = FMath::Min(
				uint32(FMath::FloorToInt(float(bNearestLevel ? Lod + 0.5 : Lod))), FMath::Min<uint32>(Tex1.MXL, 6));
		}
	}
	uint32 BasePointer = Tex0.TBP0;
	uint32 BufferWidth = Tex0.TBW;
	if (Level >= 1 && Level <= 3)
	{
		BasePointer = Context.MipTbp1.TBP[Level - 1];
		BufferWidth = Context.MipTbp1.TBW[Level - 1];
	}
	else if (Level >= 4)
	{
		BasePointer = Context.MipTbp2.TBP[Level - 4];
		BufferWidth = Context.MipTbp2.TBW[Level - 4];
	}
	const int32 Width = FMath::Max(1, (1 << Tex0.TW) >> Level);
	const int32 Height = FMath::Max(1, (1 << Tex0.TH) >> Level);
	OutLevel = Level;
	OutWidth = Width;
	OutHeight = Height;

	// The decoded level, by everything its texels depend on (CLD and the load cache are not part of it).
	FGSTex0 KeyTex0 = Tex0;
	KeyTex0.CLD = 0;
	KeyTex0.TFX = EGSTextureFunction::Modulate;
	KeyTex0.bRGBA = true;
	const bool bClut = IsClutFormat(Tex0.PSM);
	const FString Key = FString::Printf("%llx:%u:%u:%u:%llx:%x", (unsigned long long)KeyTex0.Encode(), Level,
		BasePointer, BufferWidth, (unsigned long long)TexA.Encode(), bClut ? ClutHash : 0u);
	if (const FTextureEntry* Entry = Textures.Find(Key))
	{
		return Entry->Texture;
	}
	TArray<uint8> Texels;
	Texels.SetNumUninitialized(Width * Height * 4);
	for (int32 V = 0; V < Height; ++V)
	{
		for (int32 U = 0; U < Width; ++U)
		{
			const FColor Color =
				FGSTexelDecoder::Decode(Memory, Tex0, BasePointer, BufferWidth, TexA, Clut, uint32(U), uint32(V));
			uint8* Texel = &Texels[((V * Width) + U) * 4];
			Texel[0] = Color.R;
			Texel[1] = Color.G;
			Texel[2] = Color.B;
			Texel[3] = Color.A;
		}
	}
	FTextureEntry Entry;
	glGenTextures(1, &Entry.Texture);
	glBindTexture(GL_TEXTURE_2D, Entry.Texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texels.GetData());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	Entry.FirstWord = BasePointer * 64;
	const uint64 Pixels = uint64(BufferWidth) * 64 * uint64(Height);
	Entry.EndWord = Entry.FirstWord + uint32((Pixels * FGSLocalMemory::StorageBits(Tex0.PSM) + 31) / 32);
	Textures.Add(Key, Entry);
	return Entry.Texture;
}

void FGSOpenGLEmulator::InvalidateTextures(uint32 FirstWord, uint32 EndWord)
{
	TArray<FString> Stale;
	for (const auto& Pair : Textures)
	{
		if (Pair.Value.FirstWord < EndWord && FirstWord < Pair.Value.EndWord)
		{
			Stale.Add(Pair.Key);
		}
	}
	for (const FString& Key : Stale)
	{
		glDeleteTextures(1, &Textures[Key].Texture);
		Textures.Remove(Key);
	}
}

void FGSOpenGLEmulator::ClearTextures()
{
	for (const auto& Pair : Textures)
	{
		glDeleteTextures(1, &Pair.Value.Texture);
	}
	Textures.Reset();
}

void FGSOpenGLEmulator::Present(int32 WindowWidth, int32 WindowHeight)
{
	Flush();
	const int32 Scale = FMath::Max(1, FMath::Min(WindowWidth / FrameWidth, WindowHeight / FrameHeight));
	const int32 Width = FrameWidth * Scale;
	const int32 Height = FrameHeight * Scale;
	const int32 OffsetX = (WindowWidth - Width) / 2;
	const int32 OffsetY = (WindowHeight - Height) / 2;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glViewport(0, 0, WindowWidth, WindowHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(OffsetX, OffsetY, Width, Height);
	PresentProgram.Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ColorTexture);
	PresentProgram.SetInt("uFrame", 0);
	PresentProgram.SetIVec2("uOffset", OffsetX, OffsetY);
	PresentProgram.SetInt("uScale", Scale);
	PresentProgram.SetInt("uFrame16", IsColor16(FrameFormat) ? 1 : 0);
	glBindVertexArray(PresentVertexArray);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glViewport(0, 0, WindowWidth, WindowHeight);
	glEnable(GL_DEPTH_TEST);
}

TArray<FColor> FGSOpenGLEmulator::ReadFrame(int32 Width, int32 Height)
{
	Flush();
	Width = FMath::Clamp(Width, 0, int32(FrameWidth));
	Height = FMath::Clamp(Height, 0, int32(FrameHeight));
	TArray<uint8> Rows;
	Rows.SetNumUninitialized(Width * Height * 4);
	glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	// The frame's top rows are the target's top rows.
	glReadPixels(0, FrameHeight - Height, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, Rows.GetData());
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	TArray<FColor> Pixels;
	Pixels.Reserve(Width * Height);
	const bool bFrame16 = IsColor16(FrameFormat);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		const uint8* Row = &Rows[(Height - 1 - Y) * Width * 4];
		for (int32 X = 0; X < Width; ++X)
		{
			const uint8* Texel = &Row[X * 4];
			if (bFrame16)
			{
				Pixels.Add(FColor(
					uint8(Texel[0] & 0xf8), uint8(Texel[1] & 0xf8), uint8(Texel[2] & 0xf8), uint8(Texel[3] & 0x80)));
			}
			else
			{
				const uint8 Alpha = FrameFormat == EGSPixelFormat::PSMCT32 ? Texel[3] : uint8(0x80);
				Pixels.Add(FColor(Texel[0], Texel[1], Texel[2], Alpha));
			}
		}
	}
	return Pixels;
}
