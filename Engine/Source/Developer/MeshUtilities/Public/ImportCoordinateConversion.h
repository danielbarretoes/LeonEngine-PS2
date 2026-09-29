#pragma once

#include "CoreMinimal.h"

struct FMeshData;

/** The axes of an imported source file (right-handed; the engine world is left-handed). */
enum class EImportAxes : uint8
{
	/** Y up, front +Z: glTF 2.0, the only mesh and animation format (Docs/PLANS/ps2-shipping.md D11). */
	RightHandedYUp,
};

/**
 * Converts imported data to the engine world (UE: X forward, Y right, Z up, left-handed, 1 unit = 1 cm). It is the
 * last step of every importer, after normals and winding are final (UE: the glTF importer's ConvertVec3 /
 * ConvertQuat).
 *
 * The basis swaps two axes and scales by UnitsToCm: RightHandedYUp is (X, Z, Y), as UE's glTF importer ({X, Z, Y},
 * quaternions (-X, -Z, -Y, W)). With UnitsToCm = 100 it is the pre-P7 legacy conversion the golden tests keep, bit for
 * bit.
 *
 * The basis has determinant -1: it keeps the physical scene, and triangles keep their index order and their winding
 * on screen. What flips is everything built with a handedness: a triangle's cross(E1, E2) now points against its
 * stored normal where it pointed along it before, and the sense of rotations.
 */
class MESHUTILITIES_API FImportCoordinateConversion
{
public:
	/** Centimetres in a metre: UnitsToCm of a source in metres (OBJ, glTF). */
	static constexpr float CmPerMetre = 100.0f;

	FImportCoordinateConversion(EImportAxes InSourceAxes, float InUnitsToCm);

	[[nodiscard]] EImportAxes GetSourceAxes() const
	{
		return SourceAxes;
	}

	/** Centimetres in one source unit (metres: 100). */
	[[nodiscard]] float GetUnitsToCm() const
	{
		return UnitsToCm;
	}

	/** The source's up axis: a fallback normal that converts to the world up. */
	[[nodiscard]] FVector GetSourceUp() const;

	/** Source position to a world location: the axis swap or flip, times UnitsToCm. */
	[[nodiscard]] FVector ConvertPosition(const FVector& Source) const;

	/** Source direction or normal (unitless) to a world direction. */
	[[nodiscard]] FVector ConvertDirection(const FVector& Source) const;

	/** Source per-axis scale to a world Scale3D: the axes are reordered, never negated. */
	[[nodiscard]] FVector ConvertScale(const FVector& Source) const;

	/** Source rotation to the world rotation that does the same to converted vectors (the axis turns the other way). */
	[[nodiscard]] FQuat ConvertRotation(const FQuat& Source) const;

	/**
	 * Source transform (a bone's local transform, a node's) to the world transform that does the same to converted
	 * points: the translation as a position, the rotation and the scale as above. The same as converting its matrix
	 * (ConvertMatrix), component by component, so transforms keep composing as they did.
	 */
	[[nodiscard]] FTransform ConvertTransform(const FTransform& Source) const;

	/**
	 * Source affine matrix (FMatrix row vectors: row 3 is the translation) to the world matrix that does the same to
	 * converted points: B^-1 * M * B in FMatrix product order, where a source row vector times B is the world one.
	 * Written out entry by entry, so rotations stay exact and only the translation is scaled.
	 */
	[[nodiscard]] FMatrix ConvertMatrix(const FMatrix& Source) const;

	/** B: a source row vector times it is the converted world position. */
	[[nodiscard]] FMatrix GetBasisMatrix() const;

	/** Positions and normals of a mesh in place; UVs and the index order are kept. */
	void ConvertMeshData(FMeshData& Data) const;

private:
	[[nodiscard]] float SignedAxis(const FVector& Source, int32 WorldAxis) const;

	EImportAxes SourceAxes;
	float UnitsToCm;
	/** World axis I is source axis SourceAxisOf[I], negated when bNegateAxis[I]. */
	int32 SourceAxisOf[3];
	bool bNegateAxis[3];
	/** The basis has determinant -1: rotations turn the other way. */
	bool bMirror;
};
