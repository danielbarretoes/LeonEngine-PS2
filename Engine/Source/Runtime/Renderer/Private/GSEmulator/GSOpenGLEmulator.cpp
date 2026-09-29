#include "GSOpenGLEmulator.h"

#include "Misc/Paths.h"
#include "RendererLog.h"

#include <glad/glad.h>

#include <cmath>

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

	[[nodiscard]] int32 FloorDiv16(int32 Value)
	{
		return Value >= 0 ? Value / 16 : -((-Value + 15) / 16);
	}

	[[nodiscard]] int32 CeilDiv16(int32 Value)
	{
		return -FloorDiv16(-Value);
	}

	/** The uniform names of the arrays the emulator sets element by element. */
	const ANSICHAR* const DimxNames[16] = {"uDimx[0]", "uDimx[1]", "uDimx[2]", "uDimx[3]", "uDimx[4]", "uDimx[5]",
		"uDimx[6]", "uDimx[7]", "uDimx[8]", "uDimx[9]", "uDimx[10]", "uDimx[11]", "uDimx[12]", "uDimx[13]", "uDimx[14]",
		"uDimx[15]"};
	const ANSICHAR* const LevelSizeNames[7] = {"uLevelSize[0]", "uLevelSize[1]", "uLevelSize[2]", "uLevelSize[3]",
		"uLevelSize[4]", "uLevelSize[5]", "uLevelSize[6]"};

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
	glGenTextures(1, &DestinationTexture);
	glBindTexture(GL_TEXTURE_2D, DestinationTexture);
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
	for (uint32* Texture : {&ColorTexture, &DepthTexture, &DestinationTexture})
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
			// The CLUT's load control (CLD, CBP0 / CBP1: manual 3.4.7).
			if (Clut.Update(Memory, Tex0))
			{
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
			Memory.Transfer(BitBltBuf, TrxPos, TrxReg, List.GetImage(int32(Value)));
			// The blocks the transfer's rectangle reaches in the GS's layout.
			const uint32 Span = FGSLocalMemory::GetBlockSpan(
				BitBltBuf.DBW, BitBltBuf.DPSM, uint32(TrxPos.DSAX) + TrxReg.RRW, uint32(TrxPos.DSAY) + TrxReg.RRH);
			InvalidateTextures(BitBltBuf.DBP, uint32(BitBltBuf.DBP) + Span);
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
	// The GS drops the lower 8 bits of S, T and Q's mantissas (manual 3.4.4).
	Out.S = GSTruncateTexCoord(Vertex.ST.S);
	Out.T = GSTruncateTexCoord(Vertex.ST.T);
	Out.Q = GSTruncateTexCoord(Vertex.Color.Q);
	Out.U = float(Vertex.UV.U) / 16.0f;
	Out.V = float(Vertex.UV.V) / 16.0f;
	return Out;
}

FGSOpenGLEmulator::FBatchPrimitive& FGSOpenGLEmulator::BeginPrimitive(uint32 Mode)
{
	BatchMode = Mode;
	FBatchPrimitive& Primitive = BatchPrimitives.AddDefaulted_GetRef();
	Primitive.FirstVertex = Batch.Num();
	return Primitive;
}

