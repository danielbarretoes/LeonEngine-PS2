#pragma once

#include "Animation/AnimInstance.h"
#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "SkeletalMeshComponent.generated.h"

class USkeletalMesh;

/**
 * Unreal-like USkeletalMeshComponent — a mesh component with a skeletal mesh + UAnimInstance (UE derives it from
 * USkinnedMeshComponent; Leon has no skinned base yet).
 *
 * Its bones and its skeleton's sockets are sockets: a component attached with a bone or socket name (AttachToComponent
 * / SetupAttachment with the socket name, a UStaticMeshComponent weapon for example) follows the animated bone, offset
 * by the socket's relative transform.
 */
UCLASS()
class ENGINE_API USkeletalMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	USkeletalMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The mesh (UE: SkeletalMesh, on USkinnedMeshComponent). Change it with SetSkeletalMesh. */
	UPROPERTY()
	USkeletalMesh* SkeletalMesh = nullptr;

	/** Sets the mesh and gives its skeleton to the anim instance (UE: SetSkeletalMesh); the proxy is recreated. */
	void SetSkeletalMesh(USkeletalMesh* InMesh);
	[[nodiscard]] USkeletalMesh* GetSkeletalMesh() const
	{
		return SkeletalMesh;
	}

	/** Replaces the anim instance (a new UAnimInstance when null); the component becomes its owner. */
	void SetAnimInstance(UAnimInstance* Instance);
	/** Creates a TAnim with this component as outer (UE: the anim class's instance) and uses it. */
	template <typename TAnim>
	TAnim& SetAnimInstance()
	{
		static_assert(TIsDerivedFrom<TAnim, UAnimInstance>::Value, "TAnim must derive from AnimInstance");
		TAnim* NewInstance = NewObject<TAnim>(this);
		SetAnimInstance(NewInstance);
		return *NewInstance;
	}

	[[nodiscard]] UAnimInstance& GetAnimInstance()
	{
		return *AnimInstance;
	}
	[[nodiscard]] const UAnimInstance& GetAnimInstance() const
	{
		return *AnimInstance;
	}

	template <typename TAnim>
	[[nodiscard]] TAnim* GetAnimInstance()
	{
		return Cast<TAnim>(AnimInstance);
	}
	template <typename TAnim>
	[[nodiscard]] const TAnim* GetAnimInstance() const
	{
		return Cast<TAnim>(AnimInstance);
	}

	/** Scales the mesh to FitHeight (world units, cm; its Z extent) and stands it on the component origin. */
	void ApplyFitHeight(float FitHeight);

	/** Bone model-space matrix from the current UAnimInstance pose. */
	[[nodiscard]] bool GetBoneModelMatrix(const FString& InBoneName, FMatrix& OutModel) const;

	/**
	 * A socket's world transform: a skeleton socket's (its bone's pose, then the socket's offset), a bone's, else the
	 * component's.
	 */
	[[nodiscard]] FTransform GetSocketTransform(FName InSocketName) const override;
	/** True for the names of the skeleton's sockets and of the mesh's bones. */
	[[nodiscard]] bool DoesSocketExist(FName InSocketName) const override;

	void TickComponent(float DeltaTime) override;
	/** A FSkeletalMeshSceneProxy for a valid mesh (UE: CreateSceneProxy). */
	[[nodiscard]] FPrimitiveSceneProxy* CreateSceneProxy() override;
	/** Sends the pose's skin matrices to the proxy (UE: SendRenderDynamicData_Concurrent). */
	void SendRenderDynamicData_Concurrent() override;

	/** A mesh with triangles is set. */
	[[nodiscard]] bool HasValidMesh() const;

private:
	/** Gives the anim instance the mesh's skeleton (none without a valid mesh). */
	void BindAnimInstanceToMesh();

	/** The animation instance, an inner object of the component (UE: AnimScriptInstance). */
	UPROPERTY(Transient)
	UAnimInstance* AnimInstance = nullptr;
	mutable TArray<FMatrix> SkinMatrices;
	mutable TArray<FMatrix> BoneWorldMatrices;
};
