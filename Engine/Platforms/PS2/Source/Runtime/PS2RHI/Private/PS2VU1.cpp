#include "PS2VU1.h"

#include "DynamicRHI.h"
#include "GSGifPacket.h"
#include "GSPrimitiveEmitter.h"
#include "GSTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PS2VU1Encoder.h"
#include "Templates/AlignmentTemplates.h"

#include <dma.h>
#include <kernel.h>

// VU1Programs.vsm's and Skinned.vsm's labels: the code's bytes in the ELF (MPG uploads them, the static programs at
// 0 and the skinned ones after them) and their entry points.
extern "C"
{
	extern uint32 VU1Programs_CodeStart;
	extern uint32 VU1Programs_CodeEnd;
	extern uint32 VU1Programs_StaticLit;
	extern uint32 VU1Programs_StaticUnlit;
	extern uint32 VU1Skinned_CodeStart;
	extern uint32 VU1Skinned_CodeEnd;
	extern uint32 VU1Skinned_Lit;
	extern uint32 VU1Skinned_Unlit;
}

namespace Leon::PS2
{

	namespace
	{

		/** An UNPACK's format (CMD's low 4 bits: vn, vl). */
		enum class EUnpackFormat : uint8
		{
			V2_16 = 0x05,
			V3_16 = 0x09,
			V4_32 = 0x0c,
			V4_8 = 0x0e,
		};

		/** UNPACK of Num vectors to Address (from TOPS with bTops), sign or zero extended (bUnsigned). */
		[[nodiscard]] uint32 MakeUnpack(EUnpackFormat Format, uint32 Num, uint32 Address, bool bUnsigned, bool bTops)
		{
			check(Num > 0 && Num <= 256 && Address < 1024);
			const uint32 Immediate = Address | (bUnsigned ? (1u << 14) : 0u) | (bTops ? (1u << 15) : 0u);
			return FGSGifPacket::MakeVifCode(EGSVifCommand::Unpack, Num & 0xff, Immediate) | (uint32(Format) << 24);
		}

		[[nodiscard]] uint32 FloatWord(float Value)
		{
			uint32 Word = 0;
			FMemory::Memcpy(&Word, &Value, sizeof(Word));
			return Word;
		}

		void PutQuadword(uint32* Words, float X, float Y, float Z, float W)
		{
			Words[0] = FloatWord(X);
			Words[1] = FloatWord(Y);
			Words[2] = FloatWord(Z);
			Words[3] = FloatWord(W);
		}

		/** Writes quadwords of 32-bit words into a chain (64 bits at a time: the chain goes through the UCAB). */
		uint64* CopyQuadwords(uint64* Out, const uint32* Words, uint32 NumQuadwords)
		{
			for (uint32 Index = 0; Index < NumQuadwords * 2; ++Index)
			{
				Out[Index] = uint64(Words[Index * 2]) | (uint64(Words[(Index * 2) + 1]) << 32);
			}
			return Out + (NumQuadwords * 2);
		}

