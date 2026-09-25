#include "GameFramework/PlayerInput.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "Templates/Sorting.h"

FInputActionKeyMapping::FInputActionKeyMapping(const FName InActionName, const FKey InKey, const bool bInShift,
	const bool bInCtrl, const bool bInAlt, const bool bInCmd)
	: ActionName(InActionName)
	, bShift(bInShift)
	, bCtrl(bInCtrl)
	, bAlt(bInAlt)
	, bCmd(bInCmd)
	, Key(InKey)
{
}

/** An action handler to run, in the order its event arrived (UE: FDelegateDispatchDetails). */
struct UPlayerInput::FDelegateDispatchDetails
{
	uint32 EventIndex = 0;
	uint32 FoundIndex = 0;
	FInputActionHandlerSignature Delegate;
};

UPlayerInput::UPlayerInput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPlayerInput::PostInitProperties()
{
	Super::PostInitProperties();
	// Every player starts from the project's mappings (UE: ForceRebuildingKeyMaps(true) in the constructor).
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ForceRebuildingKeyMaps(true);
	}
}

APlayerController* UPlayerInput::GetOuterAPlayerController() const
{
	return Cast<APlayerController>(GetOuter());
}

void UPlayerInput::ForceRebuildingKeyMaps(const bool bRestoreDefaults)
{
	if (bRestoreDefaults)
	{
		const UInputSettings* Settings = GetDefault<UInputSettings>();
		ActionMappings = Settings->ActionMappings;
		AxisMappings = Settings->AxisMappings;
	}
	ActionKeyMap.Reset();
	AxisKeyMap.Reset();
	AxisProperties.Reset();
	bKeyMapsBuilt = false;
	bAxisPropertiesInitialized = false;
}

void UPlayerInput::ConditionalBuildKeyMappings()
{
	if (bKeyMapsBuilt)
	{
		return;
	}
	for (const FInputActionKeyMapping& Mapping : ActionMappings)
	{
		ActionKeyMap.FindOrAdd(Mapping.ActionName).Actions.Add(Mapping);
	}
	for (const FInputAxisKeyMapping& Mapping : AxisMappings)
	{
		AxisKeyMap.FindOrAdd(Mapping.AxisName).KeyMappings.Add(Mapping);
	}
	bKeyMapsBuilt = true;
}

void UPlayerInput::ConditionalInitAxisProperties()
{
	if (bAxisPropertiesInitialized)
	{
		return;
	}
	for (const FInputAxisConfigEntry& Entry : GetDefault<UInputSettings>()->AxisConfig)
	{
		const FKey AxisKey(Entry.AxisKeyName);
		if (AxisKey.IsValid())
		{
			AxisProperties.Add(AxisKey, Entry.AxisProperties);
		}
	}
	bAxisPropertiesInitialized = true;
}

void UPlayerInput::AddActionMapping(const FInputActionKeyMapping& KeyMapping)
{
	ActionMappings.AddUnique(KeyMapping);
	ForceRebuildingKeyMaps(false);
}

void UPlayerInput::AddAxisMapping(const FInputAxisKeyMapping& KeyMapping)
{
	AxisMappings.AddUnique(KeyMapping);
	ForceRebuildingKeyMaps(false);
}

void UPlayerInput::RemoveActionMapping(const FInputActionKeyMapping& KeyMapping)
{
	ActionMappings.Remove(KeyMapping);
	ForceRebuildingKeyMaps(false);
}

void UPlayerInput::RemoveAxisMapping(const FInputAxisKeyMapping& KeyMapping)
{
	AxisMappings.Remove(KeyMapping);
	ForceRebuildingKeyMaps(false);
}

const TArray<FInputActionKeyMapping>& UPlayerInput::GetKeysForAction(const FName ActionName) const
{
	const_cast<UPlayerInput*>(this)->ConditionalBuildKeyMappings();
	static const TArray<FInputActionKeyMapping> NoKeyMappings;
	const FActionKeyDetails* Details = ActionKeyMap.Find(ActionName);
	return Details != nullptr ? Details->Actions : NoKeyMappings;
}

