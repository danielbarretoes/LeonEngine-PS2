#include "GameFramework/InputMapping.h"

#include "GameFramework/InputActions.h"
#include "InputCoreTypes.h"

#include <algorithm>

void UInputMappingContext::BindAxisKey(std::string_view Action, int InKey, float InScale)
{
	if (Action.empty() || InKey == 0)
	{
		return;
	}
	Axes[std::string(Action)].push_back(FInputAxisKeyMapping{InKey, InScale});
}

void UInputMappingContext::BindActionKey(std::string_view Action, int InKey)
{
	if (Action.empty() || InKey == 0)
	{
		return;
	}
	Actions[std::string(Action)].push_back(InKey);
}

void UInputMappingContext::BindAxisKey(std::string_view Action, EKeys InKey, float InScale)
{
	BindAxisKey(Action, ToKeyCode(InKey), InScale);
}

void UInputMappingContext::BindActionKey(std::string_view Action, EKeys InKey)
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
	Contexts.clear();
	bMapsDirty = true;
}

void UPlayerInput::AddMappingContext(UInputMappingContext InContext, int InPriority)
{
	Contexts.push_back(FContextEntry{.Priority = InPriority, .Context = std::move(InContext)});
	std::stable_sort(Contexts.begin(), Contexts.end(),
		[](const FContextEntry& A, const FContextEntry& B) { return A.Priority < B.Priority; });
	bMapsDirty = true;
}

void UPlayerInput::RebuildEffectiveMaps()
{
	EffectiveAxes.clear();
	EffectiveActions.clear();
	for (const FContextEntry& Entry : Contexts)
	{
		for (const auto& [name, keys] : Entry.Context.GetAxes())
		{
			auto& Dst = EffectiveAxes[name];
			Dst.insert(Dst.end(), keys.begin(), keys.end());
		}
		for (const auto& [name, keys] : Entry.Context.GetActions())
		{
			auto& Dst = EffectiveActions[name];
			Dst.insert(Dst.end(), keys.begin(), keys.end());
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
	AxisValues.clear();
	ActionPressed.clear();

	for (const auto& [name, keys] : EffectiveAxes)
	{
		float Value = 0.0f;
		for (const FInputAxisKeyMapping& Binding : keys)
		{
			if (Window.IsKeyPressed(static_cast<EKeys>(Binding.Key)))
			{
				Value += Binding.Scale;
			}
		}
		Value = std::clamp(Value, -1.0f, 1.0f);
		AxisValues[name] = Value;
	}

	for (const auto& [name, keys] : EffectiveActions)
	{
		bool bPressed = false;
		for (int LocalKey : keys)
		{
			if (Window.IsKeyPressed(static_cast<EKeys>(LocalKey)))
			{
				bPressed = true;
				break;
			}
		}
		ActionPressed[name] = bPressed;
	}
}

float UPlayerInput::GetAxisValue(std::string_view Action) const
{
	const auto It = AxisValues.find(std::string(Action));
	return It != AxisValues.end() ? It->second : 0.0f;
}

bool UPlayerInput::IsActionPressed(std::string_view Action) const
{
	const auto It = ActionPressed.find(std::string(Action));
	return It != ActionPressed.end() && It->second;
}

bool UPlayerInput::WasActionJustPressed(std::string_view Action) const
{
	const std::string LocalKey(Action);
	const auto Cur = ActionPressed.find(LocalKey);
	const bool bNow = Cur != ActionPressed.end() && Cur->second;
	if (!bNow)
	{
		return false;
	}
	const auto Prev = ActionPressedPrev.find(LocalKey);
	const bool bWas = Prev != ActionPressedPrev.end() && Prev->second;
	return !bWas;
}

bool UPlayerInput::WasActionJustReleased(std::string_view Action) const
{
	const std::string LocalKey(Action);
	const auto Cur = ActionPressed.find(LocalKey);
	const bool bNow = Cur != ActionPressed.end() && Cur->second;
	if (bNow)
	{
		return false;
	}
	const auto Prev = ActionPressedPrev.find(LocalKey);
	return Prev != ActionPressedPrev.end() && Prev->second;
}

FMoveAxes2D UPlayerInput::GetMoveAxes2D() const
{
	FMoveAxes2D LocalAxes{};
	LocalAxes.X = GetAxisValue(Leon::InputActions::MoveRight);
	LocalAxes.Z = GetAxisValue(Leon::InputActions::MoveForward);
	return LocalAxes;
}
