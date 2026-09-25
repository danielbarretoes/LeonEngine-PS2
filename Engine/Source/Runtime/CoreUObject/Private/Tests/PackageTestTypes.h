// Reflected fixtures of the CoreUObject package tests (save / load, schema evolution, subobjects).
#pragma once

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "PackageTestTypes.generated.h"

UENUM()
enum class EPackageTestMode : uint8
{
	First,
	Second,
	Third = 10
};

UENUM()
enum EPackageTestLegacy
{
	PTL_Alpha,
	PTL_Beta,
	PTL_Gamma
};

/** The enum class a TEnumAsByte<EPackageTestLegacy> property became (schema evolution: byte to enum by name). */
UENUM()
enum class EPackageTestLegacyV2 : uint8
{
	PTL_Alpha,
	PTL_Beta,
	PTL_Gamma
};

/** A struct saved as nested tagged properties. */
USTRUCT()
struct FPackageTestInner
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Count = 5;

	UPROPERTY()
	FString Label = TEXT("Inner");

	UPROPERTY()
	FName Tag;
};

/** A struct holding a struct, an array and an immutable (binary) Core struct. */
USTRUCT()
struct FPackageTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	FPackageTestInner Inner;

	UPROPERTY()
	TArray<int32> Numbers;

	UPROPERTY()
	FVector Offset = FVector(1.0f, 2.0f, 3.0f);

	UPROPERTY()
	float Scale = 1.0f;
};

/**
 * Every kind of property, plus native data after them (NativeValue and BulkData in Serialize). PostLoad records the
 * order of the calls and what a referenced object held at that point.
 */
UCLASS()
class UPackageTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPackageTestObject();

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

	/** The PostLoad calls since the last Reset, as path names. */
	static TArray<FString>& GetPostLoadLog();

	UPROPERTY()
	int8 Int8Value = 0;

	UPROPERTY()
	int16 Int16Value = 0;

	UPROPERTY()
	int32 IntValue = 0;

	UPROPERTY()
	int64 Int64Value = 0;

	UPROPERTY()
	uint8 ByteValue = 0;

	UPROPERTY()
	uint16 UInt16Value = 0;

	UPROPERTY()
	uint32 UInt32Value = 0;

	UPROPERTY()
	uint64 UInt64Value = 0;

	UPROPERTY()
	float FloatValue = 0.0f;

	UPROPERTY()
	double DoubleValue = 0.0;

	UPROPERTY()
	bool bNativeBool = false;

	UPROPERTY()
	uint8 bBitA : 1;

	UPROPERTY()
	uint8 bBitB : 1;

	UPROPERTY()
	uint32 bBitC : 1;

	UPROPERTY()
	FString StringValue;

	UPROPERTY()
	FName NameValue;

	UPROPERTY()
	FText TextValue;

	UPROPERTY()
	EPackageTestMode Mode = EPackageTestMode::First;

	UPROPERTY()
	TEnumAsByte<EPackageTestLegacy> LegacyMode = PTL_Alpha;

	UPROPERTY()
	FVector Location = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FLinearColor Color = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY()
	FPackageTestStruct Struct;

	UPROPERTY()
	UObject* ObjectRef = nullptr;

	UPROPERTY()
	UObject* ScriptRef = nullptr;

	UPROPERTY()
	TSubclassOf<UObject> ClassRef;

	UPROPERTY()
	TWeakObjectPtr<UObject> WeakRef;

	UPROPERTY()
	TSoftObjectPtr<UObject> SoftRef;

	UPROPERTY()
	TSoftClassPtr<UObject> SoftClassRef;

	UPROPERTY()
	FSoftObjectPath SoftPath;

	UPROPERTY()
	TArray<int32> IntArray;

	UPROPERTY()
	TArray<FString> StringArray;

	UPROPERTY()
	TArray<FPackageTestInner> StructArray;

	UPROPERTY()
	TArray<FVector> VectorArray;

	UPROPERTY()
	TArray<UObject*> ObjectArray;

	UPROPERTY()
	TArray<bool> BoolArray;

	UPROPERTY()
	TSet<FName> NameSet;

	UPROPERTY()
	TSet<int32> IntSet;

	UPROPERTY()
	TMap<FName, int32> NameToInt;

	UPROPERTY()
	TMap<FString, FPackageTestInner> StringToStruct;

	UPROPERTY()
	TMap<int32, UObject*> IntToObject;

	UPROPERTY()
	int32 FixedArray[3] = {};

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

	/** Native data (not reflected), saved after the tagged properties by Serialize. */
	int32 NativeValue = 0;

	/** A payload saved after the exports (or inline with BULKDATA_ForceInlinePayload). */
	FByteBulkData BulkData;

	/** Set by PostLoad: ObjectRef's IntValue when it is a UPackageTestObject, else INDEX_NONE. */
	int32 PostLoadSeenRefValue = INDEX_NONE;

	/** PostLoad calls on this object. */
	int32 NumPostLoads = 0;
};

/** An object only the editor needs, like an asset's import data: a filtered package leaves it out (UE: IsEditorOnly).
 */
UCLASS()
class UPackageTestEditorOnlyObject : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsEditorOnly() const override
	{
		return true;
	}

	UPROPERTY()
	int32 Value = 0;
};

/** A default subobject (and a runtime inner object) of UPackageTestOwner. */
UCLASS()
class UPackageTestSubobject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Value = 0;

	UPROPERTY()
	FString Label;
};

/** An owner whose constructor creates a default subobject and sets it up (D12). */
UCLASS()
class UPackageTestOwner : public UObject
{
	GENERATED_BODY()

public:
	UPackageTestOwner();

	UPROPERTY()
	UPackageTestSubobject* Component = nullptr;

	UPROPERTY()
	UPackageTestSubobject* Extra = nullptr;

	UPROPERTY()
	int32 OwnerValue = 0;
};

/**
 * Version 1 of a class whose properties change (schema evolution). The test saves one, renames the class in the
 * package's name table to PackageTestSchemaV2 (the same length) and loads it as the version 2 below.
 */
UCLASS()
class UPackageTestSchemaV1 : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Kept = 0;

	UPROPERTY()
	int32 Removed = 0;

	UPROPERTY()
	int32 OldName = 0;

	UPROPERTY()
	int32 Widened = 0;

	UPROPERTY()
	float Precise = 0.0f;

	UPROPERTY()
	uint8 ModeByte = 0;

	UPROPERTY()
	TEnumAsByte<EPackageTestLegacy> LegacyMode = PTL_Alpha;

	UPROPERTY()
	FString Mismatched;

	UPROPERTY()
	FPackageTestInner Nested;

	UPROPERTY()
	int32 After = 0;
};

/** Version 2: Removed is gone, OldName renamed, Widened int64, Precise double, enums, Mismatched an int32. */
UCLASS()
class UPackageTestSchemaV2 : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Kept = 0;

	UPROPERTY()
	int32 NewName = 0;

	UPROPERTY()
	int64 Widened = 0;

	UPROPERTY()
	double Precise = 0.0;

	UPROPERTY()
	EPackageTestMode ModeByte = EPackageTestMode::First;

	UPROPERTY()
	EPackageTestLegacyV2 LegacyMode = EPackageTestLegacyV2::PTL_Alpha;

	UPROPERTY()
	int32 Mismatched = 7;

	UPROPERTY()
	FPackageTestInner Nested;

	UPROPERTY()
	int32 After = 0;
};
