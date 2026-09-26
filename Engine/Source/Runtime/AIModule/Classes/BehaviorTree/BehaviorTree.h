#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/WeakObjectPtr.h"

/** Minimal Behavior Tree (Unreal BT lite): composites and leaf tasks over a typed blackboard. */
enum class EBTNodeResult : uint8
{
	Succeeded = 0,
	Failed = 1,
	Running = 2,
};

/** The type a blackboard key holds (UE: the UBlackboardKeyType classes). */
enum class EBlackboardKeyType : uint8
{
	None,
	Bool,
	Int,
	Float,
	Vector,
	Name,
	Object,
};

/**
 * The memory of a behavior (UE: UBlackboardComponent): values under FName keys, each key typed by its first value
 * (UE types the keys in a UBlackboardData asset; Leon's keys take the type they are first set with). A get of the
 * wrong type or of an unset key returns the default; a set of another type is refused. Objects are held weakly (the
 * blackboard is not a UObject: a destroyed object reads as null).
 */
class AIMODULE_API UBlackboardComponent
{
public:
	void SetValueAsBool(FName KeyName, bool bValue);
	void SetValueAsInt(FName KeyName, int32 Value);
	void SetValueAsFloat(FName KeyName, float Value);
	void SetValueAsVector(FName KeyName, const FVector& Value);
	void SetValueAsName(FName KeyName, FName Value);
	void SetValueAsObject(FName KeyName, UObject* Value);

	[[nodiscard]] bool GetValueAsBool(FName KeyName) const;
	[[nodiscard]] int32 GetValueAsInt(FName KeyName) const;
	[[nodiscard]] float GetValueAsFloat(FName KeyName) const;
	[[nodiscard]] FVector GetValueAsVector(FName KeyName) const;
	[[nodiscard]] FName GetValueAsName(FName KeyName) const;
	[[nodiscard]] UObject* GetValueAsObject(FName KeyName) const;
	template <class T>
	[[nodiscard]] T* GetValueAsObject(FName KeyName) const
	{
		return Cast<T>(GetValueAsObject(KeyName));
	}

	/** The key's type (None: never set). */
	[[nodiscard]] EBlackboardKeyType GetKeyType(FName KeyName) const;
	/** The key holds a value (UE: IsVectorValueSet and the like; an object key set to a live object). */
	[[nodiscard]] bool IsValueSet(FName KeyName) const;
	/** Forgets a key's value; the key keeps its type (UE: ClearValue). */
	void ClearValue(FName KeyName);
	/** Forgets every key. */
	void Clear()
	{
		Entries.Empty();
	}

private:
	struct FEntry
	{
		EBlackboardKeyType Type = EBlackboardKeyType::None;
		bool bSet = false;
		bool bValue = false;
		int32 IntValue = 0;
		float FloatValue = 0.0f;
		FVector VectorValue = FVector::ZeroVector;
		FName NameValue;
		TWeakObjectPtr<UObject> ObjectValue;
	};

	/** The entry to write a value of Type into, or null when the key has another type. */
	FEntry* FindForWrite(FName KeyName, EBlackboardKeyType Type);
	/** The set entry of Type, or null. */
	[[nodiscard]] const FEntry* FindForRead(FName KeyName, EBlackboardKeyType Type) const;

	TMap<FName, FEntry> Entries;
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

/** Leaf: succeed when a blackboard bool is as expected (UE: a Blackboard decorator on a bool key). */
class AIMODULE_API UBTDecorator_Bool final : public UBTNode
{
public:
	UBTDecorator_Bool(FName InKey, bool bInExpected = true)
		: Key(InKey)
		, bExpected(bInExpected)
	{
	}
	EBTNodeResult Tick(UBlackboardComponent& InBoard, float /*deltaTime*/) override
	{
		return InBoard.GetValueAsBool(Key) == bExpected ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}

private:
	FName Key;
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