		/** A REF of Bytes (padded with zeros to a quadword: VIF NOPs after the UNPACK's data) with its UNPACK. */
		uint64* WriteStream(uint64* Out, const void* Data, uint32 Bytes, uint32 UnpackCode)
		{
			const uint32 Quadwords = (Bytes + 15) / 16;
			Out[0] = FGSGifPacket::MakeDmaTag(Quadwords, EGSDmaTag::Ref, FGSGifPacket::GetDmaAddress(Data));
			Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Nop), UnpackCode);
			return Out + 2;
		}

		/** A label's distance in bytes from its block's start. */
		[[nodiscard]] uint32 GetCodeOffset(const uint32& Label, const uint32& CodeStart)
		{
			return uint32(reinterpret_cast<const uint8*>(&Label) - reinterpret_cast<const uint8*>(&CodeStart));
		}

		/** A block's instructions. */
		[[nodiscard]] uint32 GetNumInstructions(const uint32& CodeStart, const uint32& CodeEnd)
		{
			return GetCodeOffset(CodeEnd, CodeStart) / 8;
		}

		/** VIF1_STAT: VPS (the VIF's state, 0 idle) and FQC (the quadwords in its FIFO). */
		volatile uint32* const Vif1Stat = reinterpret_cast<volatile uint32*>(0x10003c00);
		/** VU1's data memory as the EE sees it while VU1 is stopped. */
		volatile const uint32* const Vu1Memory = reinterpret_cast<volatile const uint32*>(0x1100c000);

		bool GIsEnabledKnown = false;
		bool GIsEnabled = true;

	} // namespace

	uint32 GetProgramAddress(EGSVertexProgram Program)
	{
		const uint32 SkinnedStart = GetNumInstructions(VU1Programs_CodeStart, VU1Programs_CodeEnd);
		switch (Program)
		{
			case EGSVertexProgram::StaticLit:
				return GetCodeOffset(VU1Programs_StaticLit, VU1Programs_CodeStart) / 8;
			case EGSVertexProgram::SkinnedUnlit:
				return SkinnedStart + (GetCodeOffset(VU1Skinned_Unlit, VU1Skinned_CodeStart) / 8);
			case EGSVertexProgram::SkinnedLit:
				return SkinnedStart + (GetCodeOffset(VU1Skinned_Lit, VU1Skinned_CodeStart) / 8);
			default:
				return GetCodeOffset(VU1Programs_StaticUnlit, VU1Programs_CodeStart) / 8;
		}
	}

	bool FPS2VU1BatchEncoder::IsHeaderSkinned() const
	{
		return HeaderProgram == EGSVertexProgram::SkinnedUnlit || HeaderProgram == EGSVertexProgram::SkinnedLit;
	}

	bool FPS2VU1BatchEncoder::IsHeaderLit() const
	{
		return HeaderProgram == EGSVertexProgram::StaticLit || HeaderProgram == EGSVertexProgram::SkinnedLit;
	}

	void FPS2VU1BatchEncoder::Begin(const FGSDrawEnvironment& InEnvironment, bool bInKick)
	{
		Environment = InEnvironment;
		bKick = bInKick;
		HeaderList = nullptr;
		HeaderDraw = INDEX_NONE;
	}

	uint32 FPS2VU1BatchEncoder::GetPrologueQuadwords() const
	{
		return 2 + VU1Memory::NumShared;
	}

	uint32 FPS2VU1BatchEncoder::GetMaxBatchQuadwords() const
	{
		// The header's CNT, the palette's and the five streams' REFs (a skinned batch's) and the MSCAL's CNT.
		return 1 + VU1SkinnedMemory::Header + 6 + 1;
	}

	uint64* FPS2VU1BatchEncoder::WritePrologue(uint64* Out)
	{
		// STCYCL 1, 1 (contiguous) and STMOD 0 in the tag; BASE and OFFSET (TOPS = BASE, DBF 0), then the shared
		// constants at 0.
		const uint32 NumQuadwords = 1 + VU1Memory::NumShared;
		Out[0] = FGSGifPacket::MakeDmaTag(NumQuadwords, EGSDmaTag::Cnt);
		Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::StCycl, 0, 0x0101),
			FGSGifPacket::MakeVifCode(EGSVifCommand::StMod, 0, 0));
		Out[2] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Base, 0, VU1Memory::Base),
			FGSGifPacket::MakeVifCode(EGSVifCommand::Offset, 0, VU1Memory::Offset));
		Out[3] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Nop),
			MakeUnpack(EUnpackFormat::V4_32, VU1Memory::NumShared, VU1Memory::ScreenScale, false, false));
		// The GS's pixel of a clip space vertex, as FGSPrimitiveEmitter maps it: X = 2048 - 0.5 + x / w * W / 2 (FTOI4
		// truncates, so a 32nd of a pixel more rounds it as GSToFixed4 does), Y the same downwards, Z = (1 - z / w) *
		// ZMax over 16, clamped, then times 16 (the scale's w) for XYZF2's Z at bit 4 (N15). The limits' w is ADC in
		// XYZF2's F lane: 2048, 0x8000 after FTOI4.
		constexpr float Round = 1.0f / 32.0f;
		const float Center = FGSDrawEnvironment::PrimitiveCenter - 0.5f + Round;
		const float DepthScale = float(FGSDrawEnvironment::MaxDepth24) / 16.0f;
		alignas(16) uint32 Shared[VU1Memory::NumShared * 4];
		PutQuadword(&Shared[VU1Memory::ScreenScale * 4], float(Environment.Width) * 0.5f,
			float(Environment.Height) * -0.5f, -DepthScale, 16.0f);
		PutQuadword(&Shared[VU1Memory::ScreenOffset * 4], Center, Center, DepthScale, 0.0f);
		PutQuadword(&Shared[VU1Memory::Limits * 4], 255.0f, 0.5f, DepthScale, 2048.0f);
		// The clipping (ps2-polish P8b, ClipTriangles.vsi), with the emitter's guard band: a position's outcodes are
		// the signs of (w - x, w + x, w - y, w + y) = w (1, 1, 1, 1) + x (-1, 1, 0, 0) + y (0, 0, -1, 1), of the same
		// with the guard band's w (GX, GX, GY, GY), and of (z, w - z) = w (0, 1) + z (1, -1); the clip planes are
		// distance = plane . position, FGSPrimitiveEmitter's (near, far, then the guard band's four sides).
		const float GuardX = FGSPrimitiveEmitter::GuardExtent / (float(Environment.Width) * 0.5f);
		const float GuardY = FGSPrimitiveEmitter::GuardExtent / (float(Environment.Height) * 0.5f);
		uint32* Outcodes = &Shared[VU1Memory::OutcodeVectors * 4];
		PutQuadword(Outcodes + 0, 1.0f, 1.0f, 1.0f, 1.0f);
		PutQuadword(Outcodes + 4, -1.0f, 1.0f, 0.0f, 0.0f);
		PutQuadword(Outcodes + 8, 0.0f, 0.0f, -1.0f, 1.0f);
		PutQuadword(Outcodes + 12, GuardX, GuardX, GuardY, GuardY);
		PutQuadword(Outcodes + 16, 0.0f, 1.0f, 0.0f, 0.0f);
		PutQuadword(Outcodes + 20, 1.0f, -1.0f, 0.0f, 0.0f);
		uint32* Planes = &Shared[VU1Memory::ClipPlanes * 4];
		PutQuadword(Planes + 0, 0.0f, 0.0f, 1.0f, 0.0f);
		PutQuadword(Planes + 4, 0.0f, 0.0f, -1.0f, 1.0f);
		PutQuadword(Planes + 8, -1.0f, 0.0f, 0.0f, GuardX);
		PutQuadword(Planes + 12, 1.0f, 0.0f, 0.0f, GuardX);
		PutQuadword(Planes + 16, 0.0f, -1.0f, 0.0f, GuardY);
		PutQuadword(Planes + 20, 0.0f, 1.0f, 0.0f, GuardY);
		return CopyQuadwords(Out + 4, Shared, VU1Memory::NumShared);
	}

	void FPS2VU1BatchEncoder::PrepareDraw(const FGSVertexDraw& Draw)
	{
		verify(Draw.GetProgram(HeaderProgram));
		if (IsHeaderSkinned())
		{
			// The program dequantizes and poses the position, so the transform is LocalToClip itself; the
			// quantization's scale (with the weights' 1/255) and bias follow the lights.
			constexpr float One[3] = {1.0f, 1.0f, 1.0f};
			constexpr float Zero[3] = {0.0f, 0.0f, 0.0f};
			PrepareRows(Draw.LocalToClip, One, Zero, 1);
			PrepareRows(Draw.LocalToWorld, One, Zero, VU1Memory::ToWorld);
			PutQuadword(&Header[VU1SkinnedMemory::Quantization * 4], Draw.PositionScale[0], Draw.PositionScale[1],
				Draw.PositionScale[2], 1.0f / 255.0f);
			PutQuadword(&Header[(VU1SkinnedMemory::Quantization + 1) * 4], Draw.PositionBias[0], Draw.PositionBias[1],
				Draw.PositionBias[2], 0.0f);
		}
		else
		{
			PrepareRows(Draw.LocalToClip, Draw.PositionScale, Draw.PositionBias, 1);
			PrepareRows(Draw.LocalToWorld, Draw.PositionScale, Draw.PositionBias, VU1Memory::ToWorld);
		}
		// The baked bytes to RGBAQ's: the material's colour times 255 (128 textured: MODULATE's 1.0) over 255; the
		// alpha's 1.0 is 128.
		const float ColorScale = (Draw.bTextured ? 128.0f : 255.0f) / 255.0f;
		PutQuadword(&Header[5 * 4], Draw.Color.R * ColorScale, Draw.Color.G * ColorScale, Draw.Color.B * ColorScale,
			Draw.Color.A * (128.0f / 255.0f));
		// The UV scale, then the fog's line in w (F 255 everywhere without fog: FGE is off then).
		const FGSVertexFog& Fog = Draw.Fog;
		PutQuadword(&Header[7 * 4], Draw.UvScale.X, Draw.UvScale.Y, Fog.bEnabled ? Fog.Scale : 0.0f,
			Fog.bEnabled ? Fog.Offset : 255.0f);
		if (IsHeaderLit())
		{
			const FMatrix& Normal = Draw.NormalToWorld;
			for (int32 Row = 0; Row < 3; ++Row)
			{
				PutQuadword(&Header[(9 + Row) * 4], Normal.M[Row][0], Normal.M[Row][1], Normal.M[Row][2], 0.0f);
			}
			const FGSVertexLights& Lights = Draw.Lights;
			const bool bSun = Lights.NumDirectional > 0;
			const FVector Toward = bSun ? -Lights.Directional[0].Direction : FVector::ZeroVector;
			const FVector Color = bSun ? Lights.Directional[0].Color : FVector::ZeroVector;
			PutQuadword(&Header[12 * 4], Toward.X, Toward.Y, Toward.Z, 0.0f);
			PutQuadword(&Header[13 * 4], Color.X, Color.Y, Color.Z, 0.0f);
			PutQuadword(&Header[14 * 4], Lights.Ambient.X, Lights.Ambient.Y, Lights.Ambient.Z, 0.0f);
			// The point lights (N29): position and 1 / range (the reference's range is at least 0.1), colour.
			for (uint32 Index = 0; Index < VU1Memory::MaxPointLights; ++Index)
			{
				uint32* Light = &Header[(VU1Memory::PointLights + (2 * Index)) * 4];
				if (int32(Index) < Lights.NumPoint)
				{
					const FGSVertexLights::FPoint& Point = Lights.Point[Index];
					PutQuadword(Light, Point.Position.X, Point.Position.Y, Point.Position.Z,
						1.0f / FMath::Max(Point.Radius, 0.1f));
					PutQuadword(Light + 4, Point.Color.X, Point.Color.Y, Point.Color.Z, 0.0f);
				}
				else
				{
					PutQuadword(Light, 0.0f, 0.0f, 0.0f, 0.0f);
					PutQuadword(Light + 4, 0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
			HeaderLighting = 1 + uint32(Lights.NumPoint);
		}
		else
		{
			HeaderLighting = 0;
		}
		FGSPrim Prim;
		Prim.Type = EGSPrimitive::TriangleStrip;
		Prim.bGouraud = true;
		Prim.bTextured = Draw.bTextured;
		Prim.bFog = Draw.Fog.bEnabled;
		Prim.bAlphaBlend = Draw.bBlend;
		HeaderPrim = Prim.Encode();
		Prim.Type = EGSPrimitive::Triangle;
		HeaderClipPrim = Prim.Encode();
	}

	void FPS2VU1BatchEncoder::PrepareRows(const FMatrix& Matrix, const float* Scale, const float* Bias, uint32 FirstRow)
	{
		// The quantized position through Matrix: row R scaled by the axis's scale, the bias moved into the translation
		// row (UE's row vectors: (P, 1) * M).
		for (int32 Row = 0; Row < 3; ++Row)
		{
			PutQuadword(&Header[(FirstRow + Row) * 4], Matrix.M[Row][0] * Scale[Row], Matrix.M[Row][1] * Scale[Row],
				Matrix.M[Row][2] * Scale[Row], Matrix.M[Row][3] * Scale[Row]);
		}
		float Translation[4];
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Translation[Column] = Matrix.M[3][Column] + (Bias[0] * Matrix.M[0][Column]) +
				(Bias[1] * Matrix.M[1][Column]) + (Bias[2] * Matrix.M[2][Column]);
		}
		PutQuadword(&Header[(FirstRow + 3) * 4], Translation[0], Translation[1], Translation[2], Translation[3]);
	}

	uint64* FPS2VU1BatchEncoder::WriteBatch(const FGSCommandList& List, const FGSVertexBatch& Batch, uint64* Out)
	{
		const FGSVertexDraw& Draw = List.GetVertexDraws()[Batch.Draw];
		if (&List != HeaderList || Batch.Draw != HeaderDraw)
		{
			PrepareDraw(Draw);
			HeaderList = &List;
			HeaderDraw = Batch.Draw;
		}
		const uint32 NumVertices = Batch.NumVertices;
		const bool bLit = IsHeaderLit();
		const bool bSkinned = IsHeaderSkinned();
		check(bSkinned == Batch.IsSkinned());
		const uint32 HeaderQuadwords = bSkinned ? VU1SkinnedMemory::Header
			: bLit                              ? VU1Memory::HeaderLit
												: VU1Memory::HeaderUnlit;
		// The batch's own: its vertices and flags, its texture coordinates' offset and the GIFtag (NLOOP, EOP, PRE and
		// PRIM, PACKED with ST, RGBAQ and XYZF2: StaticUnlit and StaticLit write the fog's F, N15). A clipped batch's
		// (P8b) is its chunks' TRIANGLE tag, whose NLOOP the program writes.
		Header[0] = NumVertices;
		Header[1] = bKick ? 1 : 0;
		Header[2] = HeaderLighting;
		Header[3] = Batch.bClip ? 1 : 0;
		PutQuadword(&Header[6 * 4], Batch.TexCoordOffset[0], Batch.TexCoordOffset[1], 0.0f, 0.0f);
		const uint64 GifTag = FGSGifPacket::MakeTag(Batch.bClip ? 0 : NumVertices, true, EGSGifFormat::Packed, 3) |
			(uint64(1) << 46) | ((Batch.bClip ? HeaderClipPrim : HeaderPrim) << 47);
		const uint64 Registers =
			uint64(EGSRegister::ST) | (uint64(EGSRegister::RGBAQ) << 4) | (uint64(EGSRegister::XYZF2) << 8);
		Header[(VU1Memory::GifTag * 4) + 0] = uint32(GifTag);
		Header[(VU1Memory::GifTag * 4) + 1] = uint32(GifTag >> 32);
		Header[(VU1Memory::GifTag * 4) + 2] = uint32(Registers);
		Header[(VU1Memory::GifTag * 4) + 3] = 0;
		Out[0] = FGSGifPacket::MakeDmaTag(HeaderQuadwords, EGSDmaTag::Cnt);
		Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Nop),
			MakeUnpack(EUnpackFormat::V4_32, HeaderQuadwords, 0, false, true));
		Out = CopyQuadwords(Out + 2, Header, HeaderQuadwords);
		// The streams where the mesh keeps them (a skinned batch's palette where the list does). VIF1 reads the words
		// of a REF's last quadword after its UNPACK's data as VIFcodes, so a 4-byte-a-vertex stream is unpacked whole
		// (its vertices rounded up to 4: VU1 ignores the extra ones), whatever its padding holds (an instance's baked
		// colours pad with 0xff, N22); the positions' are the LPS2 v2 build's zeros (NOPs).
		const uint32 NumWhole = Align(NumVertices, 4u);
		if (bSkinned)
		{
			check((UPTRINT(Batch.Palette) & 15) == 0);
			Out = WriteStream(Out, Batch.Palette, Batch.NumBones * uint32(sizeof(FGSSkinMatrix)),
				MakeUnpack(EUnpackFormat::V4_32, Batch.NumBones * 3, VU1SkinnedMemory::Palette, false, true));
			Out = WriteStream(Out, Batch.Positions, NumVertices * 6,
				MakeUnpack(EUnpackFormat::V3_16, NumVertices, VU1SkinnedMemory::Positions, false, true));
			Out = WriteStream(Out, Batch.Normals, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V4_8, NumWhole, VU1SkinnedMemory::Normals, false, true));
			Out = WriteStream(Out, Batch.Colors, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V4_8, NumWhole, VU1SkinnedMemory::Colors, true, true));
			Out = WriteStream(Out, Batch.TexCoords, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V2_16, NumWhole, VU1SkinnedMemory::TexCoords, false, true));
			Out = WriteStream(Out, Batch.Skin, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V4_8, NumWhole, VU1SkinnedMemory::Skin, true, true));
		}
		else
		{
			Out = WriteStream(Out, Batch.Positions, NumVertices * 6,
				MakeUnpack(EUnpackFormat::V3_16, NumVertices, VU1Memory::Positions, false, true));
			Out = WriteStream(Out, Batch.Normals, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V4_8, NumWhole, VU1Memory::Normals, false, true));
			Out = WriteStream(Out, Batch.Colors, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V4_8, NumWhole, VU1Memory::Colors, true, true));
			Out = WriteStream(Out, Batch.TexCoords, NumWhole * 4,
				MakeUnpack(EUnpackFormat::V2_16, NumWhole, VU1Memory::TexCoords, false, true));
		}
		// Its program, once the one before ends (and the buffers swap).
		Out[0] = FGSGifPacket::MakeDmaTag(0, EGSDmaTag::Cnt);
		Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Nop),
			FGSGifPacket::MakeVifCode(EGSVifCommand::MsCal, 0, GetProgramAddress(HeaderProgram)));
		return Out + 2;
	}

} // namespace Leon::PS2

