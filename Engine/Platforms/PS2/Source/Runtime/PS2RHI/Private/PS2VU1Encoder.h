#pragma once

// Private VU1 encoding for the PS2 RHI plugin (not a public Engine header).

#include "GSDrawEnvironment.h"
#include "GSGifPacket.h"
#include "GSVertexBatch.h"

namespace Leon::PS2
{

	/**
	 * VU1's data memory as VU1Programs.vsm reads it (quadwords): the shared constants, VIF1's double buffer, and a
	 * batch's header, streams and GIF packet in its buffer (offsets from TOP).
	 */
	namespace VU1Memory
	{
		/** The screen's scale and offset and the limits (255, 0.5, ZMax / 16). */
		constexpr uint32 ScreenScale = 0;
		constexpr uint32 ScreenOffset = 1;
		constexpr uint32 Limits = 2;
		/**
		 * The clipping's constants (ps2-polish P8b, ClipTriangles.vsi): the vectors of a position's outcodes (the
		 * view's sides, the guard band's, near and far) and the six clip planes (near, far, then the guard band's
		 * right, left, top and bottom).
		 */
		constexpr uint32 OutcodeVectors = 3;
		constexpr uint32 NumOutcodeVectors = 6;
		constexpr uint32 ClipPlanes = OutcodeVectors + NumOutcodeVectors;
		constexpr uint32 NumClipPlanes = 6;
		constexpr uint32 NumShared = ClipPlanes + NumClipPlanes;
		/** The clipping's state, the program's own: the chunk handed over (x, VU1Conformance), the chunks, TOP. */
		constexpr uint32 ClipState = NumShared;
		/** VIF1's BASE and OFFSET: two buffers of 504 quadwords after the 16 of the shared constants. */
		constexpr uint32 Base = 16;
		constexpr uint32 Offset = 504;
		/**
		 * A buffer: the header (9 quadwords, 23 lit: the normal's transform, the sun and the ambient, then the
		 * position's transform to the world and two point lights, N29), the four streams and the GIF packet.
		 */
		constexpr uint32 HeaderUnlit = 9;
		constexpr uint32 HeaderLit = 23;
		constexpr uint32 GifTag = 8;
		constexpr uint32 ToWorld = 15;
		constexpr uint32 PointLights = 19;
		constexpr uint32 MaxPointLights = 2;
		constexpr uint32 Positions = 24;
		constexpr uint32 Normals = 88;
		constexpr uint32 Colors = 152;
		constexpr uint32 TexCoords = 216;
		constexpr uint32 Packet = 280;
		/**
		 * A clipped batch's (P8b): each vertex's record in its own rows of the streams, two polygons (10 vertices of 3
		 * quadwords: 9 and the first again) and two chunks of GIF packets (VU1Programs.vsm's CLIP_PROGRAM).
		 */
		constexpr uint32 ClipPolygonQuadwords = 30;
		constexpr uint32 ClipPolygons = Packet;
		constexpr uint32 ClipChunks = ClipPolygons + (2 * ClipPolygonQuadwords);
		constexpr uint32 ClipChunkQuadwords = 82;
		static_assert(PointLights + (2 * MaxPointLights) == HeaderLit && HeaderLit <= Positions, "The lit header");
		static_assert(MaxPointLights == FGSVertexDraw::MaxVU1PointLights, "What GetProgram lets through");
		static_assert(Packet + 1 + (3 * FGSVertexBatch::MaxVertices) <= Offset, "A batch fits its buffer");
		static_assert(ClipChunks == 340 && ClipChunks + (2 * ClipChunkQuadwords) <= Offset, "A clipped batch's too");
		static_assert(ClipState < Base && Base + (2 * Offset) <= 1024, "The buffers fit VU1's 16 KB");
	} // namespace VU1Memory

