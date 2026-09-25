// Reflected fixtures of the CoreUObject automation tests: every property type, enums, structs and functions.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "ReflectionTestTypes.generated.h"

UENUM()
enum class EReflectionTestMode : uint8
{
	First,
	Second,
	Third = 10
};

UENUM()
enum EReflectionTestLegacy
{
	RTL_Alpha,
	RTL_Beta,
	RTL_Gamma UMETA(DisplayName = "Gamma")
};

/** A struct compared with operator== (TStructOpsTypeTraits::WithIdenticalViaEquality) and hashable. */
USTRUCT()
struct FReflectionTestInner
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FString Label;

	bool operator==(const FReflectionTestInner& Other) const
	{
		return Id == Other.Id && Label == Other.Label;
	}

	friend uint32 GetTypeHash(const FReflectionTestInner& Value)
	{
		return GetTypeHash(Value.Id);
	}
};

template <>
struct TStructOpsTypeTraits<FReflectionTestInner> : public TStructOpsTypeTraitsBase2<FReflectionTestInner>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};

/** A derived struct compared property by property. */
USTRUCT()
struct FReflectionTestStruct : public FReflectionTestInner
{
	GENERATED_BODY()

	UPROPERTY()
	float Weight = 1.5f;

	UPROPERTY()
	TArray<FName> Tags;
};

/** Every supported property type, with defaults set in the class body and the constructor. */
UCLASS()
class UReflectionTestObject : public UObject
{
	GENERATED_BODY()

public:
	UReflectionTestObject();

	UPROPERTY()
	int8 Int8Value = -8;

	UPROPERTY()
	int16 Int16Value = -16;

	UPROPERTY()
	int32 IntValue = 42;

	UPROPERTY()
	int64 Int64Value = 1ll << 40;

	UPROPERTY()
	uint8 ByteValue = 200;

	UPROPERTY()
	uint16 UInt16Value = 60000;

	UPROPERTY()
	uint32 UInt32Value = 4000000000u;

	UPROPERTY()
	uint64 UInt64Value = 1ull << 63;

	UPROPERTY()
	float FloatValue = 2.5f;

	UPROPERTY()
	double DoubleValue = 0.25;

	UPROPERTY()
	bool bNativeBool = true;

	UPROPERTY()
	uint8 bFlagA : 1;

	UPROPERTY()
	uint8 bFlagB : 1;

	UPROPERTY()
	uint32 bFlagC : 1;

	UPROPERTY()
	FString StringValue = TEXT("Hello");

	UPROPERTY()
	FName NameValue = TEXT("Leon");

	UPROPERTY()
	FText TextValue;

	UPROPERTY()
	EReflectionTestMode Mode = EReflectionTestMode::Second;

	UPROPERTY()
	TEnumAsByte<EReflectionTestLegacy> Legacy = RTL_Beta;

	UPROPERTY()
	FReflectionTestStruct StructValue;

	UPROPERTY()
	FVector Location = FVector(1.0f, 2.0f, 3.0f);

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	UObject* ObjectRef = nullptr;

	UPROPERTY()
	TSubclassOf<UObject> ClassRef;

	UPROPERTY()
	TWeakObjectPtr<UObject> WeakRef;

	UPROPERTY()
	TSoftObjectPtr<UObject> SoftRef;

	UPROPERTY()
	TArray<int32> IntArray;

	UPROPERTY()
	TArray<FString> StringArray;

	UPROPERTY()
	TArray<FReflectionTestInner> StructArray;

	UPROPERTY()
	TSet<FName> NameSet;

	UPROPERTY()
	TMap<FName, int32> NameToInt;

	UPROPERTY()
	TMap<FString, FReflectionTestInner> StringToStruct;

	UPROPERTY()
	float FixedArray[3];

	UPROPERTY(Config)
	int32 ConfigValue = 7;

	UPROPERTY(Transient)
	int32 TransientValue = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString EditorNote = TEXT("Editor");

	UPROPERTY()
	int32 EditorCount = 3;
#endif

	UPROPERTY()
	int32 AfterEditorOnly = 9;

	/** Not reflected: counts Reset calls. */
	int32 ResetCount = 0;

	UFUNCTION()
	int32 AddNumbers(int32 A, int32 B) const;

	UFUNCTION()
	FString Describe(const FString& Prefix, float Scale, bool bLoud) const;

	UFUNCTION()
	FReflectionTestInner MakeInner(int32 Id, FName Label) const;

	UFUNCTION()
	int32 SumArray(const TArray<int32>& Values) const;

	UFUNCTION()
	static int64 Twice(int64 Value);

	UFUNCTION()
	void SetMode(EReflectionTestMode NewMode);

	UFUNCTION()
	bool IsSameObject(UObject* Other) const;

	UFUNCTION()
	void Reset();
};
