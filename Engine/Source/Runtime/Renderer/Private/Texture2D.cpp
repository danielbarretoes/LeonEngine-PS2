#include "Texture2D.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{

	std::size_t PixelIndex(int X, int Y, int Size)
	{
		const auto Sx = static_cast<std::size_t>(Size);
		return (static_cast<std::size_t>(Y) * Sx) + static_cast<std::size_t>(X);
	}

} // namespace

UTexture2D::~UTexture2D()
{
	Destroy();
}

UTexture2D::UTexture2D(UTexture2D&& Other) noexcept
	: Id(Other.Id)
{
	Other.Id = 0;
}

UTexture2D& UTexture2D::operator=(UTexture2D&& Other) noexcept
{
	if (this != &Other)
	{
		Destroy();
		Id = Other.Id;
		Other.Id = 0;
	}
	return *this;
}

UTexture2D UTexture2D::Create(int Width, int Height, const unsigned char* Rgba)
{
	UTexture2D Texture;
	if (Width <= 0 || Height <= 0 || Rgba == nullptr)
	{
		return Texture;
	}

	glGenTextures(1, &Texture.Id);
	glBindTexture(GL_TEXTURE_2D, Texture.Id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Rgba);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
	return Texture;
}

UTexture2D UTexture2D::CreateChecker(int Size)
{
	Size = std::max(2, Size);

	const auto Count = PixelIndex(0, Size, Size);
	std::vector<unsigned char> Pixels(Count * 4u);
	const int Cell = std::max(1, Size / 8);
	for (int Y = 0; Y < Size; ++Y)
	{
		for (int X = 0; X < Size; ++X)
		{
			const bool bDark = ((X / Cell) + (Y / Cell)) % 2 == 0;
			const unsigned char C = bDark ? static_cast<unsigned char>(60) : static_cast<unsigned char>(220);
			const std::size_t I = PixelIndex(X, Y, Size) * 4u;
			Pixels[I + 0] = C;
			Pixels[I + 1] = C;
			Pixels[I + 2] = C;
			Pixels[I + 3] = 255;
		}
	}
	return Create(Size, Size, Pixels.data());
}

UTexture2D UTexture2D::CreateFlatNormal(int Size)
{
	Size = std::max(1, Size);
	const auto Count = PixelIndex(0, Size, Size);
	std::vector<unsigned char> Pixels(Count * 4u, 255);
	for (std::size_t I = 0; I < Pixels.size(); I += 4)
	{
		Pixels[I + 0] = 128; // X
		Pixels[I + 1] = 128; // Y
		Pixels[I + 2] = 255; // Z
		Pixels[I + 3] = 255;
	}
	return Create(Size, Size, Pixels.data());
}

UTexture2D UTexture2D::CreateBumpNormal(int Size)
{
	Size = std::max(8, Size);

	// Height field ÔåÆ finite-difference normal map (tileable, intentionally strong).
	const auto Count = PixelIndex(0, Size, Size);
	std::vector<float> Height(Count);
	constexpr float TwoPi = 6.28318530718f;
	for (int Y = 0; Y < Size; ++Y)
	{
		for (int X = 0; X < Size; ++X)
		{
			const auto U = static_cast<float>(X) / static_cast<float>(Size);
			const auto V = static_cast<float>(Y) / static_cast<float>(Size);
			// Dense ripples + circular dimples so lighting/reflections clearly warp.
			const float Ripples = (0.55f * std::sin(U * TwoPi * 8.0f) * std::cos(V * TwoPi * 6.0f)) +
				(0.30f * std::sin((U + V) * TwoPi * 10.0f));
			const float Cx = std::fmod(U * 4.0f, 1.0f) - 0.5f;
			const float Cy = std::fmod(V * 4.0f, 1.0f) - 0.5f;
			const float Dimple = 0.45f * std::exp(-18.0f * ((Cx * Cx) + (Cy * Cy)));
			Height[PixelIndex(X, Y, Size)] = Ripples + Dimple;
		}
	}

	std::vector<unsigned char> Pixels(Count * 4u);
	const float Strength = 6.0f;
	for (int Y = 0; Y < Size; ++Y)
	{
		for (int X = 0; X < Size; ++X)
		{
			const int X0 = (X + Size - 1) % Size;
			const int X1 = (X + 1) % Size;
			const int Y0 = (Y + Size - 1) % Size;
			const int Y1 = (Y + 1) % Size;
			const float HL = Height[PixelIndex(X0, Y, Size)];
			const float HR = Height[PixelIndex(X1, Y, Size)];
			const float HD = Height[PixelIndex(X, Y0, Size)];
			const float HU = Height[PixelIndex(X, Y1, Size)];
			float Nx = (HL - HR) * Strength;
			float Ny = (HD - HU) * Strength;
			float Nz = 1.0f;
			const float InvLen = 1.0f / std::sqrt(((Nx * Nx) + (Ny * Ny)) + (Nz * Nz));
			Nx *= InvLen;
			Ny *= InvLen;
			Nz *= InvLen;

			const std::size_t I = PixelIndex(X, Y, Size) * 4u;
			Pixels[I + 0] = static_cast<unsigned char>(((Nx * 0.5f) + 0.5f) * 255.0f);
			Pixels[I + 1] = static_cast<unsigned char>(((Ny * 0.5f) + 0.5f) * 255.0f);
			Pixels[I + 2] = static_cast<unsigned char>(((Nz * 0.5f) + 0.5f) * 255.0f);
			Pixels[I + 3] = 255;
		}
	}
	return Create(Size, Size, Pixels.data());
}

UTexture2D UTexture2D::LoadFromFile(const std::string& Path)
{
	stbi_set_flip_vertically_on_load(1);
	int Width = 0;
	int Height = 0;
	int Channels = 0;
	unsigned char* Data = stbi_load(Path.c_str(), &Width, &Height, &Channels, 4);
	if (Data == nullptr)
	{
		std::cerr << "Failed to load texture: " << Path << " (" << stbi_failure_reason() << ")\n";
		return {};
	}

	UTexture2D Texture = Create(Width, Height, Data);
	stbi_image_free(Data);
	return Texture;
}

void UTexture2D::Bind(unsigned int Unit) const
{
	glActiveTexture(GL_TEXTURE0 + Unit);
	glBindTexture(GL_TEXTURE_2D, Id);
}

void UTexture2D::Destroy()
{
	if (Id != 0)
	{
		glDeleteTextures(1, &Id);
		Id = 0;
	}
}
