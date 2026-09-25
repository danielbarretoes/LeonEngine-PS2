// Config classes (Config=, DefaultConfig, PerObjectConfig, an inherited config class), Config / GlobalConfig members of
// several kinds and Exec functions.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ConfigObject.generated.h"

UCLASS(Config = Game, DefaultConfig)
class UGameSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TArray<FString> MapCycle;

	UPROPERTY(Config)
	int32 Slots[2];

	UPROPERTY(GlobalConfig)
	float MouseSensitivity = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	TMap<FName, int32> StartMoney;

	UPROPERTY(Transient)
	int32 RuntimeOnly = 0;

	UFUNCTION(Exec)
	void SetSensitivity(float NewSensitivity);

	UFUNCTION(Exec)
	void AddMap(FName MapName, const FString& Description);
};

/* Inherits Config=Game from its super. */
UCLASS()
class UChildSettings : public UGameSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	bool bHardcore = false;
};

UCLASS(Config = Input, PerObjectConfig)
class UBindingSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	FName Action;
};
