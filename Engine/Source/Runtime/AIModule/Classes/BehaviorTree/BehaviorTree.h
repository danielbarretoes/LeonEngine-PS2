#pragma once

#include "CoreMinimal.h"

/** Minimal Behavior Tree (Unreal BT lite): composites + leaf tasks over a string blackboard. */
enum class EBTNodeResult : uint8
{
	Succeeded = 0,
	Failed = 1,
	Running = 2,
};

class AIMODULE_API UBlackboardComponent
{
public:
	void SetBool(const FString& InKey, bool bValue)
	{
		Bools.Add(InKey, bValue);
	}
	void SetFloat(const FString& InKey, float Value)
	{
		Floats.Add(InKey, Value);
	}
	void SetInt(const FString& InKey, int32 Value)
	{
		Ints.Add(InKey, Value);
	}

	[[nodiscard]] bool GetBool(const FString& InKey, bool bFallback = false) const
	{
		const bool* Value = Bools.Find(InKey);
		return Value != nullptr ? *Value : bFallback;
	}
	[[nodiscard]] float GetFloat(const FString& InKey, float Fallback = 0.0f) const
	{
		const float* Value = Floats.Find(InKey);
		return Value != nullptr ? *Value : Fallback;
	}
	[[nodiscard]] int32 GetInt(const FString& InKey, int32 Fallback = 0) const
	{
		const int32* Value = Ints.Find(InKey);
		return Value != nullptr ? *Value : Fallback;
	}

	void Clear()
	{
		Bools.Empty();
		Floats.Empty();
		Ints.Empty();
	}

private:
	TMap<FString, bool> Bools;
	TMap<FString, float> Floats;
	TMap<FString, int32> Ints;
};

struct AIMODULE_API UBTNode
{
	virtual ~UBTNode() = default;
	virtual EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) = 0;
};

/** Run children in order until one fails (Unreal Sequence). */
class AIMODULE_API UBTComposite_Sequence final : public UBTNode
{
public:
	explicit UBTComposite_Sequence(TArray<UBTNode*> InChildren)
		: Children(MoveTemp(InChildren))
	{
	}
	EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) override
	{
		for (UBTNode* Child : Children)
		{
			if (Child == nullptr)
			{
				return EBTNodeResult::Failed;
			}
			const EBTNodeResult R = Child->Tick(InBoard, DeltaTime);
			if (R != EBTNodeResult::Succeeded)
			{
				return R;
			}
		}
		return EBTNodeResult::Succeeded;
	}

private:
	TArray<UBTNode*> Children;
};

/** Run children until one succeeds (Unreal Selector). */
class AIMODULE_API UBTComposite_Selector final : public UBTNode
{
public:
	explicit UBTComposite_Selector(TArray<UBTNode*> InChildren)
		: Children(MoveTemp(InChildren))
	{
	}
	EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) override
	{
		for (UBTNode* Child : Children)
		{
			if (Child == nullptr)
			{
				continue;
			}
			const EBTNodeResult R = Child->Tick(InBoard, DeltaTime);
			if (R != EBTNodeResult::Failed)
			{
				return R;
			}
		}
		return EBTNodeResult::Failed;
	}

private:
	TArray<UBTNode*> Children;
};

/** Leaf: succeed when blackboard bool is true. */
class AIMODULE_API UBTDecorator_Bool final : public UBTNode
{
public:
	UBTDecorator_Bool(FString InKey, bool bInExpected = true)
		: Key(MoveTemp(InKey))
		, bExpected(bInExpected)
	{
	}
	EBTNodeResult Tick(UBlackboardComponent& InBoard, float /*deltaTime*/) override
	{
		return InBoard.GetBool(Key) == bExpected ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}

private:
	FString Key;
	bool bExpected = true;
};

/** Leaf: invoke a callback (Succeeded/Failed/Running). */
class AIMODULE_API UBTTask_Action final : public UBTNode
{
public:
	using FTaskFunction = TFunction<EBTNodeResult(UBlackboardComponent&, float)>;
	explicit UBTTask_Action(FTaskFunction InFn)
		: Fn(MoveTemp(InFn))
	{
	}
	EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) override
	{
		return Fn ? Fn(InBoard, DeltaTime) : EBTNodeResult::Failed;
	}

private:
	FTaskFunction Fn;
};

/** Owns a root node pointer (non-owning children — caller owns node storage). */
class AIMODULE_API UBehaviorTree
{
public:
	void SetRoot(UBTNode* InRoot)
	{
		Root = InRoot;
	}
	[[nodiscard]] UBTNode* GetRoot() const
	{
		return Root;
	}
	[[nodiscard]] UBlackboardComponent& GetBlackboard()
	{
		return Board;
	}
	[[nodiscard]] const UBlackboardComponent& GetBlackboard() const
	{
		return Board;
	}

	EBTNodeResult Tick(float DeltaTime)
	{
		if (Root == nullptr)
		{
			return EBTNodeResult::Failed;
		}
		return Root->Tick(Board, DeltaTime);
	}

private:
	UBTNode* Root = nullptr;
	UBlackboardComponent Board{};
};
