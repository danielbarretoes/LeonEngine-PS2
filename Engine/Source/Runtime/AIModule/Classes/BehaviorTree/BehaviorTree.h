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
    void SetBool(const std::string& InKey, bool bValue) { Bools[InKey] = bValue; }
    void SetFloat(const std::string& InKey, float Value) { Floats[InKey] = Value; }
    void SetInt(const std::string& InKey, int Value) { Ints[InKey] = Value; }

    [[nodiscard]] bool GetBool(const std::string& InKey, bool bFallback = false) const {
        const auto It = Bools.find(InKey);
        return It != Bools.end() ? It->second : bFallback;
    }
    [[nodiscard]] float GetFloat(const std::string& InKey, float Fallback = 0.0f) const {
        const auto It = Floats.find(InKey);
        return It != Floats.end() ? It->second : Fallback;
    }
    [[nodiscard]] int GetInt(const std::string& InKey, int Fallback = 0) const {
        const auto It = Ints.find(InKey);
        return It != Ints.end() ? It->second : Fallback;
    }

    void Clear() {
        Bools.clear();
        Floats.clear();
        Ints.clear();
    }

private:
    std::unordered_map<std::string, bool> Bools;
    std::unordered_map<std::string, float> Floats;
    std::unordered_map<std::string, int> Ints;
};

struct UBTNode {
    virtual ~UBTNode() = default;
    virtual EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) = 0;
};

/// Run children in order until one fails (Unreal Sequence).
class UBTComposite_Sequence final : public UBTNode {
public:
    explicit UBTComposite_Sequence(std::vector<UBTNode*> InChildren) : Children(std::move(InChildren)) {}
    EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) override {
        for (UBTNode* Child : Children) {
            if (Child == nullptr) {
                return EBTNodeResult::Failed;
            }
            const EBTNodeResult R = Child->Tick(InBoard, DeltaTime);
            if (R != EBTNodeResult::Succeeded) {
                return R;
            }
        }
        return EBTNodeResult::Succeeded;
    }

private:
    std::vector<UBTNode*> Children;
};

/// Run children until one succeeds (Unreal Selector).
class UBTComposite_Selector final : public UBTNode {
public:
    explicit UBTComposite_Selector(std::vector<UBTNode*> InChildren) : Children(std::move(InChildren)) {}
    EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) override {
        for (UBTNode* Child : Children) {
            if (Child == nullptr) {
                continue;
            }
            const EBTNodeResult R = Child->Tick(InBoard, DeltaTime);
            if (R != EBTNodeResult::Failed) {
                return R;
            }
        }
        return EBTNodeResult::Failed;
    }

private:
    std::vector<UBTNode*> Children;
};

/// Leaf: succeed when blackboard bool is true.
class UBTDecorator_Bool final : public UBTNode {
public:
    UBTDecorator_Bool(std::string InKey, bool bInExpected = true)
        : Key(std::move(InKey)), bExpected(bInExpected) {}
    EBTNodeResult Tick(UBlackboardComponent& InBoard, float /*deltaTime*/) override {
        return InBoard.GetBool(Key) == bExpected ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    }

private:
    std::string Key;
    bool bExpected = true;
};

/// Leaf: invoke a callback (Succeeded/Failed/Running).
class UBTTask_Action final : public UBTNode {
public:
    using FTaskFunction = std::function<EBTNodeResult(UBlackboardComponent&, float)>;
    explicit UBTTask_Action(FTaskFunction InFn) : Fn(std::move(InFn)) {}
    EBTNodeResult Tick(UBlackboardComponent& InBoard, float DeltaTime) override {
        return Fn ? Fn(InBoard, DeltaTime) : EBTNodeResult::Failed;
    }

private:
    FTaskFunction Fn;
};

/// Owns a root node pointer (non-owning children — caller owns node storage).
class UBehaviorTree {
public:
    void SetRoot(UBTNode* InRoot) { Root = InRoot; }
    [[nodiscard]] UBTNode* GetRoot() const { return Root; }
    [[nodiscard]] UBlackboardComponent& GetBlackboard() { return Board; }
    [[nodiscard]] const UBlackboardComponent& GetBlackboard() const { return Board; }

    EBTNodeResult Tick(float DeltaTime) {
        if (Root == nullptr) {
            return EBTNodeResult::Failed;
        }
        return Root->Tick(Board, DeltaTime);
    }

private:
    UBTNode* Root = nullptr;
    UBlackboardComponent Board{};
};

