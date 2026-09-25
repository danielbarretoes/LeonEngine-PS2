#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FMeshData;

/**
 * Test only: converts legacy data (the pre-P7 world: Y up, right-handed, 1 unit = 1 metre, XYZ Euler degrees) to the
 * engine world (UE: X forward, Y right, Z up, left-handed, 1 unit = 1 cm) and back. The golden tests recorded their
 * tables in the legacy world and compare through it (Engine's Tests/LegacyGolden.h), and the mesh tests compare with
 * the legacy procedural meshes. No runtime module has it since the legacy levels went (P15): it is compiled with the
 * automation tests only, and CheckBannedApis.ps1 (gate G4) allows it in test folders only.
 *
 * The basis is UE = UnitsPerMetre * (X, Z, Y): legacy Y and Z swap, as UE 4.27's glTF importer converts ({X, Z, Y})
 * and ufbx's left_handed_z_up. The swap has determinant -1, so it keeps the physical scene: what was on the right is
 * still on the right, triangles keep their index order and their winding on screen. What flips is everything built
 * with a handedness: world cross products (the right vector is Up ^ Forward), the tangent's bitangent sign, and the
 * sense of rotations (a legacy rotation by A about an axis is a rotation by -A about the swapped axis).
 *
 * Legacy horizontal (X, Z) becomes UE (X, Y) in the same order, so a legacy yaw measured from +X toward +Z (the
 * cameras) keeps its value as a UE yaw; the actor yaw (0 = legacy +Z, toward +X) becomes 90 - yaw.
 */
struct RENDERCORE_API FLegacyCoordinateConversion
{
	/** World units in one legacy metre (UE: AWorldSettings::WorldToMeters). */
	static constexpr float UnitsPerMetre = 100.0f;

	/** Legacy position (metres) to a world location: (X, Z, Y) * UnitsPerMetre. */
	[[nodiscard]] static FVector ConvertPosition(const FVector& Legacy);

	/** Legacy direction or normal (unitless) to a world direction: (X, Z, Y). */
	[[nodiscard]] static FVector ConvertDirection(const FVector& Legacy);

	/** Legacy tangent (xyz, w = bitangent sign) to a world tangent: (X, Z, Y, -W), the bitangent flips with the basis.
	 */
	[[nodiscard]] static FVector4 ConvertTangent(const FVector4& Legacy);

	/** Legacy per-axis scale to a world Scale3D: (X, Z, Y). */
	[[nodiscard]] static FVector ConvertScale(const FVector& Legacy);

	/** Legacy length (metres) to world units. Speeds (m/s) convert the same way. */
	[[nodiscard]] static float ConvertLength(float Metres);

	/** Legacy box half extents (metres) to world units: (X, Z, Y) * UnitsPerMetre. */
	[[nodiscard]] static FVector ConvertExtent(const FVector& Legacy);

	/** Legacy rotation to the world rotation that does the same to converted vectors: (-X, -Z, -Y, W). */
	[[nodiscard]] static FQuat ConvertRotation(const FQuat& Legacy);

	[[nodiscard]] static FVector ToLegacyPosition(const FVector& World);
	[[nodiscard]] static FVector ToLegacyDirection(const FVector& World);
	[[nodiscard]] static FVector4 ToLegacyTangent(const FVector4& World);
	[[nodiscard]] static FVector ToLegacyScale(const FVector& World);
	[[nodiscard]] static float ToLegacyLength(float WorldLength);
	[[nodiscard]] static FVector ToLegacyExtent(const FVector& World);
	[[nodiscard]] static FQuat ToLegacyRotation(const FQuat& World);

	/**
	 * Legacy XYZ Euler degrees to a world rotation. The legacy model matrix applied Rz first, then Ry, then Rx (glm
	 * T * Rx * Ry * Rz * S), so this is ConvertRotation(FQuat(X, a) * FQuat(Y, b) * FQuat(Z, c)) with legacy axes.
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
	 * Legacy XYZ Euler degrees of an actor-like record (player start, AI spawn point) to a world rotation whose forward
	 * axis (+X) is the legacy forward (+Z): ConvertEulerXYZ, then a local yaw of 90 degrees. A pure legacy yaw Y gives
	 * the world yaw 90 - Y (ConvertActorYaw).
	 */
	[[nodiscard]] static FQuat ConvertActorEulerXYZ(const FVector& Degrees);

	/** World actor rotation back to legacy XYZ Euler degrees (inverse of ConvertActorEulerXYZ). */
	[[nodiscard]] static FVector ToLegacyActorEulerXYZ(const FQuat& Rotation);

	/** Legacy actor yaw (0 = legacy +Z, positive toward +X) to a world yaw (0 = +X, positive toward +Y): 90 - Yaw. */
	[[nodiscard]] static float ConvertActorYaw(float LegacyYawDegrees);

	/** World actor yaw back to the legacy actor yaw, in (-180, 180]. */
	[[nodiscard]] static float ToLegacyActorYaw(float WorldYawDegrees);

	/** Legacy yaw rate (degrees per second about legacy Y) to a world yaw rate: rotations turn the other way. */
	[[nodiscard]] static float ConvertYawRate(float LegacyDegreesPerSecond);

	/** World yaw rate back to a legacy yaw rate. */
	[[nodiscard]] static float ToLegacyYawRate(float WorldDegreesPerSecond);

	/**
	 * Legacy orbit camera angles (the eye sits at Target + Distance * (cos P cos Y, sin P, cos P sin Y)) to the world
	 * view rotation FRotator(-P, Y + 180, 0): the eye is Target - Rotation.Vector() * Distance.
	 */
	[[nodiscard]] static FRotator ConvertOrbitViewRotation(float LegacyYawDegrees, float LegacyPitchDegrees);

	/** World orbit view rotation back to legacy orbit yaw and pitch, the yaw in (-180, 180]. */
	static void ToLegacyOrbitRotation(const FRotator& ViewRotation, float& OutYawDegrees, float& OutPitchDegrees);

	/**
	 * Legacy free-look camera angles (forward (cos P cos Y, sin P, cos P sin Y)) to the world view rotation
	 * FRotator(P, Y, 0).
	 */
	[[nodiscard]] static FRotator ConvertFreeLookRotation(float LegacyYawDegrees, float LegacyPitchDegrees);

	/** World free-look view rotation back to legacy yaw and pitch, the yaw in (-180, 180]. */
	static void ToLegacyFreeLookRotation(const FRotator& ViewRotation, float& OutYawDegrees, float& OutPitchDegrees);

	/**
	 * Legacy light pitch (about X) and yaw (about Y) in degrees to a world rotation. The legacy light shone along
	 * (sin Y cos P, -sin P, cos Y cos P); the world light shines along its forward axis, FRotator(-P, 90 - Y, 0).
	 */
	[[nodiscard]] static FQuat ConvertLightRotation(float PitchDegrees, float YawDegrees);

	/** World light rotation back to legacy light pitch and yaw in degrees (from its forward axis; roll is dropped). */
	static void ToLegacyLightRotation(const FQuat& Rotation, float& OutPitchDegrees, float& OutYawDegrees);

	/**
	 * Legacy mesh data (Y up, metres: the old version 1 cooked meshes, which tests compare against) to the world in
	 * place: positions, normals and tangents. The index order is kept: the physical triangles, and their winding on
	 * screen, do not change.
	 */
	static void ConvertMeshData(FMeshData& Data);
};

#endif // WITH_DEV_AUTOMATION_TESTS