	/**
	 * A skinned batch's buffer as Skinned.vsm reads it (N14b, offsets from TOP): the header (StaticLit's 23 quadwords,
	 * the position's transforms unscaled, then the quantization's scale with 1/255 in W and its bias), the palette (3
	 * quadwords a bone), the five streams (the skin last) and the GIF packet.
	 */
	namespace VU1SkinnedMemory
	{
		constexpr uint32 Quantization = 23;
		constexpr uint32 Header = 25;
		constexpr uint32 Palette = 25;
		constexpr uint32 Positions = Palette + (3 * FGSVertexBatch::MaxBones);
		constexpr uint32 Normals = Positions + FGSVertexBatch::MaxSkinnedVertices;
		constexpr uint32 Colors = Normals + FGSVertexBatch::MaxSkinnedVertices;
		constexpr uint32 TexCoords = Colors + FGSVertexBatch::MaxSkinnedVertices;
		constexpr uint32 Skin = TexCoords + FGSVertexBatch::MaxSkinnedVertices;
		constexpr uint32 Packet = Skin + FGSVertexBatch::MaxSkinnedVertices;
		/** A clipped batch's polygons and chunks (Skinned.vsm's CLIP_PROGRAM, P8b). */
		constexpr uint32 ClipPolygons = Packet;
		constexpr uint32 ClipChunks = ClipPolygons + (2 * VU1Memory::ClipPolygonQuadwords);
		constexpr uint32 ClipChunkQuadwords = 53;
		static_assert(Positions == 97 && Packet == 337, "Skinned.vsm's offsets");
		static_assert(
			Packet + 1 + (3 * FGSVertexBatch::MaxSkinnedVertices) <= VU1Memory::Offset, "A batch fits its buffer");
		static_assert(
			ClipChunks == 397 && ClipChunks + (2 * ClipChunkQuadwords) <= VU1Memory::Offset, "A clipped batch's too");
	} // namespace VU1SkinnedMemory

	/** The microprograms' start addresses in the micro memory (in instructions, MSCAL's). */
	[[nodiscard]] uint32 GetProgramAddress(EGSVertexProgram Program);

	/**
	 * Writes a list's vertex batches into its VIF1 chain for VU1Programs.vsm (FGSGifPacket::BuildChain): the prologue
	 * sets VIF1's cycle, mode and double buffer and unpacks the shared constants; each batch is a CNT with its header
	 * (unpacked at TOPS), a REF with an UNPACK to each of the mesh's streams where the mesh keeps them (uncopied), and
	 * MSCAL of its program. The header's draw-wide quadwords are computed once per draw. A skinned batch has a longer
	 * header, a REF to its palette in the list's memory and a fifth stream, the skin (VU1SkinnedMemory).
	 */
	class FPS2VU1BatchEncoder final : public IGSVertexBatchEncoder
	{
	public:
		/** Before a list's chain: its screen, and whether each batch's program sends its packet (XGKICK). */
		void Begin(const FGSDrawEnvironment& InEnvironment, bool bInKick);

		[[nodiscard]] uint32 GetPrologueQuadwords() const override;
		[[nodiscard]] uint32 GetMaxBatchQuadwords() const override;
		uint64* WritePrologue(uint64* Out) override;
		uint64* WriteBatch(const FGSCommandList& List, const FGSVertexBatch& Batch, uint64* Out) override;

	private:
		/** The header's quadwords of a draw (as four 32-bit words each), for WriteBatch to complete per batch. */
		void PrepareDraw(const FGSVertexDraw& Draw);
		/** Four rows of the header from FirstRow: Matrix with the positions' scale and bias folded in. */
		void PrepareRows(const FMatrix& Matrix, const float* Scale, const float* Bias, uint32 FirstRow);
		/** Whether the header's program is a Skinned one, and whether it lights. */
		[[nodiscard]] bool IsHeaderSkinned() const;
		[[nodiscard]] bool IsHeaderLit() const;

		FGSDrawEnvironment Environment;
		bool bKick = true;
		/** The draw the header holds (its list and index). */
		const FGSCommandList* HeaderList = nullptr;
		int32 HeaderDraw = INDEX_NONE;
		EGSVertexProgram HeaderProgram = EGSVertexProgram::StaticUnlit;
		/** The draw's PRIM: its strips' (TRISTRIP), and a clipped batch's triangles' (TRIANGLE, P8b). */
		uint64 HeaderPrim = 0;
		uint64 HeaderClipPrim = 0;
		/** The header's z: 0 unlit, 1 lit, 2 or 3 lit with one or two point lights. */
		uint32 HeaderLighting = 0;
		alignas(16) uint32 Header[VU1SkinnedMemory::Header * 4] = {};
	};

} // namespace Leon::PS2
