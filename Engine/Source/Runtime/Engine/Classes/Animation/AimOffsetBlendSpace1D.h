#pragma once

#include "Animation/BlendSpace1D.h"
#include "CoreMinimal.h"
#include "AimOffsetBlendSpace1D.generated.h"

/**
 * An aim offset on one axis (UE: UAimOffsetBlendSpace1D): poses of the upper body aiming at pitches from -90 to +90
 * degrees (3 to 5 samples), applied as an additive on top of the rest of the anim graph (UAnimInstance::SetAimOffset).
 *
 * UE imports the samples as additive clips. Leon keeps them as full poses and makes the additive at run time: the blend
 * of the samples minus BasePose (the pose aiming straight ahead, pitch 0), so a pitch of 0 adds nothing whatever the
 * body is doing. Without a BasePose the sample nearest 0 on the axis is the base.
 */
UCLASS()
class ENGINE_API UAimOffsetBlendSpace1D : public UBlendSpace1D
{
	GENERATED_BODY()

public:
	UAimOffsetBlendSpace1D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The pose the additive is measured from (its first frame). */
	UPROPERTY()
	UAnimSequence* BasePose = nullptr;

	/** BasePose, else the clip of the sample nearest 0 on the axis; null without samples. */
	[[nodiscard]] const UAnimSequence* GetAdditiveBasePose() const;
};
