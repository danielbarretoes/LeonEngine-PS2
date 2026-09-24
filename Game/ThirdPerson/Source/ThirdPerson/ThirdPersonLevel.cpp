#include "ThirdPersonLevel.h"

#include "HAL/PlatformMath.h"
#include "PS2RHI.h"

namespace
{
	constexpr float MaxStepUp = 0.9f;
	constexpr float MaxFallCatch = 2.5f;

	/** Box with its bottom flush on Y = BottomY (no floating). */
	void PlaceGrounded(FThirdPersonPrimitive& Out, float X, float Z, float HalfX, float HalfY, float HalfZ,
		float BottomY, uint32 Yaw256, const FPS2Material* Material)
	{
		Out.LocationX = X;
		Out.LocationZ = Z;
		Out.ScaleX = HalfX;
		Out.ScaleY = HalfY;
		Out.ScaleZ = HalfZ;
		Out.LocationY = BottomY + HalfY;
		Out.Yaw256 = Yaw256;
		Out.Pitch256 = 0;
		Out.Material = Material;
	}

	/** World -> box-local XZ (math3d matrix_rotate Y: world = (x*c + z*s, -x*s + z*c)). */
	void ToLocalXZ(const FThirdPersonPrimitive& Actor, float X, float Z, float& OutLocalX, float& OutLocalZ)
	{
		const float Cos = FPlatformMath::Cos256(Actor.Yaw256);
		const float Sin = FPlatformMath::Sin256(Actor.Yaw256);
		const float DeltaX = X - Actor.LocationX;
		const float DeltaZ = Z - Actor.LocationZ;
		OutLocalX = DeltaX * Cos - DeltaZ * Sin;
		OutLocalZ = DeltaX * Sin + DeltaZ * Cos;
	}

	/** World-axis half extents of a yawed box footprint (conservative AABB). */
	void FootprintExtents(const FThirdPersonPrimitive& Actor, float& OutExtentX, float& OutExtentZ)
	{
		float Cos = FPlatformMath::Cos256(Actor.Yaw256);
		float Sin = FPlatformMath::Sin256(Actor.Yaw256);
		Cos = Cos < 0.0f ? -Cos : Cos;
		Sin = Sin < 0.0f ? -Sin : Sin;
		OutExtentX = Actor.ScaleX * Cos + Actor.ScaleZ * Sin;
		OutExtentZ = Actor.ScaleX * Sin + Actor.ScaleZ * Cos;
	}

	bool FootprintOverlaps(const FThirdPersonPrimitive& Actor, float X, float Z, float HalfWidth)
	{
		float LocalX = 0.0f;
		float LocalZ = 0.0f;
		ToLocalXZ(Actor, X, Z, LocalX, LocalZ);
		LocalX = LocalX < 0.0f ? -LocalX : LocalX;
		LocalZ = LocalZ < 0.0f ? -LocalZ : LocalZ;
		return LocalX < Actor.ScaleX + HalfWidth && LocalZ < Actor.ScaleZ + HalfWidth;
	}

	/** Ray (origin + t * dir, t in [0, 1], dir = full boom vector) vs AABB. */
	bool RayAabbHit(
		const float Origin[3], const float Direction[3], const float BoxMin[3], const float BoxMax[3], float& OutTime)
	{
		float TimeMin = 0.0f;
		float TimeMax = 1.0f;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Direction[Axis] > -0.00001f && Direction[Axis] < 0.00001f)
			{
				if (Origin[Axis] < BoxMin[Axis] || Origin[Axis] > BoxMax[Axis])
				{
					return false;
				}
				continue;
			}
			const float InvDirection = 1.0f / Direction[Axis];
			float Time0 = (BoxMin[Axis] - Origin[Axis]) * InvDirection;
			float Time1 = (BoxMax[Axis] - Origin[Axis]) * InvDirection;
			if (Time0 > Time1)
			{
				const float Swap = Time0;
				Time0 = Time1;
				Time1 = Swap;
			}
			if (Time0 > TimeMin)
			{
				TimeMin = Time0;
			}
			if (Time1 < TimeMax)
			{
				TimeMax = Time1;
			}
			if (TimeMin > TimeMax)
			{
				return false;
			}
		}
		OutTime = TimeMin;
		return TimeMin >= 0.0f && TimeMin <= 1.0f;
	}
} // namespace

