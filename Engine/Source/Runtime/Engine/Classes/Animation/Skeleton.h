#pragma once

#include "CoreMinimal.h"
#include "SkeletalAnimation.h"
#include "UObject/Object.h"
#include "Skeleton.generated.h"

class USkeletalMeshSocket;

/**
 * A skeleton asset (UE: USkeleton): the bone hierarchy the skeletal meshes and the animations of a character share,
 * and its sockets. The bones (a FReferenceSkeleton: names, parents, inverse bind pose) are native data after the
 * tagged properties; the sockets are inner objects.
 *
 * Leon has no retargeting, virtual bones, slot groups or curves.
 */
UCLASS()
class ENGINE_API USkeleton : public UObject
{
	GENERATED_BODY()

public:
	USkeleton(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Attachment points on the bones (UE: Sockets). */
	UPROPERTY()
	TArray<USkeletalMeshSocket*> Sockets;

	/** The bones (UE: GetReferenceSkeleton). */
	[[nodiscard]] const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return ReferenceSkeleton;
	}
	/** Replaces the bones (Leon; UE merges a mesh's bones into the tree). */
	void SetReferenceSkeleton(const FReferenceSkeleton& InReferenceSkeleton)
	{
		ReferenceSkeleton = InReferenceSkeleton;
	}

	/** The socket called InSocketName, or null (UE: FindSocket). */
	[[nodiscard]] USkeletalMeshSocket* FindSocket(FName InSocketName) const;

	/**
	 * Adds a socket on InBoneName, offset by RelativeTransform, as an inner object of the skeleton (Leon; UE's editor
	 * adds them). Returns the existing socket when the name is taken.
	 */
	USkeletalMeshSocket* AddSocket(
		FName InSocketName, FName InBoneName, const FTransform& RelativeTransform = FTransform::Identity);

	/** The tagged properties, then the bones. */
	void Serialize(FArchive& Ar) override;

private:
	FReferenceSkeleton ReferenceSkeleton;
};
