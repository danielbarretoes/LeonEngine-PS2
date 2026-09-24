#pragma once

#include <glm/vec3.hpp>

#include "AIController.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BehaviorTree.h"
#include <memory>
#include <vector>


/// Shared chase UBehaviorTree for pack AI (HasTarget → MoveToActor, else Stop).
/// One instance is safe to reuse serially across pawns in a Tick loop.
class FAIChaseBehavior {
public:
    FAIChaseBehavior() {
        hasTarget_ = std::make_unique<UBTDecorator_Bool>("HasTarget", true);
        chase_ = std::make_unique<UBTTask_Action>([this](UBlackboardComponent&, float) {
            if (ai_ == nullptr || target_ == nullptr) {
                return EBTNodeResult::Failed;
            }
            ai_->MoveToActor(target_);
            return EBTNodeResult::Succeeded;
        });
        stop_ = std::make_unique<UBTTask_Action>([this](UBlackboardComponent&, float) {
            if (ai_ != nullptr) {
                ai_->StopMovement();
            }
            return EBTNodeResult::Succeeded;
        });
        chaseSeq_ = std::make_unique<UBTComposite_Sequence>(
            std::vector<UBTNode*>{hasTarget_.get(), chase_.get()});
        root_ = std::make_unique<UBTComposite_Selector>(
            std::vector<UBTNode*>{chaseSeq_.get(), stop_.get()});
        tree_.SetRoot(root_.get());
    }

    /// Runs BT then `AAIController::TickAI`. Returns steering wish.
    glm::vec3 Tick(AAIController& ai, AActor* target, float deltaTime) {
        ai_ = &ai;
        target_ = target;
        tree_.GetBlackboard().SetBool("HasTarget", target != nullptr);
        (void)tree_.Tick(deltaTime);
        return ai.TickAI(deltaTime);
    }

    [[nodiscard]] UBehaviorTree& GetTree() { return tree_; }
    [[nodiscard]] const UBehaviorTree& GetTree() const { return tree_; }

private:
    AAIController* ai_ = nullptr;
    AActor* target_ = nullptr;
    std::unique_ptr<UBTDecorator_Bool> hasTarget_;
    std::unique_ptr<UBTTask_Action> chase_;
    std::unique_ptr<UBTTask_Action> stop_;
    std::unique_ptr<UBTComposite_Sequence> chaseSeq_;
    std::unique_ptr<UBTComposite_Selector> root_;
    UBehaviorTree tree_{};
};

