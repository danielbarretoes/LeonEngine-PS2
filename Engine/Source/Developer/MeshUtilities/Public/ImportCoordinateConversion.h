#pragma once

#include "CoreMinimal.h"

struct FMeshData;
struct FSkeletalMeshData;
struct FRawAnimSequence;

/** The axes of an imported source file. Both are right-handed; the engine world is left-handed. */
enum class EImportAxes : uint8
{
	/** Y up, front +Z (glTF 2.0, OBJ, FBX files that declare no axes). */
	RightHandedYUp,
	/**
	 * Z up, front -Y (ufbx_axes_right_handed_z_up): the frame UE's FBX importer converts a scene to (FbxAxisSystem Z
	 * up, -ParityOdd front, right-handed) before FFbxDataConverter flips Y.
	 */
	RightHandedZUp,
};

/**
 * Converts imported data to the engine world (UE: X forward, Y right, Z up, left-handed, 1 unit = 1 cm). It is the
 * last step of every importer, after normals and winding are final (UE: FFbxDataConverter, the glTF importer's
 * ConvertVec3 / ConvertQuat).
 *
 * Each basis swaps or flips one axis and scales by UnitsToCm:
 * - RightHandedYUp: (X, Z, Y), as UE's glTF importer ({X, Z, Y}, quaternions (-X, -Z, -Y, W)) and ufbx's
 *   left_handed_z_up. With UnitsToCm = 100 it is FLegacyCoordinateConversion, bit for bit.
 * - RightHandedZUp: (X, -Y, Z), as UE's FFbxDataConverter (ConvertPos, ConvertRotToQuat). A right-handed Y-up point
 *   (x, y, z) is (x, -z, y) in this frame, so both bases put it at (x, z, y): an FBX file ends up where the same scene
 *   exported to glTF does.
 *
 * Both bases have determinant -1: they keep the physical scene, and triangles keep their index order and their
 * winding on screen. What flips is everything built with a handedness: a triangle's cross(E1, E2) now points against
 * its stored normal where it pointed along it before, the tangent's bitangent sign, and the sense of rotations.
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

	/** Source tangent (xyz, w = bitangent sign) to a world tangent: the direction, and w flips with the basis. */
	[[nodiscard]] FVector4 ConvertTangent(const FVector4& Source) const;

	/** Source per-axis scale to a world Scale3D: the axes are reordered, never negated. */
	[[nodiscard]] FVector ConvertScale(const FVector& Source) const;

	/** Source rotation to the world rotation that does the same to converted vectors (the axis turns the other way). */
	[[nodiscard]] FQuat ConvertRotation(const FQuat& Source) const;

	/**
	 * Source affine matrix (FMatrix row vectors: row 3 is the translation) to the world matrix that does the same to
	 * converted points: B^-1 * M * B in FMatrix product order, where a source row vector times B is the world one.
	 * Written out entry by entry, so rotations stay exact and only the translation is scaled.
	 */
	[[nodiscard]] FMatrix ConvertMatrix(const FMatrix& Source) const;

	/** B: a source row vector times it is the converted world position. */
	[[nodiscard]] FMatrix GetBasisMatrix() const;

	/** Positions, normals and tangents of a static mesh in place; UVs and the index order are kept. */
	void ConvertMeshData(FMeshData& Data) const;

	/**
	 * A skinned mesh in place: vertices as ConvertMeshData, the bounds, and the inverse bind pose and embedded clip by
	 * conjugation, so InverseBind * BoneWorld skins the converted vertices to the converted positions.
	 */
	void ConvertSkeletalMeshData(FSkeletalMeshData& Data) const;

	/** Every sampled bone matrix of a clip in place (conjugation, as ConvertMatrix). */
	void ConvertAnimSequence(FRawAnimSequence& Sequence) const;

private:
	[[nodiscard]] float SignedAxis(const FVector& Source, int32 WorldAxis) const;

	EImportAxes SourceAxes;
	float UnitsToCm;
	/** World axis I is source axis SourceAxisOf[I], negated when bNegateAxis[I]. */
	int32 SourceAxisOf[3];
	bool bNegateAxis[3];
	/** The basis has determinant -1 (both do): bitangent signs flip and rotations turn the other way. */
	bool bMirror;
};
