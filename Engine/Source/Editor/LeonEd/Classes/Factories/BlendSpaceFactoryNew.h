#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "BlendSpaceFactoryNew.generated.h"

class UBlendSpaceBase;

/**
 * Makes a blend space from a description (UE: UBlendSpaceFactoryNew, which the editor's UI fills): Leon has no editor,
 * so an ImportList.ini section describes it (UImportAssetsCommandlet, Docs/TOOLS.md) and its keys are these settings:
 *
 *     [BS_Locomotion]
 *     Type=BlendSpace
 *     Dest=/Game/Characters/Animations
 *     AxisX=Speed,0,600
 *     AxisY=Direction,-180,180
 *     +Sample=A_Idle,0,0
 *     +Sample=A_Run_Fwd,600,0
 *
 * A sample names an animation (a name in the same folder, or a long object path) and its coordinates. This class makes
 * a UBlendSpace (2D); UBlendSpaceFactory1D and UAimOffsetBlendSpaceFactory1D make the 1D ones (X only). The samples are
 * added in the order listed, and the triangulation is rebuilt; creating the asset again from the same section writes
 * the same bytes.
 */
UCLASS()
class LEONED_API UBlendSpaceFactoryNew : public UFactory
{
	GENERATED_BODY()

public:
	UBlendSpaceFactoryNew(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The skeleton (a long object path or a name in the same folder); empty: the first sample's. */
	UPROPERTY()
	FString Skeleton;

	/** The first axis: `Name,Min,Max`. */
	UPROPERTY()
	FString AxisX;

	/** The second axis (2D only): `Name,Min,Max`. */
	UPROPERTY()
	FString AxisY;

	/** The samples, `Animation,X[,Y]` each, separated by `;` (repeated ImportList keys are joined so). */
	UPROPERTY()
	FString Sample;

	/** An aim offset's base pose (UAimOffsetBlendSpace1D::BasePose): an animation; empty: the sample nearest 0. */
	UPROPERTY()
	FString BasePose;

	UObject* FactoryCreateNew(
		UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context) override;

protected:
	/** Axes the class's blend spaces have (1 or 2). */
	[[nodiscard]] virtual int32 GetNumAxes() const
	{
		return 2;
	}
};
