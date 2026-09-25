#include "Animation/Skeleton.h"

#include "Engine/SkeletalMeshSocket.h"

USkeletalMeshSocket::USkeletalMeshSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USkeleton::USkeleton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USkeletalMeshSocket* USkeleton::FindSocket(FName InSocketName) const
{
	if (InSocketName.IsNone())
	{
		return nullptr;
	}
	for (USkeletalMeshSocket* Socket : Sockets)
	{
		if (Socket != nullptr && Socket->SocketName == InSocketName)
		{
			return Socket;
		}
	}
	return nullptr;
}

USkeletalMeshSocket* USkeleton::AddSocket(FName InSocketName, FName InBoneName, const FTransform& RelativeTransform)
{
	if (USkeletalMeshSocket* Existing = FindSocket(InSocketName))
	{
		return Existing;
	}
	// Named after the socket, so saves stay deterministic (D13); transient with a transient skeleton.
	USkeletalMeshSocket* Socket =
		NewObject<USkeletalMeshSocket>(this, InSocketName, HasAnyFlags(RF_Transient) ? RF_Transient : RF_NoFlags);
	Socket->SocketName = InSocketName;
	Socket->BoneName = InBoneName;
	Socket->RelativeLocation = RelativeTransform.GetLocation();
	Socket->RelativeRotation = RelativeTransform.Rotator();
	Socket->RelativeScale = RelativeTransform.GetScale3D();
	Sockets.Add(Socket);
	return Socket;
}

void USkeleton::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << ReferenceSkeleton;
}
