// UFUNCTIONs: exec thunks, parameters of every kind, return values, static / const / virtual functions.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FunctionsObject.generated.h"

USTRUCT()
struct FHitInfo
{
	GENERATED_BODY()

	UPROPERTY()
	float Distance = 0.0f;
};

UENUM()
enum class EFireMode : uint8
{
	Single,
	Burst,
	Auto
};

UENUM()
enum ELegacyChannel
{
	LC_Visibility,
	LC_Camera UMETA(DisplayName = "Camera"),
};

UCLASS()
class UFunctionsObject : public UObject
{
	GENERATED_BODY()

public:
	UFunctionsObject(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(Exec)
	void GiveAmmo(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Weapons")
	bool Fire(EFireMode Mode, const FString& Reason, float Spread = 0.5f);

	UFUNCTION(BlueprintPure)
	FHitInfo TraceAt(const FHitInfo& Previous, UObject* Instigator) const;

	UFUNCTION()
	static int64 AddLarge(int64 A, int64 B);

	UFUNCTION()
	virtual void OnHit(TSubclassOf<UObject> DamageType, FName Bone, const TArray<int32>& Indices,
		TEnumAsByte<ELegacyChannel> Channel, bool bCritical);

	UFUNCTION(BlueprintPure)
	int32 GetCount() const
	{
		return 3;
	}

	UFUNCTION()
	void Reset();

protected:
	UFUNCTION()
	FText GetTitle() const;

private:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Byte"))
	uint8 GetByte();
};
