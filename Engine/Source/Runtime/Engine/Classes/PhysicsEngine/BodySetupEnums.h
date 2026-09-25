#pragma once

#include "CoreMinimal.h"
#include "BodySetupEnums.generated.h"

/**
 * Which collision a body answers traces and physics with (UE: ECollisionTraceFlag, BodySetupEnums.h). Leon's physics
 * scene has no convex hulls: "simple" is the body setup's boxes (or the mesh's bounding box when it has none) and
 * "complex" is the mesh's triangles.
 */
UENUM()
enum ECollisionTraceFlag
{
	/**
	 * The project default (UE): Leon uses the triangles of a static (non-simulated) body with triangles, and the
	 * simple shape for everything else.
	 */
	CTF_UseDefault,
	/** Simple collision for physics, complex for traces (Leon: as CTF_UseDefault). */
	CTF_UseSimpleAndComplex,
	/** The simple shape for everything, even a static body with triangles. */
	CTF_UseSimpleAsComplex,
	/** The triangles for a static body (Leon: as CTF_UseDefault; a simulated body still uses the simple shape). */
	CTF_UseComplexAsSimple,
};
