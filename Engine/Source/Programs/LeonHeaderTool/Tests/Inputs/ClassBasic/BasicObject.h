// Every scalar property type, access sections, specifiers with an effect, API macro, final and multiple inheritance.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BasicObject.generated.h"

#define BASIC_OBJECT_MAX_COUNT 3

/** Not reflected: a second base after the reflected super. */
class FTickableHelper
{
public:
	virtual ~FTickableHelper() = default;
};

UCLASS(Config = Game, Abstract, Transient, BlueprintType, meta = (DisplayName = "Basic Object"))
class LHTTEST_API UBasicObject final : public UObject, public FTickableHelper
{
	GENERATED_BODY()

public:
	UBasicObject();

	/** Health in points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Health = 100.0f;

	UPROPERTY(Config)
	int32 MaxAmmo = 30;

	UPROPERTY(VisibleAnywhere)
	uint8 bIsAlive : 1;

	UPROPERTY(GlobalConfig)
	bool bNativeFlag = false;

	UPROPERTY(Transient)
	double Precise = 0.0;

	UPROPERTY()
	int8 Small = -1;

	UPROPERTY()
	int16 Medium;

	UPROPERTY()
	int64 Large = 1ll << 40;

	UPROPERTY()
	uint8 Byte = 0xff;

	UPROPERTY()
	uint16 UnsignedMedium;

	UPROPERTY()
	uint32 UnsignedValue = 7u;

	UPROPERTY()
	uint64 UnsignedLarge{42};

	UPROPERTY(EditDefaultsOnly, SaveGame)
	FString DisplayName = TEXT("Basic");

	UPROPERTY(DuplicateTransient)
	FName Tag = NAME_None;

	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay)
	FText Title;

	void Tick(float DeltaTime)
	{
		if (Health > 0.0f)
		{
			Health -= DeltaTime;
		}
	}

protected:
	UPROPERTY(EditInstanceOnly, NoClear)
	int32 ProtectedValue;

#if !UE_BUILD_SHIPPING
	int32 DebugCounter = 0;
#endif

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int32 PrivateCounter = 0;

	static constexpr int32 MaxCount = BASIC_OBJECT_MAX_COUNT;
	int32 NotReflected[MaxCount];
};

/* A class without constructors gets the FObjectInitializer one from GENERATED_BODY. */
UCLASS()
class UPlainObject : public UObject
{
	GENERATED_BODY()
};
