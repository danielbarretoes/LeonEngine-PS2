#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

/** The glTF test fixtures' folder (MakeSkinnedFixture.py writes Cube.glb and SkinnedArm.glb there). */
inline FString GetGltfFixturePath(const TCHAR* FileName)
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures"), FileName));
}

/**
 * Cube.glb's mesh as the file stores it (right-handed, Y up, metres), vertex for vertex: 6 faces of 4 corners
 * counter-clockwise around their outward normal, two triangles each (MakeSkinnedFixture.py's CUBE_FACES).
 */
inline FMeshData MakeGltfTestCubeSourceSpace()
{
	struct FFace
	{
		FVector Normal;
		FVector Corners[4];
	};
	const FFace Faces[6] = {
		{FVector(0, 0, -1), {FVector(-1, -1, -1), FVector(-1, 1, -1), FVector(1, 1, -1), FVector(1, -1, -1)}},
		{FVector(0, 0, 1), {FVector(-1, -1, 1), FVector(1, -1, 1), FVector(1, 1, 1), FVector(-1, 1, 1)}},
		{FVector(-1, 0, 0), {FVector(-1, -1, -1), FVector(-1, -1, 1), FVector(-1, 1, 1), FVector(-1, 1, -1)}},
		{FVector(1, 0, 0), {FVector(1, -1, -1), FVector(1, 1, -1), FVector(1, 1, 1), FVector(1, -1, 1)}},
		{FVector(0, -1, 0), {FVector(-1, -1, -1), FVector(1, -1, -1), FVector(1, -1, 1), FVector(-1, -1, 1)}},
		{FVector(0, 1, 0), {FVector(-1, 1, -1), FVector(-1, 1, 1), FVector(1, 1, 1), FVector(1, 1, -1)}},
	};
	FMeshData Data;
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const uint32 Base = uint32(Face * 4);
		for (const FVector& Corner : Faces[Face].Corners)
		{
			Data.Vertices.Add(FVertex(Corner, Faces[Face].Normal, FVector2D::ZeroVector));
		}
		Data.Indices.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
	}
	return Data;
}

#endif // WITH_DEV_AUTOMATION_TESTS
