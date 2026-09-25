#include "GameFramework/InputMapping.h"

#include "GameFramework/InputActions.h"
#include "InputCoreTypes.h"

void UInputMappingContext::BindAxisKey(const FName& Action, int32 InKey, float InScale)
{
	if (Action.IsNone() || InKey == 0)
	{
		return;
	}
	Axes.FindOrAdd(Action).Add(FInputAxisKeyMapping{InKey, InScale});
}

void UInputMappingContext::BindActionKey(const FName& Action, int32 InKey)
{
	if (Action.IsNone() || InKey == 0)
	{
		return;
	}
	Actions.FindOrAdd(Action).Add(InKey);
}

void UInputMappingContext::BindAxisKey(const FName& Action, EKeys InKey, float InScale)
{
	BindAxisKey(Action, ToKeyCode(InKey), InScale);
}

void UInputMappingContext::BindActionKey(const FName& Action, EKeys InKey)
{
	BindActionKey(Action, ToKeyCode(InKey));
}

UInputMappingContext UInputMappingContext::MakeDefault()
{
	UInputMappingContext Ctx;
	using namespace Leon::InputActions;

	Ctx.BindAxisKey(MoveForward, EKeys::W, 1.0f);
	Ctx.BindAxisKey(MoveForward, EKeys::Up, 1.0f);
	Ctx.BindAxisKey(MoveForward, EKeys::S, -1.0f);
	Ctx.BindAxisKey(MoveForward, EKeys::Down, -1.0f);

	Ctx.BindAxisKey(MoveRight, EKeys::D, 1.0f);
	Ctx.BindAxisKey(MoveRight, EKeys::Right, 1.0f);
	Ctx.BindAxisKey(MoveRight, EKeys::A, -1.0f);
	Ctx.BindAxisKey(MoveRight, EKeys::Left, -1.0f);

	Ctx.BindAxisKey(MoveUp, EKeys::E, 1.0f);
	Ctx.BindAxisKey(MoveUp, EKeys::Q, -1.0f);

	Ctx.BindActionKey(Jump, EKeys::SpaceBar);
	return Ctx;
}

void UPlayerInput::ClearContexts()
{
	Contexts.Empty();
	bMapsDirty = true;
}

void UPlayerInput::AddMappingContext(UInputMappingContext InContext, int32 InPriority)
{
	FContextEntry& Entry = Contexts.AddDefaulted_GetRef();
	Entry.Priority = InPriority;
	Entry.Context = MoveTemp(InContext);
	Contexts.StableSort([](const FContextEntry& A, const FContextEntry& B) { return A.Priority < B.Priority; });
	bMapsDirty = true;
}

void UPlayerInput::RebuildEffectiveMaps()
{
	EffectiveAxes.Empty();
	EffectiveActions.Empty();
	for (const FContextEntry& Entry : Contexts)
	{
		for (const auto& Pair : Entry.Context.GetAxes())
		{
			EffectiveAxes.FindOrAdd(Pair.Key).Append(Pair.Value);
		}
		for (const auto& Pair : Entry.Context.GetActions())
		{
			EffectiveActions.FindOrAdd(Pair.Key).Append(Pair.Value);
		}
	}
	bMapsDirty = false;
}

void UPlayerInput::Update(const FGenericWindow& Window)
{
	if (bMapsDirty)
	{
		RebuildEffectiveMaps();
	}

	ActionPressedPrev = ActionPressed;
	AxisValues.Reset();
	ActionPressed.Reset();

	for (const auto& Pair : EffectiveAxes)
	{
		float Value = 0.0f;
		for (const FInputAxisKeyMapping& Binding : Pair.Value)
		{
			if (Window.IsKeyPressed(static_cast<EKeys>(Binding.Key)))
			{
				Value += Binding.Scale;
			}
		}
		Value = FMath::Clamp(Value, -1.0f, 1.0f);
		AxisValues.Add(Pair.Key, Value);
	}

	for (const auto& Pair : EffectiveActions)
	{
		bool bPressed = false;
		for (int32 LocalKey : Pair.Value)
		{
			if (Window.IsKeyPressed(static_cast<EKeys>(LocalKey)))
			{
				bPressed = true;
				break;
			}
		}
		ActionPressed.Add(Pair.Key, bPressed);
	}
}

float UPlayerInput::GetAxisValue(const FName& Action) const
{
	const float* Value = AxisValues.Find(Action);
	return Value != nullptr ? *Value : 0.0f;
}

bool UPlayerInput::IsActionPressed(const FName& Action) const
{
	const bool* bPressed = ActionPressed.Find(Action);
	return bPressed != nullptr && *bPressed;
}

bool UPlayerInput::WasActionJustPressed(const FName& Action) const
{
	if (!IsActionPressed(Action))
	{
		return false;
	}
	const bool* bWas = ActionPressedPrev.Find(Action);
	return bWas == nullptr || !*bWas;
}

bool UPlayerInput::WasActionJustReleased(const FName& Action) const
{
	if (IsActionPressed(Action))
	{
		return false;
	}
	const bool* bWas = ActionPressedPrev.Find(Action);
	return bWas != nullptr && *bWas;
}

FMoveAxes2D UPlayerInput::GetMoveAxes2D() const
{
	FMoveAxes2D LocalAxes{};
	LocalAxes.X = GetAxisValue(Leon::InputActions::MoveRight);
	LocalAxes.Z = GetAxisValue(Leon::InputActions::MoveForward);
	return LocalAxes;
}
