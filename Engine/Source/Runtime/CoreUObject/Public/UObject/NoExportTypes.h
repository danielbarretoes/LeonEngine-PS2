#pragma once

// Reflection of Core types (UE: UObject/NoExportTypes.h). The C++ types live in Core, which knows nothing about
// reflection; the USTRUCT(noexport) declarations below are only read by LeonHeaderTool (CPP is 1 for the compiler).
// They generate Z_Construct_UScriptStruct_FVector and friends, so UPROPERTY() FVector X; works in any module. The
// generated code takes the offsets from the Core types and checks, at compile time, that each declaration here matches
// its Core type (members, types, offsets and size). FMatrix is not reflected: its C++ layout is float M[4][4].

#include "CoreMinimal.h"
#include "Math/Box.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntVector.h"
#include "Math/Plane.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Misc/Guid.h"
#include "UObject/ObjectMacros.h"
#include "NoExportTypes.generated.h"

#if !CPP

/** A point or direction in 3D space: X forward, Y right, Z up, in centimetres. */
USTRUCT(immutable, noexport, BlueprintType)
struct FVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector)
	float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector)
	float Z;
};

/** A 2D vector. */
USTRUCT(immutable, noexport, BlueprintType)
struct FVector2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector2D)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector2D)
	float Y;
};

/** A 4D homogeneous vector. */
USTRUCT(immutable, noexport, BlueprintType)
struct FVector4
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector4)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector4)
	float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector4)
	float Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Vector4)
	float W;
};

/** A plane: the normal (X, Y, Z) and W, with X * Px + Y * Py + Z * Pz = W. */
USTRUCT(immutable, noexport, BlueprintType)
struct FPlane : public FVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Plane)
	float W;
};

/** An orientation in degrees: Pitch (up), Yaw (right), Roll. */
USTRUCT(immutable, noexport, BlueprintType)
struct FRotator
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Rotator)
	float Pitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Rotator)
	float Yaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Rotator)
	float Roll;
};

/** A rotation quaternion. */
USTRUCT(immutable, noexport, BlueprintType)
struct FQuat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Quat)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Quat)
	float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Quat)
	float Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Quat)
	float W;
};

/** Rotation, translation and 3D scale. Core's FTransform befriends the generated _Statics for these offsets. */
USTRUCT(immutable, noexport, BlueprintType)
struct FTransform
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Transform)
	FQuat Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Transform)
	FVector Translation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Transform)
	FVector Scale3D;
};

/** An 8-bit sRGB color, stored B, G, R, A. */
USTRUCT(immutable, noexport, BlueprintType)
struct FColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Color)
	uint8 B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Color)
	uint8 G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Color)
	uint8 R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Color)
	uint8 A;
};

/** A linear-space color. */
USTRUCT(noexport, BlueprintType)
struct FLinearColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = LinearColor)
	float R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = LinearColor)
	float G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = LinearColor)
	float B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = LinearColor)
	float A;
};

/** A 128-bit globally unique id (Leon: the members are uint32, as in the Core type). */
USTRUCT(immutable, noexport, BlueprintType)
struct FGuid
{
	UPROPERTY(EditAnywhere, SaveGame, Category = Guid)
	uint32 A;

	UPROPERTY(EditAnywhere, SaveGame, Category = Guid)
	uint32 B;

	UPROPERTY(EditAnywhere, SaveGame, Category = Guid)
	uint32 C;

	UPROPERTY(EditAnywhere, SaveGame, Category = Guid)
	uint32 D;
};

/** A 2D integer point. */
USTRUCT(immutable, noexport, BlueprintType)
struct FIntPoint
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = IntPoint)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = IntPoint)
	int32 Y;
};

/** A 3D integer vector. */
USTRUCT(immutable, noexport, BlueprintType)
struct FIntVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = IntVector)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = IntVector)
	int32 Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = IntVector)
	int32 Z;
};

/** An axis-aligned bounding box; IsValid is 0 for an empty box. */
USTRUCT(immutable, noexport, BlueprintType)
struct FBox
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Box)
	FVector Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Box)
	FVector Max;

	UPROPERTY()
	uint8 IsValid;
};

#endif // !CPP
