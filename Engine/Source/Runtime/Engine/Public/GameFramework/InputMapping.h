#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"

/** One key contribution to a 1D axis (Unreal-like axis mapping entry). */
struct ENGINE_API FInputAxisKeyMapping
{
	int32 Key = 0; // EKeys underlying code (desktop key codes match GLFW)
	float Scale = 1.0f; // typically +1 or -1
};

/** Maps action names to keys (Unreal-like Input Mapping Context). */
class ENGINE_API UInputMappingContext
{
public:
	/** Bind a key that contributes scale to a named axis while held. */
	void BindAxisKey(const FName& Action, int32 InKey, float InScale = 1.0f);
	void BindAxisKey(const FName& Action, EKeys InKey, float InScale = 1.0f);

	/** Bind a digital action key (pressed / just-pressed queries). */
	void BindActionKey(const FName& Action, int32 InKey);
	void BindActionKey(const FName& Action, EKeys InKey);

	[[nodiscard]] const TMap<FName, TArray<FInputAxisKeyMapping>>& GetAxes() const
	{
		return Axes;
	}
	[[nodiscard]] const TMap<FName, TArray<int32>>& GetActions() const
	{
		return Actions;
	}

	/** Default Leon gameplay map: WASD+arrows move, Q/E up, Space jump. */
	[[nodiscard]] static UInputMappingContext MakeDefault();

private:
	TMap<FName, TArray<FInputAxisKeyMapping>> Axes;
	TMap<FName, TArray<int32>> Actions;
};

/** Samples mapped input once per frame (Unreal-like UPlayerInput). */
class ENGINE_API UPlayerInput
{
public:
	void ClearContexts();
	/** Higher priority is merged later (same key can appear in multiple contexts). */
	void AddMappingContext(UInputMappingContext InContext, int32 InPriority = 0);

	/** Rebuild effective binds + sample Window state. Call once per frame after pollEvents. */
	void Update(const FGenericWindow& Window);

	[[nodiscard]] float GetAxisValue(const FName& Action) const;
	[[nodiscard]] bool IsActionPressed(const FName& Action) const;
	[[nodiscard]] bool WasActionJustPressed(const FName& Action) const;
	[[nodiscard]] bool WasActionJustReleased(const FName& Action) const;

	/** The move axes as one input: X = MoveForward, Y = MoveRight (UE's forward and right axes). */
	[[nodiscard]] FVector2D GetMoveInput() const;

private:
	struct FContextEntry
	{
		int32 Priority = 0;
		UInputMappingContext Context;
	};

	void RebuildEffectiveMaps();

	TArray<FContextEntry> Contexts;
	TMap<FName, TArray<FInputAxisKeyMapping>> EffectiveAxes;
	TMap<FName, TArray<int32>> EffectiveActions;

	TMap<FName, float> AxisValues;
	TMap<FName, bool> ActionPressed;
	TMap<FName, bool> ActionPressedPrev;
	bool bMapsDirty = true;
};