void FGSOpenGLEmulator::AddTriangle(const FGSVertex& V0, const FGSVertex& V1, const FGSVertex& V2)
{
	// The pixel centers it may cover, within the frame and the scissor.
	const FGSScissor& Scissor = GetContext().Scissor;
	const int32 MinX = FMath::Max3(CeilDiv16(FMath::Min3(V0.X, V1.X, V2.X)), int32(Scissor.SCAX0), 0);
	const int32 MaxX = FMath::Min3(FloorDiv16(FMath::Max3(V0.X, V1.X, V2.X)), int32(Scissor.SCAX1), FrameWidth - 1);
	const int32 MinY = FMath::Max3(CeilDiv16(FMath::Min3(V0.Y, V1.Y, V2.Y)), int32(Scissor.SCAY0), 0);
	const int32 MaxY = FMath::Min3(FloorDiv16(FMath::Max3(V0.Y, V1.Y, V2.Y)), int32(Scissor.SCAY1), FrameHeight - 1);
	const int64 Area = (int64(V1.X - V0.X) * (V2.Y - V0.Y)) - (int64(V1.Y - V0.Y) * (V2.X - V0.X));
	if (MinX > MaxX || MinY > MaxY || Area == 0)
	{
		return;
	}
	FBatchPrimitive& Primitive = BeginPrimitive(GL_TRIANGLES);
	Primitive.MinX = MinX;
	Primitive.MinY = MinY;
	Primitive.MaxX = MaxX;
	Primitive.MaxY = MaxY;
	Primitive.NumCorners = 3;
	const FGSVertex* Corners[3] = {&V0, &V1, &V2};
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		Primitive.CornerX[Corner] = Corners[Corner]->X;
		Primitive.CornerY[Corner] = Corners[Corner]->Y;
	}
	Primitive.NumVertices = 3;
	// The kicking vertex comes last: OpenGL's provoking vertex, the flat colour's (manual 3.2.8).
	Batch.Add(ToBatchVertex(V0));
	Batch.Add(ToBatchVertex(V1));
	Batch.Add(ToBatchVertex(V2));
}

void FGSOpenGLEmulator::AddSprite(const FGSVertex& V0, const FGSVertex& V1)
{
	const FGSScissor& Scissor = GetContext().Scissor;
	const int32 Left = FMath::Min(V0.X, V1.X);
	const int32 Right = FMath::Max(V0.X, V1.X);
	const int32 Top = FMath::Min(V0.Y, V1.Y);
	const int32 Bottom = FMath::Max(V0.Y, V1.Y);
	// Top and left sides drawn, bottom and right not (manual 3.2.9).
	const int32 MinX = FMath::Max3(CeilDiv16(Left), int32(Scissor.SCAX0), 0);
	const int32 MaxX = FMath::Min3(CeilDiv16(Right) - 1, int32(Scissor.SCAX1), FrameWidth - 1);
	const int32 MinY = FMath::Max3(CeilDiv16(Top), int32(Scissor.SCAY0), 0);
	const int32 MaxY = FMath::Min3(CeilDiv16(Bottom) - 1, int32(Scissor.SCAY1), FrameHeight - 1);
	if (MinX > MaxX || MinY > MaxY)
	{
		return;
	}

	// Z, fog, colour and Q are the second vertex's; the texture coordinates span the rectangle (manual 3.2.8).
	const FBatchVertex First = ToBatchVertex(V0);
	const FBatchVertex Second = ToBatchVertex(V1);
	const auto Corner = [&First, &Second](bool bSecondX, bool bSecondY)
	{
		FBatchVertex Out = Second;
		Out.X = bSecondX ? Second.X : First.X;
		Out.Y = bSecondY ? Second.Y : First.Y;
		Out.S = bSecondX ? Second.S : First.S;
		Out.T = bSecondY ? Second.T : First.T;
		Out.U = bSecondX ? Second.U : First.U;
		Out.V = bSecondY ? Second.V : First.V;
		return Out;
	};
	FBatchPrimitive& Primitive = BeginPrimitive(GL_TRIANGLES);
	Primitive.MinX = MinX;
	Primitive.MinY = MinY;
	Primitive.MaxX = MaxX;
	Primitive.MaxY = MaxY;
	Primitive.NumCorners = 4;
	const int32 CornerX[4] = {Left, Right, Right, Left};
	const int32 CornerY[4] = {Top, Top, Bottom, Bottom};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Primitive.CornerX[Index] = CornerX[Index];
		Primitive.CornerY[Index] = CornerY[Index];
	}
	Primitive.NumVertices = 6;
	const FBatchVertex FirstFirst = Corner(false, false);
	const FBatchVertex SecondFirst = Corner(true, false);
	const FBatchVertex FirstSecond = Corner(false, true);
	const FBatchVertex SecondSecond = Corner(true, true);
	Batch.Append({FirstFirst, SecondFirst, SecondSecond, FirstFirst, SecondSecond, FirstSecond});
}