const TArray<FInputAxisKeyMapping>& UPlayerInput::GetKeysForAxis(const FName AxisName) const
{
	const_cast<UPlayerInput*>(this)->ConditionalBuildKeyMappings();
	static const TArray<FInputAxisKeyMapping> NoAxisMappings;
	const FAxisKeyDetails* Details = AxisKeyMap.Find(AxisName);
	return Details != nullptr ? Details->KeyMappings : NoAxisMappings;
}

bool UPlayerInput::InputKey(FKey Key, EInputEvent Event, float AmountDepressed, bool /*bGamepad*/)
{
	FKeyState& KeyState = KeyStateMap.FindOrAdd(Key);
	switch (Event)
	{
		case IE_Pressed:
		case IE_Repeat:
			KeyState.RawValueAccumulator = AmountDepressed;
			KeyState.EventAccumulator[Event].Add(++EventCount);
			break;
		case IE_Released:
			KeyState.RawValueAccumulator = 0.0f;
			KeyState.EventAccumulator[IE_Released].Add(++EventCount);
			break;
		case IE_DoubleClick:
			KeyState.RawValueAccumulator = AmountDepressed;
			KeyState.EventAccumulator[IE_Pressed].Add(++EventCount);
			KeyState.EventAccumulator[IE_DoubleClick].Add(++EventCount);
			break;
		default:
			break;
	}
	++KeyState.SampleCountAccumulator;
	return Event == IE_Pressed ? IsKeyHandledByAction(Key) : true;
}

bool UPlayerInput::InputAxis(FKey Key, float Delta, float /*DeltaTime*/, int32 NumSamples, bool /*bGamepad*/)
{
	FKeyState& KeyState = KeyStateMap.FindOrAdd(Key);
	KeyState.RawValueAccumulator += Delta;
	KeyState.SampleCountAccumulator = static_cast<uint8>(
		FMath::Min(static_cast<int32>(KeyState.SampleCountAccumulator) + FMath::Max(NumSamples, 1), 255));
	return true;
}

bool UPlayerInput::IsKeyHandledByAction(FKey Key) const
{
	for (const FInputActionKeyMapping& Mapping : ActionMappings)
	{
		if (Mapping.Key == Key || Mapping.Key == EKeys::AnyKey)
		{
			return true;
		}
	}
	return false;
}

float UPlayerInput::MassageAxisInput(FKey Key, float RawValue)
{
	float NewVal = RawValue;
	ConditionalInitAxisProperties();
	if (const FInputAxisProperties* Props = AxisProperties.Find(Key))
	{
		// The dead zone, rescaled to [-1, 1] after it (UE).
		if (Props->DeadZone > 0.0f)
		{
			if (NewVal > 0.0f)
			{
				NewVal = FMath::Max(0.0f, NewVal - Props->DeadZone) / (1.0f - Props->DeadZone);
			}
			else
			{
				NewVal = -FMath::Max(0.0f, -NewVal - Props->DeadZone) / (1.0f - Props->DeadZone);
			}
		}
		if (Props->Exponent != 1.0f)
		{
			NewVal = FMath::Sign(NewVal) * FMath::Pow(FMath::Abs(NewVal), Props->Exponent);
		}
		NewVal *= Props->Sensitivity;
		if (Props->bInvert != 0)
		{
			NewVal *= -1.0f;
		}
	}
	// The mouse follows the field of view when the settings ask for it (UE; no mouse smoothing in Leon).
	if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		const UInputSettings* Settings = GetDefault<UInputSettings>();
		const APlayerController* PlayerController = GetOuterAPlayerController();
		if (Settings->bEnableFOVScaling && PlayerController != nullptr &&
			PlayerController->PlayerCameraManager != nullptr)
		{
			NewVal *= Settings->FOVScale * PlayerController->PlayerCameraManager->GetFOVAngle();
		}
	}
	return NewVal;
}

bool UPlayerInput::IsPressed(FKey InKey) const
{
	const FKeyState* KeyState = KeyStateMap.Find(InKey);
	return KeyState != nullptr && KeyState->bDown != 0;
}

bool UPlayerInput::WasJustPressed(FKey InKey) const
{
	const FKeyState* KeyState = KeyStateMap.Find(InKey);
	return KeyState != nullptr && KeyState->EventCounts[IE_Pressed].Num() > 0;
}

bool UPlayerInput::WasJustReleased(FKey InKey) const
{
	const FKeyState* KeyState = KeyStateMap.Find(InKey);
	return KeyState != nullptr && KeyState->EventCounts[IE_Released].Num() > 0;
}

