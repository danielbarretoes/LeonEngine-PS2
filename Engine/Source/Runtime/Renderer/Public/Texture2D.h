#pragma once

#include "CoreMinimal.h"
#include "RHIHandles.h"

/** 2D GPU texture (RGBA8). */
class RENDERER_API UTexture2D
{
public:
	UTexture2D() = default;
	~UTexture2D();

	UTexture2D(const UTexture2D&) = delete;
	UTexture2D& operator=(const UTexture2D&) = delete;
	UTexture2D(UTexture2D&& Other) noexcept;
	UTexture2D& operator=(UTexture2D&& Other) noexcept;

	[[nodiscard]] static UTexture2D Create(int32 Width, int32 Height, const uint8* Rgba);
	[[nodiscard]] static UTexture2D CreateChecker(int32 Size = 64);
	/** Flat normal map in tangent space (points along +Z). */
	[[nodiscard]] static UTexture2D CreateFlatNormal(int32 Size = 4);
	/** Strong procedural bumps for demo normal mapping (tileable). */
	[[nodiscard]] static UTexture2D CreateBumpNormal(int32 Size = 256);
	[[nodiscard]] static UTexture2D LoadFromFile(const FString& Path);

	void Bind(uint32 Unit = 0) const;
	[[nodiscard]] bool Valid() const
	{
		return Id != 0;
	}

private:
	void Destroy();

	FRHITextureId Id = InvalidTexture;
};
