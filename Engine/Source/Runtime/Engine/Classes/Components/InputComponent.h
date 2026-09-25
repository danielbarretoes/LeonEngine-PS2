#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "InputComponent.generated.h"

/** An action binding's handler (UE: FInputActionHandlerSignature). */
DECLARE_DELEGATE(FInputActionHandlerSignature);
/** An axis binding's handler, with the axis value (UE: FInputAxisHandlerSignature). */
DECLARE_DELEGATE_OneParam(FInputAxisHandlerSignature, float);

/** What every binding has (UE: FInputBinding). */
struct FInputBinding
{
	/** The binding takes its keys: components lower on the stack do not see them (UE: bConsumeInput). */
	uint8 bConsumeInput : 1;
	/** Runs while the game is paused (UE: bExecuteWhenPaused). */
	uint8 bExecuteWhenPaused : 1;

	FInputBinding()
		: bConsumeInput(true)
		, bExecuteWhenPaused(false)
	{
	}
};

/** An action name and event bound to a handler (UE: FInputActionBinding). */
struct FInputActionBinding : public FInputBinding
{
	/** The event that runs the handler (UE: KeyEvent). */
	EInputEvent KeyEvent = IE_Pressed;
	FInputActionHandlerSignature ActionDelegate;

	FInputActionBinding() = default;
	FInputActionBinding(const FName InActionName, const EInputEvent InKeyEvent)
		: KeyEvent(InKeyEvent)
		, ActionName(InActionName)
	{
	}

	[[nodiscard]] FName GetActionName() const
	{
		return ActionName;
	}

private:
	/** UE: ActionName. */
	FName ActionName;
};

/** An axis name bound to a handler; AxisValue is its last value (UE: FInputAxisBinding). */
struct FInputAxisBinding : public FInputBinding
{
	/** UE: AxisName. */
	FName AxisName;
	FInputAxisHandlerSignature AxisDelegate;
	/** The value of the last ProcessInputStack (UE: AxisValue). */
	float AxisValue = 0.0f;

	FInputAxisBinding() = default;
	explicit FInputAxisBinding(const FName InAxisName)
		: AxisName(InAxisName)
	{
	}
};

/**
 * Input bindings of an actor (UE: UInputComponent, the 4.27 API): action names and events, and axis names, bound to
 * member functions (BindAction, BindAxis). A player controller processes the components of its input stack
 * (APlayerController::BuildInputStack): its own, the possessed pawn's (made in APawn::PawnClientRestart, filled by
 * SetupPlayerInputComponent) and any pushed one; UPlayerInput::ProcessInputStack runs the bound functions.
 */
UCLASS(Transient, Config = Input)
class ENGINE_API UInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInputComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Higher priorities are processed first when pushed on a controller's stack (UE: Priority). */
	int32 Priority = 0;

	/** Takes every key: components below it on the stack get no input (UE: bBlockInput). */
	uint8 bBlockInput : 1;

	/** The axis bindings (UE: AxisBindings). */
	TArray<FInputAxisBinding> AxisBindings;

	/** Binds an action and an event to a member function (UE: BindAction). */
	template <class UserClass>
	FInputActionBinding& BindAction(const FName ActionName, const EInputEvent KeyEvent, UserClass* Object,
		typename TMemFunPtrType<false, UserClass, void()>::Type Func)
	{
		FInputActionBinding Binding(ActionName, KeyEvent);
		Binding.ActionDelegate.BindUObject(Object, Func);
		return AddActionBinding(MoveTemp(Binding));
	}

	/** Binds an axis to a member function that takes its value each frame (UE: BindAxis). */
	template <class UserClass>
	FInputAxisBinding& BindAxis(
		const FName AxisName, UserClass* Object, typename TMemFunPtrType<false, UserClass, void(float)>::Type Func)
	{
		FInputAxisBinding Binding(AxisName);
		Binding.AxisDelegate.BindUObject(Object, Func);
		AxisBindings.Add(MoveTemp(Binding));
		return AxisBindings.Last();
	}

	/** Binds an axis without a handler, to read with GetAxisValue (UE: BindAxis). */
	FInputAxisBinding& BindAxis(const FName AxisName);

	/** Adds an action binding (UE: AddActionBinding). */
	FInputActionBinding& AddActionBinding(FInputActionBinding Binding);

	/** The last value of the first binding of AxisName, 0 when unbound (UE: GetAxisValue). */
	[[nodiscard]] float GetAxisValue(const FName AxisName) const;

	[[nodiscard]] int32 GetNumActionBindings() const
	{
		return ActionBindings.Num();
	}
	[[nodiscard]] FInputActionBinding& GetActionBinding(const int32 BindingIndex)
	{
		return ActionBindings[BindingIndex];
	}

	/** Removes every action binding (UE: ClearActionBindings). */
	void ClearActionBindings();
	/** Zeroes the axis values (UE: ClearBindingValues). */
	void ClearBindingValues();
	/** Whether anything is bound (UE: HasBindings). */
	[[nodiscard]] bool HasBindings() const;

private:
	/** The action bindings (UE: ActionBindings). */
	TArray<FInputActionBinding> ActionBindings;
};
