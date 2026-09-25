#include "GameFramework/InputSettings.h"

UInputSettings::UInputSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UInputSettings* UInputSettings::GetInputSettings()
{
	return GetMutableDefault<UInputSettings>();
}

TSubclassOf<UPlayerInput> UInputSettings::GetDefaultPlayerInputClass()
{
	UClass* Class = GetDefault<UInputSettings>()->DefaultPlayerInputClass.LoadSynchronous();
	return Class != nullptr && Class->IsChildOf(UPlayerInput::StaticClass()) ? Class : UPlayerInput::StaticClass();
}

TSubclassOf<UInputComponent> UInputSettings::GetDefaultInputComponentClass()
{
	UClass* Class = GetDefault<UInputSettings>()->DefaultInputComponentClass.LoadSynchronous();
	return Class != nullptr && Class->IsChildOf(UInputComponent::StaticClass()) ? Class
																				: UInputComponent::StaticClass();
}

void UInputSettings::AddActionMapping(const FInputActionKeyMapping& KeyMapping)
{
	ActionMappings.AddUnique(KeyMapping);
}

void UInputSettings::AddAxisMapping(const FInputAxisKeyMapping& KeyMapping)
{
	AxisMappings.AddUnique(KeyMapping);
}

void UInputSettings::GetActionMappingByName(const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const
{
	OutMappings.Reset();
	for (const FInputActionKeyMapping& Mapping : ActionMappings)
	{
		if (Mapping.ActionName == InActionName)
		{
			OutMappings.Add(Mapping);
		}
	}
}

void UInputSettings::GetAxisMappingByName(const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const
{
	OutMappings.Reset();
	for (const FInputAxisKeyMapping& Mapping : AxisMappings)
	{
		if (Mapping.AxisName == InAxisName)
		{
			OutMappings.Add(Mapping);
		}
	}
}
