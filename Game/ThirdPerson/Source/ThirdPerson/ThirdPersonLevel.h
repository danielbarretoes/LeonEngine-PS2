#pragma once

#include "CoreTypes.h"

struct FPS2Material;

/** Box primitive placed in the level (TP_ThirdPerson: the map's static mesh actors). */
struct FThirdPersonPrimitive
{
	float LocationX = 0.0f;
	float LocationY = 0.0f;
	float LocationZ = 0.0f;
	uint32 Yaw256 = 0;
	uint32 Pitch256 = 0;
	float ScaleX = 1.0f; // half extents
	float ScaleY = 1.0f;
	float ScaleZ = 1.0f;
	const FPS2Material* Material = nullptr;
};

/**
 * Large grounded primitive level: a tiled ground plane plus crates, platforms, stairs and walls.
 * Also answers the queries the character and the camera boom need (UE: world traces/sweeps).
 */
class FThirdPersonLevel
{
public:
	static constexpr float ArenaHalfExtent = 70.0f;
	static constexpr float GroundTopY = 0.0f;
	// Tile the ground so no single quad straddles the camera (avoids stretch / total cull).
	static constexpr int32 GroundTilesPerSide = 18;
	static constexpr int32 GroundTileCount = GroundTilesPerSide * GroundTilesPerSide;
	static constexpr int32 PropCount = 17;
	static constexpr int32 ActorCount = GroundTileCount + PropCount;

	/** FindSupportY result when nothing is under the footprint. */
	static constexpr float NoSupport = -10000.0f;

	void Build(const FPS2Material* GroundMaterial, const FPS2Material* PlatformMaterial, const FPS2Material* CrateMaterial);

	/** Highest top under a square footprint the feet can step onto or catch while falling. */
	float FindSupportY(float X, float Z, float HalfWidth, float FeetY) const;

	/** Pushes a square footprint out of the props it cannot step onto. */
	void ResolveWallCollisions(float& X, float& Z, float HalfWidth, float FeetY, float HeadY) const;

	/**
	 * Boom length from the look-at point towards the desired camera location that keeps a sphere of
	 * ProbeRadius clear of the props (UE: SpringArm probe sweep).
	 */
	float ProbeBoomLength(float LookX, float LookY, float LookZ, float CameraX, float CameraY, float CameraZ,
		float DesiredLength, float MinLength, float ProbeRadius) const;

	void Draw() const;

private:
	const FThirdPersonPrimitive* GetProps() const
	{
		return Actors + GroundTileCount;
	}

	FThirdPersonPrimitive Actors[ActorCount];
};
