#include "Components/InputComponent.h"

UInputComponent::UInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bBlockInput(false)
{
}

FInputAxisBinding& UInputComponent::BindAxis(const FName AxisName)
{
	AxisBindings.Add(FInputAxisBinding(AxisName));
	return AxisBindings.Last();
}

FInputActionBinding& UInputComponent::AddActionBinding(FInputActionBinding Binding)
{
	ActionBindings.Add(MoveTemp(Binding));
	return ActionBindings.Last();
}

float UInputComponent::GetAxisValue(const FName AxisName) const
{
	for (const FInputAxisBinding& Binding : AxisBindings)
	{
		if (Binding.AxisName == AxisName)
		{
			return Binding.AxisValue;
		}
	}
	return 0.0f;
}

void UInputComponent::ClearActionBindings()
{
	ActionBindings.Reset();
}

void UInputComponent::ClearBindingValues()
{
	for (FInputAxisBinding& Binding : AxisBindings)
	{
		Binding.AxisValue = 0.0f;
	}
}

bool UInputComponent::HasBindings() const
{
	return ActionBindings.Num() > 0 || AxisBindings.Num() > 0;
}
