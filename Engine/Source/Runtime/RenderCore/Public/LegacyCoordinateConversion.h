#pragma once

#include "CoreMinimal.h"

/**
 * Temporary: converts legacy data (.llev / .lmesh: Y up, right-handed, 1 unit = 1 metre, XYZ Euler degrees) to the
 * engine world and back. Only code that still holds legacy values may use it: the level and mesh readers and savers,
 * the legacy Euler fields of scene components and actors, and tests. It goes away with the legacy formats.
 *
 * For now the engine world is still the legacy one: the basis is the identity and one world unit is one legacy
 * metre. Later P7 commits change the basis to UE's (UE = 100 * (X, Z, Y)).
 */
struct RENDERCORE_API FLegacyCoordinateConversion
{
	/** Legacy position (metres) to a world location. */
	[[nodiscard]] static FVector ConvertPosition(const FVector& Legacy);

	/** Legacy direction (unitless) to a world direction. */
	[[nodiscard]] static FVector ConvertDirection(const FVector& Legacy);

	/** Legacy per-axis scale to a world Scale3D. */
	[[nodiscard]] static FVector ConvertScale(const FVector& Legacy);

	/** Legacy length (metres) to world units. */
	[[nodiscard]] static float ConvertLength(float Metres);

	[[nodiscard]] static FVector ToLegacyPosition(const FVector& World);
	[[nodiscard]] static FVector ToLegacyDirection(const FVector& World);
	[[nodiscard]] static FVector ToLegacyScale(const FVector& World);
	[[nodiscard]] static float ToLegacyLength(float WorldLength);

	/**
	 * Legacy XYZ Euler degrees to a world rotation. The legacy model matrix applied Rz first, then Ry, then Rx (glm
	 * T * Rx * Ry * Rz * S), so this is FQuat(X, a) * FQuat(Y, b) * FQuat(Z, c).
	 */
	[[nodiscard]] static FQuat ConvertEulerXYZ(const FVector& Degrees);

	/**
	 * World rotation back to legacy XYZ Euler degrees (inverse of ConvertEulerXYZ), each in (-180, 180]. Of the two
	 * Euler triples of a rotation it returns the one with the smaller X and Z, so a pure yaw comes back as (0, Yaw, 0).
	 */
	[[nodiscard]] static FVector ToLegacyEulerXYZ(const FQuat& Rotation);

	/**
	 * Legacy position, XYZ Euler degrees and scale to a world transform. A scale component closer to zero than 1e-4
	 * becomes +-1e-4, as the legacy model matrix did, so the normal matrix stays finite.
	 */
	[[nodiscard]] static FTransform ConvertTransform(
		const FVector& Position, const FVector& EulerXYZDegrees, const FVector& Scale);

	/**
	 * Legacy light pitch (about X) and yaw (about Y) in degrees to a world rotation: FQuat(Y, yaw) * FQuat(X, pitch).
	 * Legacy lights turn their forward axis by pitch, then yaw (not the mesh Euler order); the light travels along
	 * Rotation.RotateVector(LightForward()).
	 */
	[[nodiscard]] static FQuat ConvertLightRotation(float PitchDegrees, float YawDegrees);

	/** World rotation back to legacy light pitch and yaw in degrees (roll is dropped). */
	static void ToLegacyLightRotation(const FQuat& Rotation, float& OutPitchDegrees, float& OutYawDegrees);

	/** The axis a light with no rotation shines along: legacy +Z. */
	[[nodiscard]] static FVector LightForward();
};