void FGSOpenGLEmulator::AddPixel(int32 X, int32 Y, double Z, double F, double R, double G, double B, double A, double S,
	double T, double Q, double U, double V)
{
	// The reference's Z: rounded to the nearest and clamped to the format (the fragment then compares it exactly).
	const uint32 ZMax = MaxZ(GetContext().ZBuf.PSM);
	const double Rounded = FMath::Clamp(std::floor(Z + 0.5), 0.0, double(ZMax));
	FBatchVertex Out;
	Out.X = float(X);
	Out.Y = float(Y);
	Out.Depth = float(Rounded / double(ZMax));
	Out.Fog = float(F);
	Out.R = float(R);
	Out.G = float(G);
	Out.B = float(B);
	Out.A = float(A);
	Out.S = float(S);
	Out.T = float(T);
	Out.Q = float(Q);
	Out.U = float(U);
	Out.V = float(V);
	Batch.Add(Out);
	FBatchPrimitive& Primitive = BatchPrimitives.Last();
	++Primitive.NumVertices;
	if (Primitive.MaxX < Primitive.MinX)
	{
		Primitive.MinX = Primitive.MaxX = X;
		Primitive.MinY = Primitive.MaxY = Y;
	}
	else
	{
		Primitive.MinX = FMath::Min(Primitive.MinX, X);
		Primitive.MaxX = FMath::Max(Primitive.MaxX, X);
		Primitive.MinY = FMath::Min(Primitive.MinY, Y);
		Primitive.MaxY = FMath::Max(Primitive.MaxY, Y);
	}
}

void FGSOpenGLEmulator::AddLine(const FGSVertex& From, const FGSVertex& To)
{
	// The reference's pixels and attributes (GSStepLine: the end point left out, manual 3.2.9), flat shading from the
	// kicking vertex.
	(void)BeginPrimitive(GL_POINTS);
	const FGSRGBAQ& Flat = To.Color;
	const bool bGouraud = Prim.bGouraud;
	const auto Truncated = [](float Value) { return double(GSTruncateTexCoord(Value)); };
	GSStepLine(From.X, From.Y, To.X, To.Y,
		[this, &From, &To, &Flat, bGouraud, &Truncated](int32 X, int32 Y, int64 Step, int64 Steps)
		{
			const auto Lerp = [Step, Steps](double A, double B) { return GSLerpExact(A, B, Step, Steps); };
			AddPixel(X, Y, Lerp(From.Z, To.Z), Lerp(From.F, To.F), bGouraud ? Lerp(From.Color.R, To.Color.R) : Flat.R,
				bGouraud ? Lerp(From.Color.G, To.Color.G) : Flat.G, bGouraud ? Lerp(From.Color.B, To.Color.B) : Flat.B,
				bGouraud ? Lerp(From.Color.A, To.Color.A) : Flat.A, Lerp(Truncated(From.ST.S), Truncated(To.ST.S)),
				Lerp(Truncated(From.ST.T), Truncated(To.ST.T)), Lerp(Truncated(From.Color.Q), Truncated(To.Color.Q)),
				Lerp(From.UV.U, To.UV.U) / 16.0, Lerp(From.UV.V, To.UV.V) / 16.0);
		});
	if (BatchPrimitives.Last().NumVertices == 0)
	{
		BatchPrimitives.Pop();
	}
}

void FGSOpenGLEmulator::AddPoint(const FGSVertex& Vertex)
{
	// The pixel nearest the point (manual 3.2.9).
	(void)BeginPrimitive(GL_POINTS);
	AddPixel(FloorDiv16(Vertex.X + 8), FloorDiv16(Vertex.Y + 8), double(Vertex.Z), double(Vertex.F),
		double(Vertex.Color.R), double(Vertex.Color.G), double(Vertex.Color.B), double(Vertex.Color.A),
		double(GSTruncateTexCoord(Vertex.ST.S)), double(GSTruncateTexCoord(Vertex.ST.T)),
		double(GSTruncateTexCoord(Vertex.Color.Q)), double(Vertex.UV.U) / 16.0, double(Vertex.UV.V) / 16.0);
}

