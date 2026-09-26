#pragma once

#include "CollisionResponseContainer.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CollisionProfile.generated.h"

/**
 * A collision channel the config names, and the response every body starts with on it (UE: FCustomChannelSetup):
 * `+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=True,
 * bStaticObject=False,Name="Weapon")`.
 */
USTRUCT()
struct ENGINE_API FCustomChannelSetup
{
	GENERATED_BODY()

	/** The channel (UE: Channel), one of the engine or game trace channels. */
	UPROPERTY()
	TEnumAsByte<ECollisionChannel> Channel = ECC_GameTraceChannel1;

	/** What bodies answer the channel with unless they say otherwise (UE: DefaultResponse). */
	UPROPERTY()
	TEnumAsByte<ECollisionResponse> DefaultResponse = ECR_Block;

	/** A trace channel (queries trace on it) rather than an object type (UE: bTraceType). */
	UPROPERTY()
	bool bTraceType = false;

	/** An object type of static bodies (UE: bStaticObject; informative in Leon). */
	UPROPERTY()
	bool bStaticObject = false;

	/** The channel's name (UE: Name), e.g. Weapon. */
	UPROPERTY()
	FName Name;
};

/**
 * The project's collision channels (UE: UCollisionProfile), read from [/Script/Engine.CollisionProfile] of the Engine
 * config: the names of the game channels and the response every body and query starts with on them
 * (FCollisionResponseContainer::GetDefaultResponseContainer). UE's named profiles (BlockAll, Pawn, Trigger, ...) are
 * not implemented: a component sets its object type and responses itself (UPrimitiveComponent::SetCollision*), and the
 * engine classes set UE's profile values in their constructors (the character's capsule is UE's Pawn profile).
 */
UCLASS(Config = Engine, DefaultConfig)
class ENGINE_API UCollisionProfile : public UObject
{
	GENERATED_BODY()

public:
	UCollisionProfile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The named channels (UE: DefaultChannelResponses). */
	UPROPERTY(Config)
	TArray<FCustomChannelSetup> DefaultChannelResponses;

	/** The settings (UE: Get): the class default object. */
	[[nodiscard]] static UCollisionProfile* Get();

	/**
	 * Applies DefaultChannelResponses to the default response container (UE: LoadProfileConfig). The class default
	 * object does it when the config loads it; bodies and components made before keep the responses they had.
	 */
	void LoadProfileConfig(bool bForceInit = false);

	/** A channel's name: the engine channels' own, a game channel's from the config, else None (UE). */
	[[nodiscard]] FName ReturnChannelNameFromContainerIndex(int32 ContainerIndex) const;

	/** The channel of a name, INDEX_NONE when no channel has it (UE: ReturnContainerIndexFromChannelName). */
	[[nodiscard]] int32 ReturnContainerIndexFromChannelName(FName DisplayName) const;

	void PostInitProperties() override;
};
