#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace1D;
class USkeletalMeshComponent;
class USkeleton;

/**
 * UE-like UAnimInstance base: UBlendSpace1D locomotion only (no jump state machine).
 * Game / character subclasses add game-specific graphs through NativeInitializeAnimation.
 *
 * It references its skeleton, blend space and the clips it samples through UPROPERTYs, so the garbage collector keeps
 * the assets it plays alive.
 */
UCLASS(Transient)
class ENGINE_API UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	UAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetOwningMeshComponent(USkeletalMeshComponent* Owner)
	{
		OwningMesh = Owner;
	}
	[[nodiscard]] USkeletalMeshComponent* GetOwningMeshComponent() const
	{
		return OwningMesh;
	}

	void SetSkeleton(const USkeleton* InSkeleton)
	{
		Skeleton = InSkeleton;
	}
	void SetBlendSpace(const UBlendSpace1D* InBlendSpace)
	{
		BlendSpace = InBlendSpace;
	}

	void SetBlendSpaceInput(float AxisValue);
	[[nodiscard]] float GetBlendSpaceInput() const
	{
		return BlendInput;
	}
	[[nodiscard]] float GetBlendSpaceInputTarget() const
	{
		return BlendInputTarget;
	}

	void SetLocomotionBlendInterpSpeed(float Speed)
	{
		LocomotionBlendInterpSpeed = Speed >= 0.0f ? Speed : 0.0f;
	}
	[[nodiscard]] float GetLocomotionBlendInterpSpeed() const
	{
		return LocomotionBlendInterpSpeed;
	}

	virtual void NativeInitializeAnimation()
	{
	}
	virtual void NativeUpdateAnimation(float DeltaTime);
	/** Current pose bone model-space matrices (node_to_world). */
	virtual void GetBoneWorldMatrices(TArray<FMatrix>& OutBoneWorld) const;
	virtual void GetSkinMatrices(TArray<FMatrix>& OutSkin) const;

	[[nodiscard]] float GetBlendAlpha() const
	{
		return BlendAlpha;
	}

protected:
	void UpdateLocomotion(float DeltaTime);
	void SampleLocomotionBoneWorld(TArray<FMatrix>& OutBoneWorld) const;
	void SkinFromBoneWorld(const TArray<FMatrix>& BoneWorld, TArray<FMatrix>& OutSkin) const;

	[[nodiscard]] const USkeleton* GetSkeleton() const
	{
		return Skeleton;
	}
	[[nodiscard]] const UBlendSpace1D* GetBlendSpace() const
	{
		return BlendSpace;
	}
	/** Bones of the skeleton, 0 without one. */
	[[nodiscard]] int32 GetNumBones() const;

private:
	UPROPERTY(Transient)
	USkeletalMeshComponent* OwningMesh = nullptr;

	UPROPERTY(Transient)
	const USkeleton* Skeleton = nullptr;

	UPROPERTY(Transient)
	const UBlendSpace1D* BlendSpace = nullptr;

	float BlendInput = 0.0f;
	float BlendInputTarget = 0.0f;
	float LocomotionBlendInterpSpeed = 0.0f;
	float BlendAlpha = 0.0f;

	/** The blend space's clips of the last update. */
	UPROPERTY(Transient)
	const UAnimSequence* SampleA = nullptr;

	UPROPERTY(Transient)
	const UAnimSequence* SampleB = nullptr;

	float TimeA = 0.0f;
	float TimeB = 0.0f;
};
