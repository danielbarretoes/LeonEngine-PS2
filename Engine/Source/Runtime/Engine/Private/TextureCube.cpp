#include "Engine/TextureCube.h"

#include "Engine/Texture2D.h"

UTextureCube::UTextureCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const TCHAR* UTextureCube::GetFaceName(ECubeFace Face)
{
	switch (Face)
	{
		case ECubeFace::PosX:
			return TEXT("PosX");
		case ECubeFace::NegX:
			return TEXT("NegX");
		case ECubeFace::PosY:
			return TEXT("PosY");
		case ECubeFace::NegY:
			return TEXT("NegY");
		case ECubeFace::PosZ:
			return TEXT("PosZ");
		case ECubeFace::NegZ:
			return TEXT("NegZ");
		case ECubeFace::Num:
			break;
	}
	return TEXT("None");
}

void UTextureCube::GetFaceBasis(ECubeFace Face, FVector& OutForward, FVector& OutRight, FVector& OutUp)
{
	OutUp = FVector::UpVector;
	switch (Face)
	{
		case ECubeFace::PosX:
			OutForward = FVector(1.0f, 0.0f, 0.0f);
			break;
		case ECubeFace::NegX:
			OutForward = FVector(-1.0f, 0.0f, 0.0f);
			break;
		case ECubeFace::PosY:
			OutForward = FVector(0.0f, 1.0f, 0.0f);
			break;
		case ECubeFace::NegY:
			OutForward = FVector(0.0f, -1.0f, 0.0f);
			break;
		case ECubeFace::PosZ:
			OutForward = FVector(0.0f, 0.0f, 1.0f);
			OutUp = FVector(-1.0f, 0.0f, 0.0f);
			break;
		case ECubeFace::NegZ:
		case ECubeFace::Num:
			OutForward = FVector(0.0f, 0.0f, -1.0f);
			OutUp = FVector(1.0f, 0.0f, 0.0f);
			break;
	}
	// UE's left-handed right vector (Docs/CODING_STANDARD.md, Coordinates).
	OutRight = OutUp ^ OutForward;
}

FVector UTextureCube::GetFaceDirection(ECubeFace Face, float U, float V)
{
	FVector Forward;
	FVector Right;
	FVector Up;
	GetFaceBasis(Face, Forward, Right, Up);
	return (Forward + (Right * ((U * 2.0f) - 1.0f)) + (Up * ((V * 2.0f) - 1.0f))).GetSafeNormal();
}

bool UTextureCube::HasValidFaces() const
{
	if (Faces.Num() != int32(ECubeFace::Num))
	{
		return false;
	}
	const int32 Size = GetSizeX();
	for (const UTexture2D* Face : Faces)
	{
		if (Face == nullptr || !Face->HasValidPlatformData() || Face->GetSizeX() != Size || Face->GetSizeY() != Size)
		{
			return false;
		}
	}
	return Size > 0;
}

int32 UTextureCube::GetSizeX() const
{
	const UTexture2D* First = Faces.Num() > 0 ? Faces[0] : nullptr;
	return First != nullptr ? First->GetSizeX() : 0;
}
