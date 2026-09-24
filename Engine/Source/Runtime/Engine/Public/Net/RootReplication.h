#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Net/NetProtocol.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>

namespace Leon::Net
{

	/// Capture Actor root location + yaw for replication (no full USceneComponent graph).
	[[nodiscard]] inline FPawnSnap CaptureActorRoot(std::uint8_t Slot, const AActor& Actor, float VelocityY = 0.0f,
		float AnimBlend = 0.0f, float BoomYaw = 0.0f, float BoomPitch = 0.0f, bool bGrounded = true)
	{
		FPawnSnap Snap{};
		Snap.Slot = Slot;
		const glm::vec3& Loc = Actor.GetActorLocation();
		Snap.X = Loc.x;
		Snap.Y = Loc.y;
		Snap.Z = Loc.z;
		Snap.Yaw = Actor.GetActorYaw();
		Snap.VelY = VelocityY;
		Snap.AnimBlend = AnimBlend;
		Snap.BoomYaw = BoomYaw;
		Snap.BoomPitch = BoomPitch;
		Snap.Grounded = bGrounded ? 1 : 0;
		Snap.Health = 100.0f;
		Snap.Flags = PawnSnapAlive;
		return Snap;
	}

	/// Capture Character movement-relevant fields for snapshots.
	[[nodiscard]] inline FPawnSnap CaptureCharacterRoot(
		std::uint8_t Slot, const ACharacter& Character, float BoomYaw = 0.0f, float BoomPitch = 0.0f)
	{
		FPawnSnap Snap = CaptureActorRoot(Slot, Character, Character.GetVelocityZ(), Character.GetAnimBlendInput(),
			BoomYaw, BoomPitch, Character.IsMovingOnGround());
		return Snap;
	}

	/// Apply a replicated root snapshot onto an Actor (location + yaw only).
	inline void ApplyActorRoot(AActor& Actor, const FPawnSnap& Snap)
	{
		Actor.SetActorLocationAndRotation({Snap.X, Snap.Y, Snap.Z}, Snap.Yaw);
	}

	/// Distance relevancy check (Unreal Net relevancy lite). `maxDist <= 0` always relevant.
	[[nodiscard]] inline bool IsPawnRelevant(const glm::vec3& Viewer, const glm::vec3& Pawn, float MaxDist)
	{
		if (MaxDist <= 0.0f)
		{
			return true;
		}
		const glm::vec3 D = Pawn - Viewer;
		const float DistSq = D.x * D.x + D.y * D.y + D.z * D.z;
		return DistSq <= (MaxDist * MaxDist);
	}

	[[nodiscard]] inline bool IsPawnRelevantXZ(const glm::vec3& Viewer, const glm::vec3& Pawn, float MaxDist)
	{
		if (MaxDist <= 0.0f)
		{
			return true;
		}
		const float Dx = Pawn.x - Viewer.x;
		const float Dz = Pawn.z - Viewer.z;
		return (Dx * Dx + Dz * Dz) <= (MaxDist * MaxDist);
	}

	/// Default XZ cull radius for AI / remote pawns in snapshots (meters).
	constexpr float DefaultAiRelevancyXZ = 64.0f;

	/// True if `pawn` is within `maxDist` XZ of any viewer in [begin, end).
	template <typename Iterator>
	[[nodiscard]] bool IsPawnRelevantToAnyXZ(
		Iterator Begin, Iterator End, const glm::vec3& Pawn, float MaxDist = DefaultAiRelevancyXZ)
	{
		if (MaxDist <= 0.0f)
		{
			return true;
		}
		for (Iterator It = Begin; It != End; ++It)
		{
			if (IsPawnRelevantXZ(*It, Pawn, MaxDist))
			{
				return true;
			}
		}
		return false;
	}

} // namespace Leon::Net
