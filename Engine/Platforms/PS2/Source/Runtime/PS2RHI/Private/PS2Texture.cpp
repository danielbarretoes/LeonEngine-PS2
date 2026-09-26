#include "Containers/Array.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"
#include "PS2SceneState.h"

#include <graph.h>
#include <gs_psm.h>

namespace
{

	[[nodiscard]] int NextPow2(int V)
	{
		int P = 1;
		while (P < V)
		{
			P <<= 1;
		}
		return P;
	}

	/** TEX0's TW / TH: the log2 of the power of two that holds Size texels. */
	[[nodiscard]] uint8 SizeLog2(int Size)
	{
		uint8 Log = 0;
		while ((1 << Log) < Size)
		{
			++Log;
		}
		return Log;
	}

	void FillChecker(unsigned char* Rgba, int Size)
	{
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				const int Cell = ((X >> 3) ^ (Y >> 3)) & 1;
				const unsigned char C = Cell ? static_cast<unsigned char>(220) : static_cast<unsigned char>(40);
				const int I = (Y * Size + X) * 4;
				Rgba[I + 0] = C;
				Rgba[I + 1] = C;
				Rgba[I + 2] = Cell ? static_cast<unsigned char>(200) : static_cast<unsigned char>(50);
				Rgba[I + 3] = 255;
			}
		}
	}

	void FillGrid(unsigned char* Rgba, int Size)
	{
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				const bool bLine = (X & 7) == 0 || (Y & 7) == 0;
				const int I = (Y * Size + X) * 4;
				Rgba[I + 0] = bLine ? static_cast<unsigned char>(70) : static_cast<unsigned char>(160);
				Rgba[I + 1] = bLine ? static_cast<unsigned char>(90) : static_cast<unsigned char>(170);
				Rgba[I + 2] = bLine ? static_cast<unsigned char>(80) : static_cast<unsigned char>(150);
				Rgba[I + 3] = 255;
			}
		}
	}

} // namespace

FPS2Texture FPS2Texture::CreateFromRgba(int InWidth, int InHeight, const unsigned char* Rgba)
{
	auto& Gs = Leon::PS2::GetGSContext();
	const int32 NumBytes = InWidth * InHeight * 4;
	if (!Gs.bReady || Rgba == nullptr || InWidth <= 0 || InHeight <= 0 || NumBytes % 16 != 0 ||
		!FGSCommandList::IsSupportedUpload(EGSPixelFormat::PSMCT32, 0, uint16(InWidth)))
	{
		return {};
	}
	// The buffer width is TBW's unit, 64 texels.
	const int BufW = NextPow2(InWidth < 64 ? 64 : InWidth);
	const int Addr = Leon::PS2::AllocateVram(BufW, InHeight, GS_PSM_32, GRAPH_ALIGN_BLOCK);
	if (Addr < 0)
	{
		return {};
	}
	// The upload goes out with the frame, before any draw that samples it.
	FGSBitBltBuf Destination;
	Destination.DBP = uint16(Addr / 64);
	Destination.DBW = uint8(BufW / 64);
	Destination.DPSM = EGSPixelFormat::PSMCT32;
	Gs.FrameList.UploadImage(Destination, 0, 0, uint16(InWidth), uint16(InHeight), MakeArrayView(Rgba, NumBytes));
	Gs.FrameList.TexFlush();
	return FPS2Texture(InWidth, InHeight, Addr, BufW);
}

FPS2Texture::~FPS2Texture()
{
	Destroy();
}

FPS2Texture::FPS2Texture(FPS2Texture&& Other) noexcept
	: Width(Other.Width)
	, Height(Other.Height)
	, VramAddress(Other.VramAddress)
	, BufferWidth(Other.BufferWidth)
{
	Other.Width = 0;
	Other.Height = 0;
	Other.VramAddress = 0;
	Other.BufferWidth = 0;
}

FPS2Texture& FPS2Texture::operator=(FPS2Texture&& Other) noexcept
{
	if (this != &Other)
	{
		Destroy();
		Width = Other.Width;
		Height = Other.Height;
		VramAddress = Other.VramAddress;
		BufferWidth = Other.BufferWidth;
		Other.Width = 0;
		Other.Height = 0;
		Other.VramAddress = 0;
		Other.BufferWidth = 0;
	}
	return *this;
}

void FPS2Texture::Destroy()
{
	if (VramAddress != 0)
	{
		Leon::PS2::InvalidateBoundTexture();
	}
	Width = 0;
	Height = 0;
	VramAddress = 0;
	BufferWidth = 0;
}

bool FPS2Texture::Valid() const
{
	return Width > 0 && Height > 0 && VramAddress > 0;
}

FPS2Texture FPS2Texture::Create(int InWidth, int InHeight, const unsigned char* Rgba)
{
	return CreateFromRgba(InWidth, InHeight, Rgba);
}

FPS2Texture FPS2Texture::CreateChecker(int Size)
{
	Size = Size < 8 ? 8 : Size;
	TArray<unsigned char> Rgba;
	Rgba.SetNumUninitialized(Size * Size * 4);
	FillChecker(Rgba.GetData(), Size);
	return CreateFromRgba(Size, Size, Rgba.GetData());
}

FPS2Texture FPS2Texture::CreateGrid(int Size)
{
	Size = Size < 8 ? 8 : Size;
	TArray<unsigned char> Rgba;
	Rgba.SetNumUninitialized(Size * Size * 4);
	FillGrid(Rgba.GetData(), Size);
	return CreateFromRgba(Size, Size, Rgba.GetData());
}

void FPS2Texture::Bind() const
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Valid() || !Gs.bReady)
	{
		return;
	}
	auto& Scene = Leon::PS2::GetSceneState();
	if (Scene.BoundTextureVram == VramAddress)
	{
		return;
	}

	FGSTex0 Tex0;
	Tex0.TBP0 = uint16(VramAddress / 64);
	Tex0.TBW = uint8(BufferWidth / 64);
	Tex0.PSM = EGSPixelFormat::PSMCT32;
	Tex0.TW = SizeLog2(Width);
	Tex0.TH = SizeLog2(Height);
	// RGB: the texel alpha is ignored (a bad A would otherwise punch holes).
	Tex0.bRGBA = false;
	Tex0.TFX = EGSTextureFunction::Modulate;
	FGSTex1 Tex1;
	Tex1.bFixedLOD = true;
	Tex1.MMAG = EGSFilter::Linear;
	Tex1.MMIN = EGSFilter::Linear;
	Gs.FrameList.SetTex1(0, Tex1);
	Gs.FrameList.SetTex0(0, Tex0);
	Scene.BoundTextureVram = VramAddress;
}