float UPlayerInput::GetKeyValue(FKey InKey) const
{
	const FKeyState* KeyState = KeyStateMap.Find(InKey);
	return KeyState != nullptr ? KeyState->Value : 0.0f;
}

float UPlayerInput::GetRawKeyValue(FKey InKey) const
{
	const FKeyState* KeyState = KeyStateMap.Find(InKey);
	return KeyState != nullptr ? KeyState->RawValue : 0.0f;
}

bool UPlayerInput::IsKeyConsumed(FKey Key) const
{
	const FKeyState* KeyState = KeyStateMap.Find(Key);
	return KeyState != nullptr && KeyState->bConsumed != 0;
}

void UPlayerInput::ConsumeKey(FKey Key)
{
	KeyStateMap.FindOrAdd(Key).bConsumed = 1;
}

bool UPlayerInput::KeyEventOccurred(FKey Key, EInputEvent Event, TArray<uint32>& EventIndices) const
{
	const FKeyState* KeyState = KeyStateMap.Find(Key);
	if (KeyState != nullptr && KeyState->EventCounts[Event].Num() > 0)
	{
		EventIndices = KeyState->EventCounts[Event];
		return true;
	}
	return false;
}

bool UPlayerInput::ModifiersMatch(const FInputActionKeyMapping& Mapping, const UPlayerInput& Input)
{
	const bool bShift = Input.IsPressed(EKeys::LeftShift) || Input.IsPressed(EKeys::RightShift);
	const bool bCtrl = Input.IsPressed(EKeys::LeftControl) || Input.IsPressed(EKeys::RightControl);
	const bool bAlt = Input.IsPressed(EKeys::LeftAlt) || Input.IsPressed(EKeys::RightAlt);
	const bool bCmd = Input.IsPressed(EKeys::LeftCommand) || Input.IsPressed(EKeys::RightCommand);
	return (Mapping.bShift == 0 || bShift) && (Mapping.bCtrl == 0 || bCtrl) && (Mapping.bAlt == 0 || bAlt) &&
		(Mapping.bCmd == 0 || bCmd);
}

