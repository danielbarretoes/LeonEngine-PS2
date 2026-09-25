// Reflected fixtures of the CoreUObject config and console command tests.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "ConfigExecTestTypes.generated.h"

UENUM()
enum class EConfigTestMode : uint8
{
	Off,
	Low,
	High
};

USTRUCT()
struct FConfigTestEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 Count = 0;
};

/** Config members of every kind, in the section /Script/CoreUObject.ConfigTestObject of the Game config. */
UCLASS(Config = Game)
class UConfigTestObject : public UObject
{
	GENERATED_BODY()

public:
	UConfigTestObject(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Config)
	int32 IntValue = 1;

	UPROPERTY(Config)
	float FloatValue = 1.0f;

	UPROPERTY(Config)
	bool bFlag = false;

	UPROPERTY(Config)
	FString StringValue = TEXT("Default");

	UPROPERTY(Config)
	FName NameValue;

	UPROPERTY(Config)
	FText TextValue;

	UPROPERTY(Config)
	EConfigTestMode Mode = EConfigTestMode::Off;

	UPROPERTY(Config)
	FVector Location = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(Config)
	FConfigTestEntry Entry;

	UPROPERTY(Config)
	TArray<FString> Items;

	UPROPERTY(Config)
	TArray<FConfigTestEntry> Entries;

	UPROPERTY(Config)
	TArray<int32> Indexed;

	UPROPERTY(Config)
	int32 Fixed[3];

	UPROPERTY(Config)
	TSet<FName> Tags;

	UPROPERTY(Config)
	TMap<FName, int32> Scores;

	UPROPERTY(Config)
	UObject* ObjectRef = nullptr;

	UPROPERTY(Config)
	TSubclassOf<UObject> ClassRef;

	UPROPERTY(Config)
	FSoftObjectPath SoftPath;

	UPROPERTY(Config)
	FSoftClassPath SoftClass;

	UPROPERTY(Config)
	TSoftObjectPtr<UObject> SoftObject;

	/** Always read from this class's section, whatever the object's class. */
	UPROPERTY(GlobalConfig)
	int32 GlobalValue = 0;

	/** Not config: never loaded or saved. */
	UPROPERTY()
	int32 NotConfig = 5;

	/** Not reflected: counts PostReloadConfig calls. */
	int32 ReloadCount = 0;

	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
};

/** A subclass: its own section overrides its parent's (Config is inherited). */
UCLASS()
class UConfigTestChild : public UConfigTestObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	int32 ChildValue = 0;
};

/** One section per object: "<Name> ConfigTestPerObject". */
UCLASS(Config = Game, PerObjectConfig)
class UConfigTestPerObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	FString Label;

	UPROPERTY(Config)
	int32 Level = 0;
};

/** Console commands: UFUNCTION(Exec) with parameters of every kind CallFunctionByNameWithArguments parses. */
UCLASS()
class UExecTestObject : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void SetValues(int32 InInt, float InFloat, FName InName);

	UFUNCTION(Exec)
	void SetMode(EConfigTestMode InMode);

	UFUNCTION(Exec)
	void SetTarget(UObject* InTarget);

	/** The last FString parameter takes the rest of the line. */
	UFUNCTION(Exec)
	void Say(int32 InTimes, FString InMessage);

	/** A first object parameter receives the executor. */
	UFUNCTION(Exec)
	void Greet(UObject* InExecutor, FString InName);

	UFUNCTION()
	void NotExec(int32 Value);

	int32 IntValue = 0;
	float FloatValue = 0.0f;
	FName NameValue;
	EConfigTestMode Mode = EConfigTestMode::Off;
	UObject* Target = nullptr;
	int32 Times = 0;
	FString Message;
	UObject* Executor = nullptr;
	int32 NumCalls = 0;
};
