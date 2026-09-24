#pragma once

#include <glm/vec3.hpp>

#include "AIController.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BehaviorTree.h"
#include <memory>
#include <vector>


/// Shared chase BehaviorTree for pack AI (HasTarget → MoveToActor, else Stop).
/// One instance is safe to reuse serially across pawns in a Tick loop.
class AIChaseBehavior {
public:
    AIChaseBehavior() {
        hasTarget_ = std::make_unique<BTConditionBool>("HasTarget", true);
        chase_ = std::make_unique<BTAction>([this](Blackboard&, float) {
            if (ai_ == nullptr || target_ == nullptr) {
                return EBTNodeResult::Failed;
            }
            ai_->MoveToActor(target_);
            return EBTNodeResult::Succeeded;
        });
        stop_ = std::make_unique<BTAction>([this](Blackboard&, float) {
            if (ai_ != nullptr) {
                ai_->StopMovement();
            }
            return EBTNodeResult::Succeeded;
        });
        chaseSeq_ = std::make_unique<BTSequence>(
            std::vector<BTNode*>{hasTarget_.get(), chase_.get()});
        root_ = std::make_unique<BTSelector>(
            std::vector<BTNode*>{chaseSeq_.get(), stop_.get()});
        tree_.SetRoot(root_.get());
    }

    /// Runs BT then `AIController::TickAI`. Returns steering wish.
    glm::vec3 Tick(AIController& ai, AActor* target, float deltaTime) {
        ai_ = &ai;
        target_ = target;
        tree_.GetBlackboard().SetBool("HasTarget", target != nullptr);
        (void)tree_.Tick(deltaTime);
        return ai.TickAI(deltaTime);
    }

    [[nodiscard]] BehaviorTree& GetTree() { return tree_; }
    [[nodiscard]] const BehaviorTree& GetTree() const { return tree_; }

private:
    AIController* ai_ = nullptr;
    AActor* target_ = nullptr;
    std::unique_ptr<BTConditionBool> hasTarget_;
    std::unique_ptr<BTAction> chase_;
    std::unique_ptr<BTAction> stop_;
    std::unique_ptr<BTSequence> chaseSeq_;
    std::unique_ptr<BTSelector> root_;
    BehaviorTree tree_{};
};