bool FGSOpenGLEmulator::Overlaps(int32 First, int32 End, const FBatchPrimitive& Primitive) const
{
	for (int32 Index = First; Index < End; ++Index)
	{
		const FBatchPrimitive& Other = BatchPrimitives[Index];
		if (Other.MaxX < Primitive.MinX || Primitive.MaxX < Other.MinX || Other.MaxY < Primitive.MinY ||
			Primitive.MaxY < Other.MinY)
		{
			continue;
		}
		if (Other.NumCorners == 0 || Primitive.NumCorners == 0)
		{
			// Points: their rectangles are their pixels.
			return true;
		}
		// Two convex shapes whose insides do not meet (a separating axis among their sides' normals, touching allowed)
		// share no pixel center: the top-left rule gives a center on a shared side to one of them.
		bool bSeparated = false;
		for (int32 Shape = 0; Shape < 2 && !bSeparated; ++Shape)
		{
			const FBatchPrimitive& Sides = Shape == 0 ? Other : Primitive;
			for (int32 Side = 0; Side < Sides.NumCorners && !bSeparated; ++Side)
			{
				const int32 Next = (Side + 1) % Sides.NumCorners;
				const int64 AxisX = -int64(Sides.CornerY[Next] - Sides.CornerY[Side]);
				const int64 AxisY = int64(Sides.CornerX[Next] - Sides.CornerX[Side]);
				if (AxisX == 0 && AxisY == 0)
				{
					continue;
				}
				const auto Project = [AxisX, AxisY](const FBatchPrimitive& Corners, int64& OutMin, int64& OutMax)
				{
					OutMin = TNumericLimits<int64>::Max();
					OutMax = TNumericLimits<int64>::Lowest();
					for (int32 Corner = 0; Corner < Corners.NumCorners; ++Corner)
					{
						const int64 Projection = (AxisX * Corners.CornerX[Corner]) + (AxisY * Corners.CornerY[Corner]);
						OutMin = FMath::Min(OutMin, Projection);
						OutMax = FMath::Max(OutMax, Projection);
					}
				};
				int64 MinA = 0;
				int64 MaxA = 0;
				int64 MinB = 0;
				int64 MaxB = 0;
				Project(Other, MinA, MaxA);
				Project(Primitive, MinB, MaxB);
				bSeparated = MaxA <= MinB || MaxB <= MinA;
			}
		}
		if (!bSeparated)
		{
			return true;
		}
	}
	return false;
}

