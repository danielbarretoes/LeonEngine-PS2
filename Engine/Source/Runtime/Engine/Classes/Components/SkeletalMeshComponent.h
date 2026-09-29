#pragma once

#include "Animation/AnimInstance.h"
#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "SkeletalMeshComponent.generated.h"

class USkeletalMesh;

/**
 * What a skeletal mesh does when it is not drawn (UE: EVisibilityBasedAnimTickOption, trimmed): the anim instance
 * always updates (montages, notifies and the state machine keep their time); AlwaysTickPose skips the pose's evaluation
 * while the mesh has not been drawn recently (UPrimitiveComponent::WasRecentlyRendered), and the sockets keep the last
 * pose.
 */
enum class EVisibilityBasedAnimTickOption : uint8
{
	AlwaysTickPoseAndRefreshBones,
	AlwaysTickPose,
};

/**
 * The update rate optimization's settings (UE: FAnimUpdateRateParameters' distance factors; Leon measures the distance
 * to the nearest view drawn last frame, UWorld::ViewLocationsRenderedLastFrame), from
 * `[/Script/Engine.AnimationSettings]` of the engine config: the pose is evaluated every frame within
 * UpdateRateDistanceStep, every second frame within twice that, and so on up to every MaxUpdateRate frames.
 */
struct ENGINE_API FAnimUpdateRateSettings
{
	/** Centimetres per step of the rate. */
	float UpdateRateDistanceStep = 1500.0f;
	/** The fewest evaluations: one every this many frames. */
	int32 MaxUpdateRate = 4;

	/** The config's values over the defaults (read once). */
	[[nodiscard]] static const FAnimUpdateRateSettings& Get();

	/** Frames between evaluations at Distance (cm) from the nearest view: 1 near, up to MaxUpdateRate. */
	[[nodiscard]] int32 GetUpdateRate(float Distance) const;
};

/**
 * Unreal-like USkeletalMeshComponent — a mesh component with a skeletal mesh + UAnimInstance (UE derives it from
 * USkinnedMeshComponent; Leon has no skinned base yet).
 *
 * Its bones and its skeleton's sockets are sockets: a component attached with a bone or socket name (AttachToComponent
 * / SetupAttachment with the socket name, a UStaticMeshComponent weapon for example) follows the animated bone, offset
 * by the socket's relative transform.
 *
 * The pose cache (Docs/PLANS/ps2-shipping.md N25): the pose is evaluated at most once per update
 * (RefreshBoneTransforms, after the anim instance's update, every temporary on the frame's stack): the anim instance's
 * local pose becomes component-space matrices, which the sockets, the skin matrices and the pose bounds the renderer
 * culls with all read; nothing samples again until the next evaluation.
 *
 * Throttling: the anim instance updates every tick, but the evaluation may be skipped, keeping the last pose:
 * - VisibilityBasedAnimTickOption AlwaysTickPose skips it while the mesh is not drawn (culled, or no view);
 * - bEnableUpdateRateOptimizations (UE: URO) evaluates every GetUpdateRate() frames by the distance to the nearest view
 *   (FAnimUpdateRateSettings), staggered by the component's id so that far characters do not all evaluate together.
 * The first evaluation always happens; GetNumPoseEvaluations counts them.
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

	/** A slot's material: the component's override, else the mesh's (UE: GetMaterial). */
	[[nodiscard]] UMaterialInterface* GetMaterial(int32 ElementIndex) const override;

	/** Scales the mesh to FitHeight (world units, cm; its Z extent) and stands it on the component origin. */
	void ApplyFitHeight(float FitHeight);

	/** The index of a bone of the mesh's skeleton, or INDEX_NONE (UE: GetBoneIndex). */
	[[nodiscard]] int32 GetBoneIndex(FName BoneName) const;

	/**
	 * Every bone's component-space matrix of the current pose (UE: GetComponentSpaceTransforms), evaluated when the
	 * pose changed since the last call; empty without a valid mesh.
	 */
	[[nodiscard]] const TArray<FMatrix>& GetComponentSpaceTransforms() const;

	/**
	 * Evaluates the anim instance's pose into the component-space matrices (UE: RefreshBoneTransforms). The update
	 * does it (unless throttled); call it after changing the anim instance's state outside an update.
	 */
	void RefreshBoneTransforms();

	/** What the mesh does when not drawn (UE: VisibilityBasedAnimTickOption); UE's default refreshes always. */
	EVisibilityBasedAnimTickOption VisibilityBasedAnimTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	/** Evaluates less often far from the views (UE: bEnableUpdateRateOptimizations, off by default). */
	UPROPERTY()
	bool bEnableUpdateRateOptimizations = false;

	/** The pose's evaluations since the component was made (the throttling's measure). */
	[[nodiscard]] int32 GetNumPoseEvaluations() const
	{
		return NumPoseEvaluations;
	}
	/** Frames between evaluations at the last update (1: every frame; 0: skipped, not drawn). */
	[[nodiscard]] int32 GetUpdateRate() const
	{
		return UpdateRate;
	}

	/** The current pose's bounds in the component's space (USkeletalMesh::GetPoseBounds). */
	[[nodiscard]] FBox GetPoseBounds() const;

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
	/** Sends the pose's skin matrices and bounds to the proxy (UE: SendRenderDynamicData_Concurrent). */
	void SendRenderDynamicData_Concurrent() override;

	/** A mesh with triangles is set. */
	[[nodiscard]] bool HasValidMesh() const;

private:
	/** Gives the anim instance the mesh's skeleton (none without a valid mesh). */
	void BindAnimInstanceToMesh();
	/** Evaluates the pose when it changed (the const path of RefreshBoneTransforms). */
	void RefreshBoneTransformsIfDirty() const;
	/** The throttling's decision for this update (sets UpdateRate). */
	[[nodiscard]] bool ShouldRefreshBoneTransforms();

	/** The animation instance, an inner object of the component (UE: AnimScriptInstance). */
	UPROPERTY(Transient)
	UAnimInstance* AnimInstance = nullptr;

	/** The pose, cached until the next evaluation (the buffers keep their memory from frame to frame). */
	mutable TArray<FTransform> LocalPose;
	mutable TArray<FMatrix> ComponentSpaceTransforms;
	mutable bool bPoseDirty = true;
	mutable int32 NumPoseEvaluations = 0;
	TArray<FMatrix> SkinMatrices;
	/** The evaluation and the proxy the skin matrices were last sent for (a pose not evaluated again is not resent). */
	int32 SentPoseEvaluation = -1;
	const FPrimitiveSceneProxy* SentProxy = nullptr;
	/** Ticks since the component was made, and the rate of the last one. */
	uint32 UpdateCounter = 0;
	int32 UpdateRate = 1;
};
