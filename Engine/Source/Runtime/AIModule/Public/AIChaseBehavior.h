#pragma once

#include <glm/vec3.hpp>

#include "AIController.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BehaviorTree.h"
#include <memory>
#include <vector>


/// Shared chase UBehaviorTree for pack AI (HasTarget → MoveToActor, else Stop).
/// One instance is safe to reuse serially across pawns in a Tick loop.
class AIMODULE_API FAIChaseBehavior {
public:
    FAIChaseBehavior() {
        HasTarget = std::make_unique<UBTDecorator_Bool>("HasTarget", true);
        Chase = std::make_unique<UBTTask_Action>([this](UBlackboardComponent&, float) {
            if (Ai == nullptr || Target == nullptr) {
                return EBTNodeResult::Failed;
            }
            Ai->MoveToActor(Target);
            return EBTNodeResult::Succeeded;
        });
        Stop = std::make_unique<UBTTask_Action>([this](UBlackboardComponent&, float) {
            if (Ai != nullptr) {
                Ai->StopMovement();
            }
            return EBTNodeResult::Succeeded;
        });
        ChaseSeq = std::make_unique<UBTComposite_Sequence>(
            std::vector<UBTNode*>{HasTarget.get(), Chase.get()});
        Root = std::make_unique<UBTComposite_Selector>(
            std::vector<UBTNode*>{ChaseSeq.get(), Stop.get()});
        Tree.SetRoot(Root.get());
    }

    /// Runs BT then `AAIController::TickAI`. Returns steering wish.
    glm::vec3 Tick(AAIController& InAi, AActor* InTarget, float DeltaTime) {
        Ai = &InAi;
        Target = InTarget;
        Tree.GetBlackboard().SetBool("HasTarget", InTarget != nullptr);
        (void)Tree.Tick(DeltaTime);
        return InAi.TickAI(DeltaTime);
    }

    [[nodiscard]] UBehaviorTree& GetTree() { return Tree; }
    [[nodiscard]] const UBehaviorTree& GetTree() const { return Tree; }

private:
    AAIController* Ai = nullptr;
    AActor* Target = nullptr;
    std::unique_ptr<UBTDecorator_Bool> HasTarget;
    std::unique_ptr<UBTTask_Action> Chase;
    std::unique_ptr<UBTTask_Action> Stop;
    std::unique_ptr<UBTComposite_Sequence> ChaseSeq;
    std::unique_ptr<UBTComposite_Selector> Root;
    UBehaviorTree Tree{};
};