void UPlayerInput::ProcessInputStack(
	const TArray<UInputComponent*>& InputComponentStack, const float /*DeltaTime*/, const bool bGamePaused)
{
	ConditionalBuildKeyMappings();

	// The events and samples since the last frame become this frame's state (UE).
	for (auto& Pair : KeyStateMap)
	{
		const FKey& Key = Pair.Key;
		FKeyState& KeyState = Pair.Value;
		uint32 LastDown = 0;
		uint32 LastUp = 0;
		for (int32 EventIndex = 0; EventIndex < IE_MAX; ++EventIndex)
		{
			KeyState.EventCounts[EventIndex] = MoveTemp(KeyState.EventAccumulator[EventIndex]);
			KeyState.EventAccumulator[EventIndex].Reset();
		}
		for (const uint32 Index : KeyState.EventCounts[IE_Pressed])
		{
			LastDown = FMath::Max(LastDown, Index);
		}
		for (const uint32 Index : KeyState.EventCounts[IE_Repeat])
		{
			LastDown = FMath::Max(LastDown, Index);
		}
		for (const uint32 Index : KeyState.EventCounts[IE_Released])
		{
			LastUp = FMath::Max(LastUp, Index);
		}
		if (LastDown != 0 || LastUp != 0)
		{
			KeyState.bDown = LastDown > LastUp ? 1 : 0;
		}
		// Without samples a button keeps its value; an axis that updates without samples (the mouse) reads 0 (UE).
		if (KeyState.SampleCountAccumulator > 0 || Key.ShouldUpdateAxisWithoutSamples())
		{
			KeyState.RawValue = KeyState.RawValueAccumulator;
		}
		KeyState.Value = MassageAxisInput(Key, KeyState.RawValue);
		KeyState.SampleCountAccumulator = 0;
		KeyState.RawValueAccumulator = 0.0f;
	}

	TArray<FDelegateDispatchDetails> ActionDelegates;
	TArray<TPair<FInputAxisHandlerSignature, float>> AxisDelegates;
	TArray<FKey> KeysToConsume;
	TArray<uint32> EventIndices;

	// The stack from the top: each component's bindings take their keys from the components below (UE).
	int32 StackIndex = InputComponentStack.Num() - 1;
	for (; StackIndex >= 0; --StackIndex)
	{
		UInputComponent* const Component = InputComponentStack[StackIndex];
		if (Component == nullptr)
		{
			continue;
		}
		for (int32 BindingIndex = 0; BindingIndex < Component->GetNumActionBindings(); ++BindingIndex)
		{
			const FInputActionBinding& Binding = Component->GetActionBinding(BindingIndex);
			if (bGamePaused && Binding.bExecuteWhenPaused == 0)
			{
				continue;
			}
			const FActionKeyDetails* Details = ActionKeyMap.Find(Binding.GetActionName());
			if (Details == nullptr)
			{
				continue;
			}
			for (const FInputActionKeyMapping& Mapping : Details->Actions)
			{
				if (IsKeyConsumed(Mapping.Key) || !ModifiersMatch(Mapping, *this) ||
					!KeyEventOccurred(Mapping.Key, Binding.KeyEvent, EventIndices))
				{
					continue;
				}
				for (const uint32 EventIndex : EventIndices)
				{
					FDelegateDispatchDetails& Dispatch = ActionDelegates.AddDefaulted_GetRef();
					Dispatch.EventIndex = EventIndex;
					Dispatch.FoundIndex = static_cast<uint32>(ActionDelegates.Num());
					Dispatch.Delegate = Binding.ActionDelegate;
				}
				if (Binding.bConsumeInput != 0)
				{
					KeysToConsume.AddUnique(Mapping.Key);
				}
			}
		}
		for (FInputAxisBinding& Binding : Component->AxisBindings)
		{
			// The axis sums its mapped keys' values times their scales (UE: DetermineAxisValue).
			float AxisValue = 0.0f;
			if (const FAxisKeyDetails* Details = AxisKeyMap.Find(Binding.AxisName))
			{
				for (const FInputAxisKeyMapping& Mapping : Details->KeyMappings)
				{
					if (IsKeyConsumed(Mapping.Key))
					{
						continue;
					}
					if (!bGamePaused || Binding.bExecuteWhenPaused != 0)
					{
						AxisValue += GetKeyValue(Mapping.Key) * Mapping.Scale;
					}
					if (Binding.bConsumeInput != 0)
					{
						KeysToConsume.AddUnique(Mapping.Key);
					}
				}
			}
			Binding.AxisValue = AxisValue;
			if (Binding.AxisDelegate.IsBound())
			{
				AxisDelegates.Emplace(Binding.AxisDelegate, AxisValue);
			}
		}
		if (Component->bBlockInput != 0)
		{
			--StackIndex;
			break;
		}
		// Consumed after the whole component, so its other bindings still see the keys (UE).
		for (const FKey& Key : KeysToConsume)
		{
			ConsumeKey(Key);
		}
		KeysToConsume.Reset();
	}
	// Below a blocking component the axes read 0 (UE).
	for (; StackIndex >= 0; --StackIndex)
	{
		if (UInputComponent* Component = InputComponentStack[StackIndex])
		{
			Component->ClearBindingValues();
		}
	}

	// The actions in the order their events came, then the axes (UE).
	StableSort(ActionDelegates.GetData(), ActionDelegates.Num(),
		[](const FDelegateDispatchDetails& A, const FDelegateDispatchDetails& B)
		{ return A.EventIndex == B.EventIndex ? A.FoundIndex < B.FoundIndex : A.EventIndex < B.EventIndex; });
	for (const FDelegateDispatchDetails& Dispatch : ActionDelegates)
	{
		(void)Dispatch.Delegate.ExecuteIfBound();
	}
	for (const TPair<FInputAxisHandlerSignature, float>& Axis : AxisDelegates)
	{
		(void)Axis.Key.ExecuteIfBound(Axis.Value);
	}

	// The frame is done (UE: FinishProcessingPlayerInput).
	for (auto& Pair : KeyStateMap)
	{
		Pair.Value.bDownPrevious = Pair.Value.bDown;
		Pair.Value.bConsumed = 0;
	}
}

void UPlayerInput::FlushPressedKeys()
{
	for (auto& Pair : KeyStateMap)
	{
		FKeyState& KeyState = Pair.Value;
		KeyState = FKeyState();
	}
}
