#pragma once

#include "Components/InputComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerInput.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "InputSettings.generated.h"

/**
 * The project's input (UE: UInputSettings), read from [/Script/Engine.InputSettings] of the Input config:
 * Engine/Config/BaseInput.ini, then the project's DefaultInput.ini (`+ActionMappings=(ActionName="Jump",Key=SpaceBar)`,
 * `+AxisMappings=(AxisName="MoveForward",Key=W,Scale=1.0)`, `+AxisConfig=(AxisKeyName="MouseX",AxisProperties=(...))`).
 * Every player controller's UPlayerInput starts from these mappings.
 */
UCLASS(Config = Input, DefaultConfig)
class ENGINE_API UInputSettings : public UObject
{
	GENERATED_BODY()

public:
	UInputSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** How each axis key's raw value is shaped: dead zone, sensitivity, exponent, invert (UE: AxisConfig). */
	UPROPERTY(Config)
	TArray<FInputAxisConfigEntry> AxisConfig;

	/** The action mappings (UE: ActionMappings). */
	UPROPERTY(Config)
	TArray<FInputActionKeyMapping> ActionMappings;

	/** The axis mappings (UE: AxisMappings). */
	UPROPERTY(Config)
	TArray<FInputAxisKeyMapping> AxisMappings;

	/** Scales the mouse axes by the field of view (UE: bEnableFOVScaling; off in Leon's BaseInput.ini). */
	UPROPERTY(Config)
	bool bEnableFOVScaling = false;

	/** The mouse scale per degree of field of view when FOV scaling is on (UE: FOVScale). */
	UPROPERTY(Config)
	float FOVScale = 0.01111f;

	/** How the game window captures the mouse (UE: DefaultViewportMouseCaptureMode). */
	UPROPERTY(Config)
	EMouseCaptureMode DefaultViewportMouseCaptureMode = EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;

	/** The class of a player controller's input (UE: DefaultPlayerInputClass). */
	UPROPERTY(Config)
	TSoftClassPtr<UPlayerInput> DefaultPlayerInputClass;

	/** The class of the input components the engine makes (UE: DefaultInputComponentClass). */
	UPROPERTY(Config)
	TSoftClassPtr<UInputComponent> DefaultInputComponentClass;

	/** The settings (UE: GetInputSettings): the class default object. */
	[[nodiscard]] static UInputSettings* GetInputSettings();

	/** DefaultPlayerInputClass, UPlayerInput when it is not set (UE). */
	[[nodiscard]] static TSubclassOf<UPlayerInput> GetDefaultPlayerInputClass();

	/** DefaultInputComponentClass, UInputComponent when it is not set (UE). */
	[[nodiscard]] static TSubclassOf<UInputComponent> GetDefaultInputComponentClass();

	/** Adds a mapping to the settings, not saved (UE: AddActionMapping / AddAxisMapping). */
	void AddActionMapping(const FInputActionKeyMapping& KeyMapping);
	void AddAxisMapping(const FInputAxisKeyMapping& KeyMapping);

	/** The mappings of one action or axis (UE: GetActionMappingByName / GetAxisMappingByName). */
	void GetActionMappingByName(const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const;
	void GetAxisMappingByName(const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const;
};