void FThirdPersonLevel::Build(
	const FPS2Material* GroundMaterial, const FPS2Material* PlatformMaterial, const FPS2Material* CrateMaterial)
{
	const float TileHalf = ArenaHalfExtent / static_cast<float>(GroundTilesPerSide);
	int32 Index = 0;
	for (int32 TileZ = 0; TileZ < GroundTilesPerSide; ++TileZ)
	{
		for (int32 TileX = 0; TileX < GroundTilesPerSide; ++TileX)
		{
			FThirdPersonPrimitive& Tile = Actors[Index++];
			Tile = {};
			Tile.LocationX = -ArenaHalfExtent + TileHalf + static_cast<float>(TileX) * (TileHalf * 2.0f);
			Tile.LocationY = -0.5f;
			Tile.LocationZ = -ArenaHalfExtent + TileHalf + static_cast<float>(TileZ) * (TileHalf * 2.0f);
			Tile.ScaleX = TileHalf;
			Tile.ScaleY = 0.5f;
			Tile.ScaleZ = TileHalf;
			Tile.Material = GroundMaterial;
		}
	}

	// Grounded crates / blocks (bottom on Y = 0).
	PlaceGrounded(Actors[Index++], 14.0f, -10.0f, 2.0f, 2.0f, 2.0f, GroundTopY, 20, CrateMaterial);
	PlaceGrounded(Actors[Index++], -16.0f, 8.0f, 2.5f, 2.5f, 2.5f, GroundTopY, 40, CrateMaterial);
	PlaceGrounded(Actors[Index++], 8.0f, 18.0f, 2.0f, 2.0f, 2.0f, GroundTopY, 10, CrateMaterial);
	PlaceGrounded(Actors[Index++], -22.0f, -14.0f, 3.0f, 1.5f, 3.0f, GroundTopY, 0, CrateMaterial);
	PlaceGrounded(Actors[Index++], 28.0f, 6.0f, 2.2f, 2.2f, 2.2f, GroundTopY, 55, CrateMaterial);
	PlaceGrounded(Actors[Index++], -8.0f, 28.0f, 2.0f, 2.0f, 2.0f, GroundTopY, 30, CrateMaterial);
	PlaceGrounded(Actors[Index++], 20.0f, -28.0f, 2.5f, 1.8f, 2.5f, GroundTopY, 15, CrateMaterial);

	PlaceGrounded(Actors[Index++], -32.0f, -24.0f, 6.0f, 5.0f, 6.0f, GroundTopY, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], 36.0f, 22.0f, 5.0f, 4.0f, 5.0f, GroundTopY, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], -40.0f, 30.0f, 4.0f, 3.5f, 4.0f, GroundTopY, 12, PlatformMaterial);

	// Stairs.
	PlaceGrounded(Actors[Index++], 0.0f, -40.0f, 6.0f, 1.0f, 4.0f, GroundTopY, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], 0.0f, -40.0f, 4.5f, 1.0f, 3.0f, GroundTopY + 2.0f, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], 0.0f, -40.0f, 3.0f, 1.0f, 2.0f, GroundTopY + 4.0f, 0, PlatformMaterial);

	// Walls.
	PlaceGrounded(Actors[Index++], 45.0f, 0.0f, 1.5f, 3.0f, 12.0f, GroundTopY, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], -45.0f, -8.0f, 1.5f, 3.0f, 10.0f, GroundTopY, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], 10.0f, 45.0f, 14.0f, 2.5f, 1.5f, GroundTopY, 0, PlatformMaterial);
	PlaceGrounded(Actors[Index++], -12.0f, -48.0f, 10.0f, 2.5f, 1.5f, GroundTopY, 0, PlatformMaterial);
}

float FThirdPersonLevel::FindSupportY(float X, float Z, float HalfWidth, float FeetY) const
{
	float Best = NoSupport;
	for (int32 Index = 0; Index < ActorCount; ++Index)
	{
		const FThirdPersonPrimitive& Actor = Actors[Index];
		if (!FootprintOverlaps(Actor, X, Z, HalfWidth))
		{
			continue;
		}
		const float Top = Actor.LocationY + Actor.ScaleY;
		if (Top > FeetY + MaxStepUp || Top < FeetY - MaxFallCatch)
		{
			continue;
		}
		if (Top > Best)
		{
			Best = Top;
		}
	}
	return Best;
}

