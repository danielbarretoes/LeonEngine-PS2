#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "StaticMeshSocket.generated.h"

class UStaticMeshComponent;

/**
 * A named point on a static mesh (UE: UStaticMeshSocket): a weapon's muzzle, where its shots and flash start. A
 * component attached to a static mesh component with the socket's name follows it; GetSocketTransform of the
 * component gives it in the world. The static mesh import makes one per glTF node named `SOCKET_<Name>` under the mesh
 * (UE's FBX convention), and the mesh keeps them (UStaticMesh::Sockets).
 */
UCLASS()
class ENGINE_API UStaticMeshSocket : public UObject
{
	GENERATED_BODY()

public:
	UStaticMeshSocket(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The name components attach with (UE: SocketName). */
	UPROPERTY()
	FName SocketName;

	/** The place in the mesh's space, cm (UE: RelativeLocation). */
	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;

	/** UE: RelativeRotation. */
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** UE: RelativeScale. */
	UPROPERTY()
	FVector RelativeScale = FVector(1.0f, 1.0f, 1.0f);

	/** Free text for the game (UE: Tag). */
	UPROPERTY()
	FString Tag;

	/** The socket in the mesh's space (Leon; UE builds it in GetSocketTransform). */
	[[nodiscard]] FTransform GetSocketLocalTransform() const
	{
		return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
	}

	/** The socket in the world, on a component showing the mesh (UE: GetSocketTransform). False without a component. */
	bool GetSocketTransform(FTransform& OutTransform, const UStaticMeshComponent* MeshComp) const;
};
