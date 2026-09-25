// USTRUCT(NoExport) inside #if !CPP: reflection for C++ types defined elsewhere (UE: CoreUObject's NoExportTypes.h).
// No GENERATED_BODY; the generated code reads the C++ type's offsets and checks the declaration against it.
#pragma once

#include "CoreMinimal.h"
#include "NoExportTypes.generated.h"

#if !CPP

/** A point or direction in 3D space. */
USTRUCT(immutable, noexport, BlueprintType)
struct FVector
{
	UPROPERTY(EditAnywhere, SaveGame, Category = "Vector")
	float X;

	UPROPERTY(EditAnywhere, SaveGame, Category = "Vector")
	float Y;

	UPROPERTY(EditAnywhere, SaveGame, Category = "Vector")
	float Z;
};

USTRUCT(immutable, noexport)
struct FQuat
{
	UPROPERTY()
	float X;

	UPROPERTY()
	float Y;

	UPROPERTY()
	float Z;

	UPROPERTY()
	float W;
};

/** A reflected base: the layout check derives from FVector too. */
USTRUCT(immutable, noexport)
struct FPlane : public FVector
{
	UPROPERTY()
	float W;
};

/** Non-public members in the C++ type: it befriends Z_Construct_UScriptStruct_FTransform_Statics. */
USTRUCT(immutable, noexport)
struct FTransform
{
	UPROPERTY()
	FQuat Rotation;

	UPROPERTY()
	FVector Translation;

	UPROPERTY()
	FVector Scale3D;
};

#endif // !CPP

/** A normal struct holding a NoExport one. */
USTRUCT()
struct FUsesNoExport
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	TArray<FTransform> Path;
};
