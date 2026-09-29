#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AnimNotify.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/**
 * An object that handles an animation notify itself (UE: UAnimNotify): a game derives from it (a footstep that plays a
 * sound, a notify that spawns an effect) and places it on a timeline (FAnimNotifyEvent::Notify). The anim instance
 * calls Notify once each time a player crosses the event, after the anim update; the event is also broadcast to
 * UAnimInstance::OnAnimNotify.
 */
UCLASS()
class ENGINE_API UAnimNotify : public UObject
{
	GENERATED_BODY()

public:
	UAnimNotify(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The event fired on MeshComp from Animation (UE: Notify). The base does nothing. */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation);

	/** The name the event is broadcast with when its FAnimNotifyEvent has none (UE: GetNotifyName): the class's. */
	[[nodiscard]] virtual FName GetNotifyName() const;
};
