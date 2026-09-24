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

	static FPS2Texture Create(int width, int height, const unsigned char* rgba);

	/** Content name: T_Checker_D */
	static FPS2Texture CreateChecker(int size = 64);

	/** Content name: T_Grid_D */
	static FPS2Texture CreateGrid(int size = 64);

	void Destroy();
	bool Valid() const;

	/** Binds TEX0 / sampling for subsequent textured draws. */
	void Bind() const;

	int Width() const
	{
		return width_;
	}

	int Height() const
	{
		return height_;
	}

	int VramAddress() const
	{
		return vramAddress_;
	}

private:
	explicit FPS2Texture(int width, int height, int vramAddress, int bufferWidth)
		: width_(width)
		, height_(height)
		, vramAddress_(vramAddress)
		, bufferWidth_(bufferWidth)
	{
	}

	/** `rgba` must be 16-byte aligned. Uploads to VRAM; does not free `rgba`. */
	static FPS2Texture CreateFromAlignedRgba(int width, int height, unsigned char* rgba);

	int width_ = 0;
	int height_ = 0;
	int vramAddress_ = 0;
	int bufferWidth_ = 0;
};
