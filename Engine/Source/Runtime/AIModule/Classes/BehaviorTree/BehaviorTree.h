#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


/// Minimal Behavior Tree (Unreal BT lite): composites + leaf tasks over a string blackboard.
enum class EBTNodeResult : std::uint8_t {
    Succeeded = 0,
    Failed = 1,
    Running = 2,
};

class UBlackboardComponent {
public:
    void SetBool(const std::string& key, bool value) { bools_[key] = value; }
    void SetFloat(const std::string& key, float value) { floats_[key] = value; }
    void SetInt(const std::string& key, int value) { ints_[key] = value; }

    [[nodiscard]] bool GetBool(const std::string& key, bool fallback = false) const {
        const auto it = bools_.find(key);
        return it != bools_.end() ? it->second : fallback;
    }
    [[nodiscard]] float GetFloat(const std::string& key, float fallback = 0.0f) const {
        const auto it = floats_.find(key);
        return it != floats_.end() ? it->second : fallback;
    }
    [[nodiscard]] int GetInt(const std::string& key, int fallback = 0) const {
        const auto it = ints_.find(key);
        return it != ints_.end() ? it->second : fallback;
    }

    void Clear() {
        bools_.clear();
        floats_.clear();
        ints_.clear();
    }

private:
    std::unordered_map<std::string, bool> bools_;
    std::unordered_map<std::string, float> floats_;
    std::unordered_map<std::string, int> ints_;
};

struct UBTNode {
    virtual ~UBTNode() = default;
    virtual EBTNodeResult Tick(UBlackboardComponent& board, float deltaTime) = 0;
};

/// Run children in order until one fails (Unreal Sequence).
class UBTComposite_Sequence final : public UBTNode {
public:
    explicit UBTComposite_Sequence(std::vector<UBTNode*> children) : children_(std::move(children)) {}
    EBTNodeResult Tick(UBlackboardComponent& board, float deltaTime) override {
        for (UBTNode* child : children_) {
            if (child == nullptr) {
                return EBTNodeResult::Failed;
            }
            const EBTNodeResult r = child->Tick(board, deltaTime);
            if (r != EBTNodeResult::Succeeded) {
                return r;
            }
        }
        return EBTNodeResult::Succeeded;
    }

private:
    std::vector<UBTNode*> children_;
};

/// Run children until one succeeds (Unreal Selector).
class UBTComposite_Selector final : public UBTNode {
public:
    explicit UBTComposite_Selector(std::vector<UBTNode*> children) : children_(std::move(children)) {}
    EBTNodeResult Tick(UBlackboardComponent& board, float deltaTime) override {
        for (UBTNode* child : children_) {
            if (child == nullptr) {
                continue;
            }
            const EBTNodeResult r = child->Tick(board, deltaTime);
            if (r != EBTNodeResult::Failed) {
                return r;
            }
        }
        return EBTNodeResult::Failed;
    }

private:
    std::vector<UBTNode*> children_;
};

/// Leaf: succeed when blackboard bool is true.
class UBTDecorator_Bool final : public UBTNode {
public:
    UBTDecorator_Bool(std::string key, bool expected = true)
        : key_(std::move(key)), expected_(expected) {}
    EBTNodeResult Tick(UBlackboardComponent& board, float /*deltaTime*/) override {
        return board.GetBool(key_) == expected_ ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    }

private:
    std::string key_;
    bool expected_ = true;
};

/// Leaf: invoke a callback (Succeeded/Failed/Running).
class UBTTask_Action final : public UBTNode {
public:
    using FTaskFunction = std::function<EBTNodeResult(UBlackboardComponent&, float)>;
    explicit UBTTask_Action(FTaskFunction fn) : fn_(std::move(fn)) {}
    EBTNodeResult Tick(UBlackboardComponent& board, float deltaTime) override {
        return fn_ ? fn_(board, deltaTime) : EBTNodeResult::Failed;
    }

private:
    FTaskFunction fn_;
};

/// Owns a root node pointer (non-owning children — caller owns node storage).
class UBehaviorTree {
public:
    void SetRoot(UBTNode* root) { root_ = root; }
    [[nodiscard]] UBTNode* GetRoot() const { return root_; }
    [[nodiscard]] UBlackboardComponent& GetBlackboard() { return board_; }
    [[nodiscard]] const UBlackboardComponent& GetBlackboard() const { return board_; }

    EBTNodeResult Tick(float deltaTime) {
        if (root_ == nullptr) {
            return EBTNodeResult::Failed;
        }
        return root_->Tick(board_, deltaTime);
    }

private:
    UBTNode* root_ = nullptr;
    UBlackboardComponent board_{};
};

