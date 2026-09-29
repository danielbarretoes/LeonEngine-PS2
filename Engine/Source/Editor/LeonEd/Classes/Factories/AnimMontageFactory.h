#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AnimMontageFactory.generated.h"

/**
 * Makes a montage from a description (UE: UAnimMontageFactory, which the editor makes from an animation): an
 * ImportList.ini section (UImportAssetsCommandlet, Docs/TOOLS.md) whose keys are these settings:
 *
 *     [AM_Rifle_Reload]
 *     Type=AnimMontage
 *     Dest=/Game/Characters/Animations
 *     Animation=A_Rifle_Reload
 *     SlotName=UpperBody
 *     BlendInTime=0.1
 *     BlendOutTime=0.2
 *     +Section=Start,0
 *     +Section=Insert,0.8,Insert
 *     +Notify=MagOut,0.4
 *
 * The animation's own notifies (its glTF extras) play with it; `Notify` adds the montage's. Sections are
 * `Name,StartTime[,NextSection]`.
 */
UCLASS()
class LEONED_API UAnimMontageFactory : public UFactory
{
	GENERATED_BODY()

public:
	UAnimMontageFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The clip: a name in the same folder or a long object path. */
	UPROPERTY()
	FString Animation;

	/** UAnimMontage::SlotName (DefaultSlot, UpperBody). */
	UPROPERTY()
	FString SlotName = TEXT("DefaultSlot");

	UPROPERTY()
	float BlendInTime = 0.1f;

	UPROPERTY()
	float BlendOutTime = 0.15f;

	/** The sections, `Name,StartTime[,NextSection]` each, separated by `;`. */
	UPROPERTY()
	FString Section;

	/** The montage's notifies, `Name,Time` each, separated by `;`. */
	UPROPERTY()
	FString Notify;

	UObject* FactoryCreateNew(
		UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context) override;
};
