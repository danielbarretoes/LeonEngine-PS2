#include "Engine/Texture2D.h"

#include "Containers/StringConv.h"
#include "EngineLogs.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{

	int32 PixelIndex(int32 X, int32 Y, int32 Size)
	{
		return (Y * Size) + X;
	}

} // namespace

UTexture2D UTexture2D::Create(int32 Width, int32 Height, const uint8* Rgba)
{
	UTexture2D Texture;
	if (Width <= 0 || Height <= 0 || Rgba == nullptr)
	{
		return Texture;
	}
	Texture.SizeX = Width;
	Texture.SizeY = Height;
	Texture.Pixels.Append(Rgba, Width * Height * 4);
	return Texture;
}

UTexture2D UTexture2D::CreateChecker(int32 Size)
{
	Size = FMath::Max(2, Size);

	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Size * Size * 4);
	const int32 Cell = FMath::Max(1, Size / 8);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const bool bDark = ((X / Cell) + (Y / Cell)) % 2 == 0;
			const uint8 C = bDark ? static_cast<uint8>(60) : static_cast<uint8>(220);
			const int32 I = PixelIndex(X, Y, Size) * 4;
			Pixels[I + 0] = C;
			Pixels[I + 1] = C;
			Pixels[I + 2] = C;
			Pixels[I + 3] = 255;
		}
	}
	return Create(Size, Size, Pixels.GetData());
}

UTexture2D UTexture2D::CreateFlatNormal(int32 Size)
{
	Size = FMath::Max(1, Size);
	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Size * Size * 4);
	for (int32 I = 0; I < Pixels.Num(); I += 4)
	{
		Pixels[I + 0] = 128; // X
		Pixels[I + 1] = 128; // Y
		Pixels[I + 2] = 255; // Z
		Pixels[I + 3] = 255;
	}
	return Create(Size, Size, Pixels.GetData());
}

UTexture2D UTexture2D::CreateBumpNormal(int32 Size)
{
	Size = FMath::Max(8, Size);

	// Height field to a finite-difference normal map (tileable, intentionally strong).
	TArray<float> Height;
	Height.SetNumUninitialized(Size * Size);
	constexpr float TwoPi = 6.28318530718f;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const float U = static_cast<float>(X) / static_cast<float>(Size);
			const float V = static_cast<float>(Y) / static_cast<float>(Size);
			// Dense ripples + circular dimples so lighting / reflections clearly warp.
			const float Ripples = (0.55f * FMath::Sin(U * TwoPi * 8.0f) * FMath::Cos(V * TwoPi * 6.0f)) +
				(0.30f * FMath::Sin((U + V) * TwoPi * 10.0f));
			const float Cx = FMath::Fmod(U * 4.0f, 1.0f) - 0.5f;
			const float Cy = FMath::Fmod(V * 4.0f, 1.0f) - 0.5f;
			const float Dimple = 0.45f * FMath::Exp(-18.0f * ((Cx * Cx) + (Cy * Cy)));
			Height[PixelIndex(X, Y, Size)] = Ripples + Dimple;
		}
	}

	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Size * Size * 4);
	const float Strength = 6.0f;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const int32 X0 = (X + Size - 1) % Size;
			const int32 X1 = (X + 1) % Size;
			const int32 Y0 = (Y + Size - 1) % Size;
			const int32 Y1 = (Y + 1) % Size;
			const float HL = Height[PixelIndex(X0, Y, Size)];
			const float HR = Height[PixelIndex(X1, Y, Size)];
			const float HD = Height[PixelIndex(X, Y0, Size)];
			const float HU = Height[PixelIndex(X, Y1, Size)];
			float Nx = (HL - HR) * Strength;
			float Ny = (HD - HU) * Strength;
			float Nz = 1.0f;
			const float InvLen = 1.0f / FMath::Sqrt(((Nx * Nx) + (Ny * Ny)) + (Nz * Nz));
			Nx *= InvLen;
			Ny *= InvLen;
			Nz *= InvLen;

			const int32 I = PixelIndex(X, Y, Size) * 4;
			Pixels[I + 0] = static_cast<uint8>(((Nx * 0.5f) + 0.5f) * 255.0f);
			Pixels[I + 1] = static_cast<uint8>(((Ny * 0.5f) + 0.5f) * 255.0f);
			Pixels[I + 2] = static_cast<uint8>(((Nz * 0.5f) + 0.5f) * 255.0f);
			Pixels[I + 3] = 255;
		}
	}
	return Create(Size, Size, Pixels.GetData());
}

UTexture2D UTexture2D::LoadFromFile(const FString& Path)
{
	stbi_set_flip_vertically_on_load(1);
	int32 Width = 0;
	int32 Height = 0;
	int32 Channels = 0;
	uint8* Data = stbi_load(TCHAR_TO_UTF8(*Path), &Width, &Height, &Channels, 4);
	if (Data == nullptr)
	{
		UE_LOG(LogEngine, Error, "Failed to load texture: %s (%s)", *Path, stbi_failure_reason());
		return {};
	}

	UTexture2D Texture = Create(Width, Height, Data);
	stbi_image_free(Data);
	return Texture;
}
