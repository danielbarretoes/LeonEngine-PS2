#pragma once

#include "CoreTypes.h"

/** GS VRAM texture (RGBA8 -> PSMCT32). Accessors match host Texture Create / Bind / Valid. */
class PS2RHI_API FPS2Texture
{
public:
	FPS2Texture() = default;
	~FPS2Texture();

	FPS2Texture(const FPS2Texture&) = delete;
	FPS2Texture& operator=(const FPS2Texture&) = delete;
	FPS2Texture(FPS2Texture&& Other) noexcept;
	FPS2Texture& operator=(FPS2Texture&& Other) noexcept;

	static FPS2Texture Create(int InWidth, int InHeight, const unsigned char* Rgba);

	/** Content name: T_Checker_D */
	static FPS2Texture CreateChecker(int Size = 64);

	/** Content name: T_Grid_D */
	static FPS2Texture CreateGrid(int Size = 64);

	void Destroy();
	bool Valid() const;

	/** Binds TEX0 / sampling for subsequent textured draws. */
	void Bind() const;

	int GetWidth() const
	{
		return Width;
	}

	int GetHeight() const
	{
		return Height;
	}

	int GetVramAddress() const
	{
		return VramAddress;
	}

private:
	explicit FPS2Texture(int InWidth, int InHeight, int InVramAddress, int InBufferWidth)
		: Width(InWidth)
		, Height(InHeight)
		, VramAddress(InVramAddress)
		, BufferWidth(InBufferWidth)
	{
	}

	/** `rgba` must be 16-byte aligned. Uploads to VRAM; does not free `rgba`. */
	static FPS2Texture CreateFromAlignedRgba(int InWidth, int InHeight, unsigned char* Rgba);

	int Width = 0;
	int Height = 0;
	int VramAddress = 0;
	int BufferWidth = 0;
};
