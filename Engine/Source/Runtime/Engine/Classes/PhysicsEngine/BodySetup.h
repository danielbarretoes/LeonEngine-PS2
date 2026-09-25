#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetupEnums.h"
#include "UObject/Object.h"
#include "BodySetup.generated.h"

/**
 * The collision description of a mesh (UE: UBodySetup): its simple shapes and which collision answers traces. A
 * UStaticMesh makes one when it is built and saves it as an inner object.
 *
 * What the physics scene (FPhysScene) does with a static mesh body, as before the asset classes:
 * - a static (not simulated) body of a mesh with triangles collides with those triangles (UE's complex as simple),
 *   unless CollisionTraceFlag is CTF_UseSimpleAsComplex;
 * - every other body is a box: the bounds of the AggGeom boxes, or the mesh's bounding box when there are none (the
 *   default: a built mesh has no boxes; UE would give such a body no simple collision).
 *
 * Leon has no cooked physics data (UE: the PhysX / Chaos meshes), no physical material and no per-body settings.
 */
UCLASS()
class ENGINE_API UBodySetup : public UObject
{
	GENERATED_BODY()

public:
	UBodySetup(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The simple collision shapes (UE: AggGeom). */
	UPROPERTY()
	FKAggregateGeom AggGeom;

	/** Which collision answers (UE: CollisionTraceFlag). */
	UPROPERTY()
	TEnumAsByte<ECollisionTraceFlag> CollisionTraceFlag = CTF_UseDefault;

	/** UE: GetCollisionTraceFlag. */
	[[nodiscard]] ECollisionTraceFlag GetCollisionTraceFlag() const
	{
		return CollisionTraceFlag;
	}

	/** True when a static body with triangles collides with them (the rules above). */
	[[nodiscard]] bool UsesComplexAsSimpleForStaticBodies() const
	{
		return CollisionTraceFlag != CTF_UseSimpleAsComplex;
	}
};
