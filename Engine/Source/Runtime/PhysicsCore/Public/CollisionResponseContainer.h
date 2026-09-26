#pragma once

#include "CoreMinimal.h"
#include "CollisionResponseContainer.generated.h"

/**
 * A collision channel (UE: ECollisionChannel, Engine/Classes/Engine/EngineTypes.h there; Leon keeps the collision types
 * next to FHitResult in PhysicsCore). A channel is both what a body is (its object type) and what a query traces
 * (its trace channel): a query on a channel hits the bodies whose response to that channel is not Ignore, and whose
 * object type the query does not ignore (FCollisionResponseParams). The game channels are named in the Engine config
 * ([/Script/Engine.CollisionProfile] DefaultChannelResponses, UCollisionProfile).
 */
UENUM()
enum ECollisionChannel
{
	ECC_WorldStatic,
	ECC_WorldDynamic,
	ECC_Pawn,
	ECC_Visibility,
	ECC_Camera,
	ECC_PhysicsBody,
	ECC_Vehicle,
	ECC_Destructible,

	/** Reserved for the engine. */
	ECC_EngineTraceChannel1,
	ECC_EngineTraceChannel2,
	ECC_EngineTraceChannel3,
	ECC_EngineTraceChannel4,
	ECC_EngineTraceChannel5,
	ECC_EngineTraceChannel6,

	/** Free for games: name them in the config (UE: the project's collision channels). */
	ECC_GameTraceChannel1,
	ECC_GameTraceChannel2,
	ECC_GameTraceChannel3,
	ECC_GameTraceChannel4,
	ECC_GameTraceChannel5,
	ECC_GameTraceChannel6,
	ECC_GameTraceChannel7,
	ECC_GameTraceChannel8,
	ECC_GameTraceChannel9,
	ECC_GameTraceChannel10,
	ECC_GameTraceChannel11,
	ECC_GameTraceChannel12,
	ECC_GameTraceChannel13,
	ECC_GameTraceChannel14,
	ECC_GameTraceChannel15,
	ECC_GameTraceChannel16,
	ECC_GameTraceChannel17,
	ECC_GameTraceChannel18,

	/** UE keeps it for old content; nothing uses it. */
	ECC_OverlapAll_Deprecated,
	ECC_MAX,
};

/** How a body answers a channel (UE: ECollisionResponse). The weaker of two responses wins. */
UENUM()
enum ECollisionResponse
{
	/** The query passes through. */
	ECR_Ignore,
	/** The query reports the body without stopping (a touch; Multi queries only). */
	ECR_Overlap,
	/** The query stops at the body. */
	ECR_Block,
	ECR_MAX,
};

/**
 * One response per channel (UE: FCollisionResponseContainer): a body's responses to the trace channels, or a query's
 * responses to the bodies' object types (FCollisionResponseParams). The members are UE's names, one byte each, so a map
 * saves only the channels a component changed.
 */
USTRUCT()
struct PHYSICSCORE_API FCollisionResponseContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> WorldStatic;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> WorldDynamic;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> Pawn;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> Visibility;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> Camera;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> PhysicsBody;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> Vehicle;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> Destructible;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> EngineTraceChannel1;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> EngineTraceChannel2;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> EngineTraceChannel3;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> EngineTraceChannel4;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> EngineTraceChannel5;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> EngineTraceChannel6;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel1;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel2;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel3;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel4;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel5;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel6;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel7;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel8;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel9;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel10;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel11;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel12;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel13;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel14;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel15;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel16;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel17;

	UPROPERTY()
	TEnumAsByte<ECollisionResponse> GameTraceChannel18;

	/** Number of channels a container holds (UE: the size of EnumArray). */
	static constexpr int32 NumChannels = 32;

	/** Every channel blocks (UE's constructor). */
	FCollisionResponseContainer();
	/** Every channel answers DefaultResponse (UE). */
	explicit FCollisionResponseContainer(ECollisionResponse DefaultResponse);

	/** Sets one channel; true when it changed (UE: SetResponse). A channel past the container is ignored. */
	bool SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse);
	/** Sets every channel; true when one changed (UE: SetAllChannels). */
	bool SetAllChannels(ECollisionResponse NewResponse);
	/** Changes every channel that answers OldResponse to NewResponse; true when one changed (UE: ReplaceChannels). */
	bool ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse);
	/** The response to a channel; Ignore for a channel past the container (UE: GetResponse). */
	[[nodiscard]] ECollisionResponse GetResponse(ECollisionChannel Channel) const;

	/** The weaker response of A and B on every channel (UE: CreateMinContainer). */
	[[nodiscard]] static FCollisionResponseContainer CreateMinContainer(
		const FCollisionResponseContainer& A, const FCollisionResponseContainer& B);

	/**
	 * The responses a new body or query starts from (UE: GetDefaultResponseContainer): every channel blocks, except
	 * what the config's DefaultChannelResponses change (UCollisionProfile::LoadProfileConfig).
	 */
	[[nodiscard]] static const FCollisionResponseContainer& GetDefaultResponseContainer()
	{
		return DefaultResponseContainer;
	}

	bool operator==(const FCollisionResponseContainer& Other) const;
	bool operator!=(const FCollisionResponseContainer& Other) const
	{
		return !(*this == Other);
	}

private:
	/** UCollisionProfile (Engine) writes the config's defaults (UE: a friend there too). */
	friend class UCollisionProfile;

	[[nodiscard]] TEnumAsByte<ECollisionResponse>* GetResponseRef(int32 Index);
	[[nodiscard]] const TEnumAsByte<ECollisionResponse>* GetResponseRef(int32 Index) const;

	static FCollisionResponseContainer DefaultResponseContainer;
};