void FGSOpenGLEmulator::Flush()
{
	if (Batch.Num() == 0)
	{
		BatchPrimitives.Reset();
		return;
	}
	const FContext& Context = GetContext();
	const FGSFrame& Frame = Context.Frame;
	FrameFormat = Frame.PSM;
	glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
	glViewport(0, 0, FrameWidth, FrameHeight);
	Program.Bind();
	Program.SetVec2("uTargetSize", float(FrameWidth), float(FrameHeight));
	Program.SetInt("uTargetHeight", FrameHeight);
	Program.SetInt("uGouraud", Prim.bGouraud ? 1 : 0);
	Program.SetInt("uFog", Prim.bFog ? 1 : 0);
	Program.SetIVec3("uFogColor", FogCol.R, FogCol.G, FogCol.B);
	// Each sampler on its own unit, used or not.
	Program.SetInt("uTexture", 0);
	Program.SetInt("uDestination", 1);

	// The texture: its levels, the LOD and the filters (manual 3.4.11, 3.4.12).
	uint32 Texture = 0;
	FTextureLevels Levels;
	if (Prim.bTextured)
	{
		Texture = BindTexture(Context, Levels);
	}
	Program.SetInt("uTextured", Texture != 0 ? 1 : 0);
	if (Texture != 0)
	{
		const FGSTex0& Tex0 = Context.Tex0;
		const FGSTex1& Tex1 = Context.Tex1;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, Texture);
		Program.SetInt("uUseUV", Prim.bUseUV ? 1 : 0);
		Program.SetInt("uTFX", int32(Tex0.TFX));
		Program.SetInt("uTCC", Tex0.bRGBA ? 1 : 0);
		Program.SetIVec2("uBaseSize", 1 << Tex0.TW, 1 << Tex0.TH);
		for (int32 Level = 0; Level < Levels.NumLevels; ++Level)
		{
			Program.SetIVec2(LevelSizeNames[Level], Levels.Width[Level], Levels.Height[Level]);
		}
		Program.SetInt("uMaxLevel", Levels.NumLevels - 1);
		Program.SetInt("uFixedLOD", Tex1.bFixedLOD ? 1 : 0);
		Program.SetInt("uL", Tex1.L);
		Program.SetFloat("uK", float(Tex1.K) / 16.0f);
		Program.SetInt("uMMAG", int32(Tex1.MMAG));
		Program.SetInt("uMMIN", int32(Tex1.MMIN));
		Program.SetIVec2("uWrap", int32(Context.Clamp.WMS), int32(Context.Clamp.WMT));
		Program.SetIVec4("uRegion", Context.Clamp.MINU, Context.Clamp.MAXU, Context.Clamp.MINV, Context.Clamp.MAXV);
	}

	// The frame buffer's format, dithering, the colour clamp and FBA.
	const bool bFrame16 = IsColor16(Frame.PSM);
	const bool bFrame24 = Frame.PSM == EGSPixelFormat::PSMCT24;
	Program.SetInt("uFrameFormat", bFrame16 ? 2 : bFrame24 ? 1 : 0);
	Program.SetInt("uDither", bDither ? 1 : 0);
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Program.SetInt(DimxNames[(Row * 4) + Column], Dimx.M[Row][Column]);
		}
	}
	Program.SetInt("uColorClamp", bColorClamp ? 1 : 0);
	Program.SetInt("uFBA", Context.bFba ? 1 : 0);

	// Blending, the whole equation (manual 3.8), and PABE.
	const FGSAlpha& Alpha = Context.Alpha;
	Program.SetInt("uBlend", Prim.bAlphaBlend ? 1 : 0);
	Program.SetIVec4("uBlendInputs", int32(Alpha.A), int32(Alpha.B), int32(Alpha.C), int32(Alpha.D));
	Program.SetInt("uFix", Alpha.FIX);
	Program.SetInt("uPABE", bPixelAlphaBlend ? 1 : 0);

	// The pixel tests.
	const FGSTest& Test = Context.Test;
	Program.SetInt("uAlphaTest", Test.bAlphaTest ? 1 : 0);
	Program.SetInt("uAlphaTestMode", int32(Test.ATST));
	Program.SetInt("uAlphaRef", Test.AREF);
	Program.SetInt("uDestAlphaTest", Test.bDestinationAlphaTest ? 1 : 0);
	Program.SetInt("uDestAlphaOne", Test.bDestinationAlphaOne ? 1 : 0);
	glEnable(GL_DEPTH_TEST);
	static constexpr GLenum DepthFunctions[4] = {GL_NEVER, GL_ALWAYS, GL_GEQUAL, GL_GREATER};
	glDepthFunc(DepthFunctions[int32(Test.ZTST) & 3]);
	const GLboolean bWriteZ = Context.ZBuf.bMask ? GL_FALSE : GL_TRUE;
	glDisable(GL_BLEND);

	// FBMSK as the bits each pixel keeps (manual 3.9.5): a 16-bit frame holds 5 bits a channel and the alpha bit, a
	// 24-bit one no alpha. A channel whose bits are all kept or all written is a colour mask; anything else, blending,
	// the destination alpha test and an AFAIL that still writes need the destination.
	const uint32 ChannelBits[4] = {bFrame16 ? 0xf8u : 0xffu, bFrame16 ? 0xf800u : 0xff00u,
		bFrame16 ? 0xf80000u : 0xff0000u, bFrame16 ? 0x80000000u : 0xff000000u};
	const uint32 KeepMask = Frame.FBMSK | (bFrame24 ? 0xff000000u : 0u);
	bool bChannelMasks = true;
	GLboolean bWriteChannel[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
	for (int32 Channel = 0; Channel < 4; ++Channel)
	{
		const uint32 Kept = KeepMask & ChannelBits[Channel];
		bWriteChannel[Channel] = Kept == ChannelBits[Channel] ? GL_FALSE : GL_TRUE;
		bChannelMasks &= Kept == 0 || Kept == ChannelBits[Channel];
	}
	const bool bFailWrites = Test.bAlphaTest && Test.AFAIL != EGSAlphaFail::Keep;
	const bool bReadsFrame = Prim.bAlphaBlend || Test.bDestinationAlphaTest || !bChannelMasks || bFailWrites;

	// The scissor, inclusive (the frame's top row is the target's top).
	const FGSScissor& Scissor = Context.Scissor;
	glEnable(GL_SCISSOR_TEST);
	const int32 ScissorWidth = FMath::Max(0, int32(Scissor.SCAX1) - int32(Scissor.SCAX0) + 1);
	const int32 ScissorHeight = FMath::Max(0, int32(Scissor.SCAY1) - int32(Scissor.SCAY0) + 1);
	glScissor(Scissor.SCAX0, FrameHeight - 1 - int32(Scissor.SCAY1), ScissorWidth, ScissorHeight);

	glBindVertexArray(VertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(Batch.Num() * sizeof(FBatchVertex)), Batch.GetData(), GL_STREAM_DRAW);

	if (!bReadsFrame)
	{
		// Written as it comes out of the pipeline; what fails the alpha test (AFAIL KEEP) is discarded.
		Program.SetInt("uReadsFrame", 0);
		Program.SetInt("uAlphaPass", 0);
		glColorMask(bWriteChannel[0], bWriteChannel[1], bWriteChannel[2], bWriteChannel[3]);
		glDepthMask(bWriteZ);
		glDrawArrays(BatchMode, 0, GLsizei(Batch.Num()));
	}
	else
	{
		// Group by group: the frame's rectangle under the group copied as the destination, what passes the alpha test
		// (everything without one), then what fails it and AFAIL still writes (manual 3.7.2).
		Program.SetInt("uReadsFrame", 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, DestinationTexture);
		const uint32 FailKeepMask =
			KeepMask | (Test.AFAIL == EGSAlphaFail::RGBOnly && Frame.PSM == EGSPixelFormat::PSMCT32 ? 0xff000000u : 0u);
		int32 First = 0;
		while (First < BatchPrimitives.Num())
		{
			int32 End = First + 1;
			int32 MinX = BatchPrimitives[First].MinX;
			int32 MinY = BatchPrimitives[First].MinY;
			int32 MaxX = BatchPrimitives[First].MaxX;
			int32 MaxY = BatchPrimitives[First].MaxY;
			while (End < BatchPrimitives.Num() && !Overlaps(First, End, BatchPrimitives[End]))
			{
				const FBatchPrimitive& Added = BatchPrimitives[End];
				MinX = FMath::Min(MinX, Added.MinX);
				MinY = FMath::Min(MinY, Added.MinY);
				MaxX = FMath::Max(MaxX, Added.MaxX);
				MaxY = FMath::Max(MaxY, Added.MaxY);
				++End;
			}
			const int32 FirstVertex = BatchPrimitives[First].FirstVertex;
			const FBatchPrimitive& Last = BatchPrimitives[End - 1];
			const GLsizei GroupVertices = GLsizei(Last.FirstVertex + Last.NumVertices - FirstVertex);
			// The group's rectangle within the frame; OpenGL's row 0 is the frame's bottom row.
			MinX = FMath::Max(MinX, 0);
			MinY = FMath::Max(MinY, 0);
			MaxX = FMath::Min(MaxX, FrameWidth - 1);
			MaxY = FMath::Min(MaxY, FrameHeight - 1);
			if (MinX <= MaxX && MinY <= MaxY)
			{
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, MinX, FrameHeight - 1 - MaxY, MinX, FrameHeight - 1 - MaxY,
					MaxX - MinX + 1, MaxY - MinY + 1);
			}
			Program.SetInt("uAlphaPass", 0);
			Program.SetInt("uKeepMask", int32(KeepMask));
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(bWriteZ);
			glDrawArrays(BatchMode, FirstVertex, GroupVertices);
			if (bFailWrites)
			{
				Program.SetInt("uAlphaPass", 1);
				const bool bZOnly = Test.AFAIL == EGSAlphaFail::ZBufferOnly;
				Program.SetInt("uKeepMask", int32(FailKeepMask));
				glColorMask(!bZOnly, !bZOnly, !bZOnly, !bZOnly);
				glDepthMask(bZOnly ? bWriteZ : GL_FALSE);
				glDrawArrays(BatchMode, FirstVertex, GroupVertices);
			}
			First = End;
		}
		glActiveTexture(GL_TEXTURE0);
	}
	glBindVertexArray(0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glDisable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	Batch.Reset();
	BatchPrimitives.Reset();
}

