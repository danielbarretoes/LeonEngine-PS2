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

class Blackboard {
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

struct BTNode {
    virtual ~BTNode() = default;
    virtual EBTNodeResult Tick(Blackboard& board, float deltaTime) = 0;
};

/// Run children in order until one fails (Unreal Sequence).
class BTSequence final : public BTNode {
public:
    explicit BTSequence(std::vector<BTNode*> children) : children_(std::move(children)) {}
    EBTNodeResult Tick(Blackboard& board, float deltaTime) override {
        for (BTNode* child : children_) {
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
    std::vector<BTNode*> children_;
};

/// Run children until one succeeds (Unreal Selector).
class BTSelector final : public BTNode {
public:
    explicit BTSelector(std::vector<BTNode*> children) : children_(std::move(children)) {}
    EBTNodeResult Tick(Blackboard& board, float deltaTime) override {
        for (BTNode* child : children_) {
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
    std::vector<BTNode*> children_;
};

/// Leaf: succeed when blackboard bool is true.
class BTConditionBool final : public BTNode {
public:
    BTConditionBool(std::string key, bool expected = true)
        : key_(std::move(key)), expected_(expected) {}
    EBTNodeResult Tick(Blackboard& board, float /*deltaTime*/) override {
        return board.GetBool(key_) == expected_ ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    }

private:
    std::string key_;
    bool expected_ = true;
};

/// Leaf: invoke a callback (Succeeded/Failed/Running).
class BTAction final : public BTNode {
public:
    using Fn = std::function<EBTNodeResult(Blackboard&, float)>;
    explicit BTAction(Fn fn) : fn_(std::move(fn)) {}
    EBTNodeResult Tick(Blackboard& board, float deltaTime) override {
        return fn_ ? fn_(board, deltaTime) : EBTNodeResult::Failed;
    }

private:
    Fn fn_;
};

/// Owns a root node pointer (non-owning children — caller owns node storage).
class BehaviorTree {
public:
    void SetRoot(BTNode* root) { root_ = root; }
    [[nodiscard]] BTNode* GetRoot() const { return root_; }
    [[nodiscard]] Blackboard& GetBlackboard() { return board_; }
    [[nodiscard]] const Blackboard& GetBlackboard() const { return board_; }

    EBTNodeResult Tick(float deltaTime) {
        if (root_ == nullptr) {
            return EBTNodeResult::Failed;
        }
        return root_->Tick(board_, deltaTime);
    }

private:
    BTNode* root_ = nullptr;
    Blackboard board_{};
};