bool FPS2VU1::IsEnabled()
{
	using namespace Leon::PS2;
	if (!GIsEnabledKnown)
	{
		GIsEnabled = !FParse::Param(FCommandLine::Get(), TEXT("novu1"));
		GIsEnabledKnown = true;
	}
	return GIsEnabled;
}

void FPS2VU1::UploadPrograms()
{
	// MPG takes 256 instructions at most: a REF of the code's bytes (in the ELF, 16-byte aligned) per 256; the static
	// programs at 0, the skinned ones after them.
	using namespace Leon::PS2;
	const uint32 NumStatic = GetNumInstructions(VU1Programs_CodeStart, VU1Programs_CodeEnd);
	const uint32 NumSkinned = GetNumInstructions(VU1Skinned_CodeStart, VU1Skinned_CodeEnd);
	const uint32 NumInstructions = NumStatic + NumSkinned;
	check(NumStatic % 2 == 0 && NumSkinned % 2 == 0 && NumInstructions <= 2048);
	alignas(64) static uint64 Chain[2 * 12];
	uint64* Out = Chain;
	const auto Upload = [&Out](const uint32& CodeStart, uint32 NumBlock, uint32 Address)
	{
		const uint8* Code = reinterpret_cast<const uint8*>(&CodeStart);
		for (uint32 First = 0; First < NumBlock; First += 256)
		{
			const uint32 Count = FMath::Min(NumBlock - First, 256u);
			Out[0] =
				FGSGifPacket::MakeDmaTag(Count / 2, EGSDmaTag::Ref, FGSGifPacket::GetDmaAddress(Code + (First * 8)));
			Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Nop),
				FGSGifPacket::MakeVifCode(EGSVifCommand::Mpg, Count & 0xff, Address + First));
			Out += 2;
		}
	};
	Upload(VU1Programs_CodeStart, NumStatic, 0);
	Upload(VU1Skinned_CodeStart, NumSkinned, NumStatic);
	Out[0] = FGSGifPacket::MakeDmaTag(0, EGSDmaTag::End);
	Out[1] = 0;
	Out += 2;
	FlushCache(WRITEBACK_DCACHE);
	dma_channel_send_chain(DMA_CHANNEL_VIF1, Chain, int((Out - Chain) / 2), DMA_FLAG_TRANSFERTAG, 0);
	dma_channel_wait(DMA_CHANNEL_VIF1, 0);
	UE_LOG(LogRHI, Log,
		"FPS2VU1: %u instructions of microprograms uploaded (StaticLit at %u, StaticUnlit at %u, SkinnedLit at %u, "
		"SkinnedUnlit at %u)",
		NumInstructions, GetProgramAddress(EGSVertexProgram::StaticLit),
		GetProgramAddress(EGSVertexProgram::StaticUnlit), GetProgramAddress(EGSVertexProgram::SkinnedLit),
		GetProgramAddress(EGSVertexProgram::SkinnedUnlit));
}