uint32 FGSOpenGLEmulator::BindTexture(const FContext& Context, FTextureLevels& OutLevels)
{
	const FGSTex0& Tex0 = Context.Tex0;
	const FGSTex1& Tex1 = Context.Tex1;
	const FGSClamp& Clamp = Context.Clamp;
	// The MIPMAP filters sample levels 0 to MXL (manual 3.4.11), the others level 0.
	const bool bMipmap = uint8(Tex1.MMIN) >= uint8(EGSFilter::NearestMipmapNearest);
	OutLevels.NumLevels = bMipmap ? FMath::Min<int32>(Tex1.MXL, 6) + 1 : 1;

	// Each level's buffer and size, and the texels its wrap mode reaches: the region modes may reach past the size
	// (their fields shifted by the level).
	const auto Reach = [](EGSWrapMode Mode, int32 Size, int32 Level, uint16 Min, uint16 Max)
	{
		switch (Mode)
		{
			case EGSWrapMode::RegionClamp:
				return FMath::Max(Size, (int32(FMath::Max(Min, Max)) >> Level) + 1);
			case EGSWrapMode::RegionRepeat:
				return FMath::Max(Size, (int32(Min | Max) >> Level) + 1);
			default:
				return Size;
		}
	};
	uint32 BasePointers[7] = {};
	uint32 BufferWidths[7] = {};
	int32 ReachU[7] = {};
	int32 ReachV[7] = {};
	int32 LayerWidth = 1;
	int32 LayerHeight = 1;
	FString Key;
	FGSTex0 KeyTex0 = Tex0;
	KeyTex0.CLD = 0;
	KeyTex0.TFX = EGSTextureFunction::Modulate;
	KeyTex0.bRGBA = true;
	const bool bClut = IsClutFormat(Tex0.PSM);
	Key = FString::Printf(
		"%llx:%llx:%x", (unsigned long long)KeyTex0.Encode(), (unsigned long long)TexA.Encode(), bClut ? ClutHash : 0u);
	for (int32 Level = 0; Level < OutLevels.NumLevels; ++Level)
	{
		BasePointers[Level] = Tex0.TBP0;
		BufferWidths[Level] = Tex0.TBW;
		if (Level >= 1 && Level <= 3)
		{
			BasePointers[Level] = Context.MipTbp1.TBP[Level - 1];
			BufferWidths[Level] = Context.MipTbp1.TBW[Level - 1];
		}
		else if (Level >= 4)
		{
			BasePointers[Level] = Context.MipTbp2.TBP[Level - 4];
			BufferWidths[Level] = Context.MipTbp2.TBW[Level - 4];
		}
		OutLevels.Width[Level] = FMath::Max(1, (1 << Tex0.TW) >> Level);
		OutLevels.Height[Level] = FMath::Max(1, (1 << Tex0.TH) >> Level);
		ReachU[Level] = Reach(Clamp.WMS, OutLevels.Width[Level], Level, Clamp.MINU, Clamp.MAXU);
		ReachV[Level] = Reach(Clamp.WMT, OutLevels.Height[Level], Level, Clamp.MINV, Clamp.MAXV);
		LayerWidth = FMath::Max(LayerWidth, ReachU[Level]);
		LayerHeight = FMath::Max(LayerHeight, ReachV[Level]);
		Key += FString::Printf(":%u:%u:%d:%d", BasePointers[Level], BufferWidths[Level], ReachU[Level], ReachV[Level]);
	}
	if (const FTextureEntry* Entry = Textures.Find(Key))
	{
		return Entry->Texture;
	}

	FTextureEntry Entry;
	glGenTextures(1, &Entry.Texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, Entry.Texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, LayerWidth, LayerHeight, OutLevels.NumLevels, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	TArray<uint8> Texels;
	for (int32 Level = 0; Level < OutLevels.NumLevels; ++Level)
	{
		const int32 Width = ReachU[Level];
		const int32 Height = ReachV[Level];
		Texels.SetNumUninitialized(Width * Height * 4);
		for (int32 V = 0; V < Height; ++V)
		{
			for (int32 U = 0; U < Width; ++U)
			{
				const FColor Color = FGSTexelDecoder::Decode(
					Memory, Tex0, BasePointers[Level], BufferWidths[Level], TexA, Clut, uint32(U), uint32(V));
				uint8* Texel = &Texels[((V * Width) + U) * 4];
				Texel[0] = Color.R;
				Texel[1] = Color.G;
				Texel[2] = Color.B;
				Texel[3] = Color.A;
			}
		}
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0, Level, Width, Height, 1, GL_RGBA, GL_UNSIGNED_BYTE, Texels.GetData());
		FBlockRange& Range = Entry.Ranges.AddDefaulted_GetRef();
		Range.FirstBlock = BasePointers[Level];
		Range.EndBlock = BasePointers[Level] +
			FGSLocalMemory::GetBlockSpan(BufferWidths[Level], Tex0.PSM, uint32(Width), uint32(Height));
	}
	const uint32 Texture = Entry.Texture;
	Textures.Add(Key, MoveTemp(Entry));
	return Texture;
}