void FThirdPersonLevel::ResolveWallCollisions(float& X, float& Z, float HalfWidth, float FeetY, float HeadY) const
{
	const FThirdPersonPrimitive* Props = GetProps();
	for (int32 Index = 0; Index < PropCount; ++Index)
	{
		const FThirdPersonPrimitive& Actor = Props[Index];
		const float Top = Actor.LocationY + Actor.ScaleY;
		const float Bottom = Actor.LocationY - Actor.ScaleY;
		if (Top <= FeetY + MaxStepUp || Bottom >= HeadY)
		{
			continue;
		}
		float LocalX = 0.0f;
		float LocalZ = 0.0f;
		ToLocalXZ(Actor, X, Z, LocalX, LocalZ);
		const float PenetrationX = Actor.ScaleX + HalfWidth - (LocalX < 0.0f ? -LocalX : LocalX);
		const float PenetrationZ = Actor.ScaleZ + HalfWidth - (LocalZ < 0.0f ? -LocalZ : LocalZ);
		if (PenetrationX <= 0.0f || PenetrationZ <= 0.0f)
		{
			continue;
		}
		// Minimum-penetration axis in box space, then rotate the push back to world.
		float PushX = 0.0f;
		float PushZ = 0.0f;
		if (PenetrationX < PenetrationZ)
		{
			PushX = LocalX < 0.0f ? -PenetrationX : PenetrationX;
		}
		else
		{
			PushZ = LocalZ < 0.0f ? -PenetrationZ : PenetrationZ;
		}
		const float Cos = FPlatformMath::Cos256(Actor.Yaw256);
		const float Sin = FPlatformMath::Sin256(Actor.Yaw256);
		X += PushX * Cos + PushZ * Sin;
		Z += -PushX * Sin + PushZ * Cos;
	}
}

float FThirdPersonLevel::ProbeBoomLength(float LookX, float LookY, float LookZ, float CameraX, float CameraY,
	float CameraZ, float DesiredLength, float MinLength, float ProbeRadius) const
{
	const float Origin[3] = {LookX, LookY, LookZ};
	const float Direction[3] = {CameraX - LookX, CameraY - LookY, CameraZ - LookZ};
	float BestTime = 1.0f;
	const FThirdPersonPrimitive* Props = GetProps();
	for (int32 Index = 0; Index < PropCount; ++Index)
	{
		const FThirdPersonPrimitive& Actor = Props[Index];
		float ExtentX = 0.0f;
		float ExtentZ = 0.0f;
		FootprintExtents(Actor, ExtentX, ExtentZ);
		const float BoxMin[3] = {Actor.LocationX - ExtentX - ProbeRadius, Actor.LocationY - Actor.ScaleY - ProbeRadius,
			Actor.LocationZ - ExtentZ - ProbeRadius};
		const float BoxMax[3] = {Actor.LocationX + ExtentX + ProbeRadius, Actor.LocationY + Actor.ScaleY + ProbeRadius,
			Actor.LocationZ + ExtentZ + ProbeRadius};
		float Time = 0.0f;
		if (RayAabbHit(Origin, Direction, BoxMin, BoxMax, Time) && Time < BestTime)
		{
			BestTime = Time;
		}
	}
	// Pull in slightly before the hit so the near plane stays clear of the surface.
	float Length = DesiredLength * BestTime - ProbeRadius * 0.35f;
	if (Length < MinLength)
	{
		Length = MinLength;
	}
	if (Length > DesiredLength)
	{
		Length = DesiredLength;
	}
	return Length;
}

void FThirdPersonLevel::Draw() const
{
	for (int32 Index = 0; Index < ActorCount; ++Index)
	{
		const FThirdPersonPrimitive& Actor = Actors[Index];
		FPS2RHI::BindMaterial(*Actor.Material);
		(void)FPS2RHI::DrawBox(Actor.LocationX, Actor.LocationY, Actor.LocationZ, Actor.Yaw256, Actor.Pitch256,
			Actor.ScaleX, Actor.ScaleY, Actor.ScaleZ);
	}
}