bool FPS2VU1::RunBatchForTest(const FGSVertexDraw& Draw, const FGSVertexBatch& Batch,
	const FGSDrawEnvironment& Environment, TArray<uint64>& OutQuadwords)
{
	using namespace Leon::PS2;
	FGSCommandList List;
	FGSVertexBatch Local = Batch;
	Local.Draw = List.AddVertexDraw(Draw);
	List.DrawVertexBatch(Local);
	FPS2VU1BatchEncoder Encoder;
	Encoder.Begin(Environment, false);
	// The prologue, a batch and the END.
	constexpr uint32 ChainQuadwords = 2 + VU1Memory::NumShared + 1 + VU1SkinnedMemory::Header + 6 + 1 + 1;
	alignas(64) static uint64 Chain[2 * ChainQuadwords];
	check(Encoder.GetPrologueQuadwords() + Encoder.GetMaxBatchQuadwords() + 1 <= ChainQuadwords);
	uint64* Out = Encoder.WritePrologue(Chain);
	Out = Encoder.WriteBatch(List, Local, Out);
	// FLUSHE last: once VIF1 is idle with nothing in its FIFO, the program has ended (or stopped with a chunk, P8b).
	Out[0] = FGSGifPacket::MakeDmaTag(0, EGSDmaTag::End);
	Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::FlushE), 0);
	Out += 2;
	const auto SendAndWait = [](uint64* Start, uint64* End)
	{
		FlushCache(WRITEBACK_DCACHE);
		dma_channel_send_chain(DMA_CHANNEL_VIF1, Start, int((End - Start) / 2), DMA_FLAG_TRANSFERTAG, 0);
		dma_channel_wait(DMA_CHANNEL_VIF1, 0);
		bool bIdle = false;
		for (int32 Spin = 0; Spin < 1000000 && !bIdle; ++Spin)
		{
			const uint32 Stat = *Vif1Stat;
			bIdle = (Stat & 3) == 0 && ((Stat >> 24) & 0x1f) == 0;
		}
		return bIdle;
	};
	const auto Read = [&OutQuadwords](uint32 Address, uint32 NumQuadwords)
	{
		const uint32 First = Address * 4;
		for (uint32 Index = 0; Index < NumQuadwords * 2; ++Index)
		{
			OutQuadwords.Add(
				uint64(Vu1Memory[First + (Index * 2)]) | (uint64(Vu1Memory[First + (Index * 2) + 1]) << 32));
		}
	};
	if (!SendAndWait(Chain, Out))
	{
		return false;
	}
	OutQuadwords.Reset();
	if (!Batch.bClip)
	{
		// The first batch after OFFSET is in the buffer at BASE.
		Read(VU1Memory::Base + (Batch.IsSkinned() ? VU1SkinnedMemory::Packet : VU1Memory::Packet),
			GetPacketQuadwords(Batch.NumVertices));
		return true;
	}
	// A clipped batch stops with each full chunk, its address in the clipping's state (x), until MSCNT; 0 there once it
	// has ended.
	for (int32 Chunk = 0; Chunk < 256; ++Chunk)
	{
		const uint32 Address = Vu1Memory[VU1Memory::ClipState * 4] & 0xffff;
		if (Address == 0)
		{
			return true;
		}
		Read(Address, GetPacketQuadwords(Vu1Memory[Address * 4] & 0x7fff));
		alignas(64) static uint64 Continue[2];
		Continue[0] = FGSGifPacket::MakeDmaTag(0, EGSDmaTag::End);
		Continue[1] = FGSGifPacket::MakeVifCodes(
			FGSGifPacket::MakeVifCode(EGSVifCommand::MsCnt), FGSGifPacket::MakeVifCode(EGSVifCommand::FlushE));
		if (!SendAndWait(Continue, Continue + 2))
		{
			return false;
		}
	}
	return false;
}
