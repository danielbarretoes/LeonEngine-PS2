#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "SkeletalMeshSocket.generated.h"

/**
 * A named attachment point on a bone of a skeleton (UE: USkeletalMeshSocket): a component attached to a skeletal mesh
 * component with the socket's name follows the bone, offset by the socket's relative transform.
 */
UCLASS()
class ENGINE_API USkeletalMeshSocket : public UObject
{
	GENERATED_BODY()

public:
	USkeletalMeshSocket(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The name components attach with (UE: SocketName). */
	UPROPERTY()
	FName SocketName;

	/** The bone it follows (UE: BoneName). */
	UPROPERTY()
	FName BoneName;

	/** Offset from the bone, in the bone's space, cm (UE: RelativeLocation). */
	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;

	/** UE: RelativeRotation. */
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** UE: RelativeScale. */
	UPROPERTY()
	FVector RelativeScale = FVector(1.0f, 1.0f, 1.0f);

	/** The offset from the bone as a transform (UE: GetSocketLocalTransform). */
	[[nodiscard]] FTransform GetSocketLocalTransform() const
	{
		return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
	}
};
