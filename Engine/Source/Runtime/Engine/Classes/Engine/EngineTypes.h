#pragma once

#include "CoreMinimal.h"

/** What a world is for (UE: EWorldType). */
namespace EWorldType
{
	enum Type : uint8
	{
		None,
		/** The game's world (LeonGame, tests). */
		Game,
		Editor,
		PIE,
		EditorPreview,
		GamePreview,
		GameRPC,
		Inactive,
	};
} // namespace EWorldType

/** Why an actor or a component stops playing (UE: EEndPlayReason, EngineTypes.h). */
namespace EEndPlayReason
{
	enum Type : uint8
	{
		/** The actor or the component was destroyed explicitly (AActor::Destroy, UWorld::DestroyActor). */
		Destroyed,
		/** The level changed (P13: UEngine::LoadMap). */
		LevelTransition,
		/** Play in editor ended (unused: Leon has no editor yet). */
		EndPlayInEditor,
		/** Removed from the world without being destroyed (unused until streaming levels exist). */
		RemovedFromWorld,
		/** The world was torn down (UWorld::DestroyWorld, application exit). */
		Quit,
	};
} // namespace EEndPlayReason

/**
 * Whether a component may move while the game runs (UE: EComponentMobility). The values are the `.llev` on-disk ones:
 * UE's Stationary (lights that change colour but not position) does not exist in Leon.
 */
enum class EComponentMobility : uint8
{
	/** Never moves (static lighting, when it exists, bakes it). */
	Static = 0,
	/** May move at runtime. */
	Movable = 1,
};

/** What a primitive's collision takes part in (UE: ECollisionEnabled). */
namespace ECollisionEnabled
{
	enum Type : uint8
	{
		/** No body in the physics scene. */
		NoCollision,
		/** Traces and overlaps only. */
		QueryOnly,
		/** Simulation only. */
		PhysicsOnly,
		/** Traces, overlaps and simulation. */
		QueryAndPhysics,
	};
} // namespace ECollisionEnabled

/**
 * Converts a rotator to a quaternion and back through a one-entry cache (UE: FRotationConversionCache), so a rotation
 * set as a quaternion reads back bit for bit: USceneComponent keeps its relative rotation as an FRotator, and a
 * transform set through SetRelativeTransform / SetWorldTransform must not pick up the rotator round trip's rounding.
 * Unlike UE, the quaternion is cached as given (not normalized) and the rotator as set (not normalized), so a rotator
 * that never went through a quaternion converts exactly as FTransform(FRotator, ...) does.
 */
struct FRotationConversionCache
{
	/** The quaternion of InRotator: the cached one when InRotator is the cached rotator, else FQuat(InRotator). */
	[[nodiscard]] FQuat RotatorToQuat(const FRotator& InRotator) const
	{
		if (CachedRotator != InRotator)
		{
			CachedRotator = InRotator;
			CachedQuat = FQuat(InRotator);
		}
		return CachedQuat;
	}

	/** The rotator of InQuat, remembering both so RotatorToQuat returns InQuat itself for that rotator. */
	[[nodiscard]] FRotator QuatToRotator(const FQuat& InQuat) const
	{
		if (CachedQuat != InQuat)
		{
			CachedQuat = InQuat;
			CachedRotator = InQuat.Rotator();
		}
		return CachedRotator;
	}

private:
	mutable FQuat CachedQuat = FQuat::Identity;
	mutable FRotator CachedRotator = FRotator::ZeroRotator;
};

/** How a component's relative transform is set when it is attached (UE: EAttachmentRule). */
enum class EAttachmentRule : uint8
{
	/** Keeps the relative transform: the component moves with its new parent. */
	KeepRelative,
	/** Keeps the world transform: the relative transform is recomputed against the new parent. */
	KeepWorld,
	/** Snaps to the parent (or the socket): the relative transform becomes the identity. */
	SnapToTarget,
};

/** How a component's relative transform is set when it is detached (UE: EDetachmentRule). */
enum class EDetachmentRule : uint8
{
	KeepRelative,
	KeepWorld,
};

/** The rules AttachToComponent applies to location, rotation and scale (UE: FAttachmentTransformRules). */
struct ENGINE_API FAttachmentTransformRules
{
	static const FAttachmentTransformRules KeepRelativeTransform;
	static const FAttachmentTransformRules KeepWorldTransform;
	static const FAttachmentTransformRules SnapToTargetNotIncludingScale;
	static const FAttachmentTransformRules SnapToTargetIncludingScale;

	FAttachmentTransformRules(EAttachmentRule InRule, bool bInWeldSimulatedBodies)
		: LocationRule(InRule)
		, RotationRule(InRule)
		, ScaleRule(InRule)
		, bWeldSimulatedBodies(bInWeldSimulatedBodies)
	{
	}

	FAttachmentTransformRules(EAttachmentRule InLocationRule, EAttachmentRule InRotationRule,
		EAttachmentRule InScaleRule, bool bInWeldSimulatedBodies)
		: LocationRule(InLocationRule)
		, RotationRule(InRotationRule)
		, ScaleRule(InScaleRule)
		, bWeldSimulatedBodies(bInWeldSimulatedBodies)
	{
	}

	EAttachmentRule LocationRule;
	EAttachmentRule RotationRule;
	EAttachmentRule ScaleRule;
	/** Kept for the UE signature: Leon has no simulated bodies to weld. */
	bool bWeldSimulatedBodies;
};

/** The rules DetachFromComponent applies to location, rotation and scale (UE: FDetachmentTransformRules). */
struct ENGINE_API FDetachmentTransformRules
{
	static const FDetachmentTransformRules KeepRelativeTransform;
	static const FDetachmentTransformRules KeepWorldTransform;

	FDetachmentTransformRules(EDetachmentRule InRule, bool bInCallModify)
		: LocationRule(InRule)
		, RotationRule(InRule)
		, ScaleRule(InRule)
		, bCallModify(bInCallModify)
	{
	}

	EDetachmentRule LocationRule;
	EDetachmentRule RotationRule;
	EDetachmentRule ScaleRule;
	/** Kept for the UE signature: Leon has no transaction buffer. */
	bool bCallModify;
};
