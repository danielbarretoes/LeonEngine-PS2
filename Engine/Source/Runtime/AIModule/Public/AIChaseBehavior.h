#pragma once

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * Shared chase UBehaviorTree for game AI (HasTarget → MoveToActor, else Stop).
 * One instance is safe to reuse serially across pawns in a Tick loop.
 */
class AIMODULE_API FAIChaseBehavior
{
public:
	FAIChaseBehavior()
	{
		HasTarget = MakeUnique<UBTDecorator_Bool>("HasTarget", true);
		Chase = MakeUnique<UBTTask_Action>(
			[this](UBlackboardComponent&, float)
			{
				if (Ai == nullptr || Target == nullptr)
				{
					return EBTNodeResult::Failed;
				}
				Ai->MoveToActor(Target);
				return EBTNodeResult::Succeeded;
			});
		Stop = MakeUnique<UBTTask_Action>(
			[this](UBlackboardComponent&, float)
			{
				if (Ai != nullptr)
				{
					Ai->StopMovement();
				}
				return EBTNodeResult::Succeeded;
			});
		ChaseSeq = MakeUnique<UBTComposite_Sequence>(TArray<UBTNode*>{HasTarget.Get(), Chase.Get()});
		Root = MakeUnique<UBTComposite_Selector>(TArray<UBTNode*>{ChaseSeq.Get(), Stop.Get()});
		Tree.SetRoot(Root.Get());
	}

	/** Runs BT then AAIController::TickAI. Returns steering wish. */
	FVector Tick(AAIController& InAi, AActor* InTarget, float DeltaTime)
	{
		Ai = &InAi;
		Target = InTarget;
		Tree.GetBlackboard().SetValueAsBool(TEXT("HasTarget"), InTarget != nullptr);
		(void)Tree.Tick(DeltaTime);
		return InAi.TickAI(DeltaTime);
	}

	[[nodiscard]] UBehaviorTree& GetTree()
	{
		return Tree;
	}
	[[nodiscard]] const UBehaviorTree& GetTree() const
	{
		return Tree;
	}

private:
	AAIController* Ai = nullptr;
	AActor* Target = nullptr;
	TUniquePtr<UBTDecorator_Bool> HasTarget;
	TUniquePtr<UBTTask_Action> Chase;
	TUniquePtr<UBTTask_Action> Stop;
	TUniquePtr<UBTComposite_Sequence> ChaseSeq;
	TUniquePtr<UBTComposite_Selector> Root;
	UBehaviorTree Tree{};
};