void FGSOpenGLEmulator::InvalidateTextures(uint32 FirstBlock, uint32 EndBlock)
{
	// The ranges may run past the end of memory, which wraps: compare them a memory's size apart too.
	const auto Overlaps = [FirstBlock, EndBlock](const FBlockRange& Range)
	{
		for (const uint32 Offset : {0u, FGSLocalMemory::NumBlocks})
		{
			if ((Range.FirstBlock + Offset < EndBlock && FirstBlock < Range.EndBlock + Offset) ||
				(FirstBlock + Offset < Range.EndBlock && Range.FirstBlock < EndBlock + Offset))
			{
				return true;
			}
		}
		return false;
	};
	TArray<FString> Stale;
	for (const auto& Pair : Textures)
	{
		for (const FBlockRange& Range : Pair.Value.Ranges)
		{
			if (Overlaps(Range))
			{
				Stale.Add(Pair.Key);
				break;
			}
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

void FGSOpenGLEmulator::GetPresentRect(int32 WindowWidth, int32 WindowHeight, float DisplayAspectRatio, int32& OutX,
	int32& OutY, int32& OutWidth, int32& OutHeight)
{
	const float Aspect = DisplayAspectRatio > 0.0f ? DisplayAspectRatio : float(FrameWidth) / float(FrameHeight);
	// The largest whole number of window lines per frame line that fits the width at the aspect.
	const int32 ByHeight = WindowHeight / FrameHeight;
	const int32 ByWidth = int32(float(WindowWidth) / (float(FrameHeight) * Aspect));
	const int32 Scale = FMath::Max(1, FMath::Min(ByHeight, ByWidth));
	OutHeight = FrameHeight * Scale;
	OutWidth = FMath::RoundToInt(float(OutHeight) * Aspect);
	OutX = (WindowWidth - OutWidth) / 2;
	OutY = (WindowHeight - OutHeight) / 2;
}

void FGSOpenGLEmulator::Present(int32 WindowWidth, int32 WindowHeight, float DisplayAspectRatio)
{
	Flush();
	int32 OffsetX = 0;
	int32 OffsetY = 0;
	int32 Width = 0;
	int32 Height = 0;
	GetPresentRect(WindowWidth, WindowHeight, DisplayAspectRatio, OffsetX, OffsetY, Width, Height);
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
	PresentProgram.SetIVec2("uSize", Width, Height);
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
