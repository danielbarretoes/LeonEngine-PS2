#include <iostream>
#include "IPhysicsBackend.h"

namespace {

class FArcadePhysicsBackend final : public IPhysicsBackend {
public:
    [[nodiscard]] const char* GetName() const override { return "Arcade"; }
};

FPhysicsBackendFactory& JoltFactory() {
    static FPhysicsBackendFactory factory = nullptr;
    return factory;
}

} // namespace

void RegisterPhysicsBackendFactory(EPhysicsBackendKind kind, FPhysicsBackendFactory factory) {
    if (kind == EPhysicsBackendKind::Jolt) {
        JoltFactory() = factory;
    }
}

std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(EPhysicsBackendKind kind) {
    if (kind == EPhysicsBackendKind::Jolt) {
        if (JoltFactory() != nullptr) {
            return JoltFactory()();
        }
        static bool s_loggedJoltFallback = false;
        if (!s_loggedJoltFallback) {
            s_loggedJoltFallback = true;
            std::cerr << "CreatePhysicsBackend: JoltPhysics plugin not enabled; falling back to Arcade\n";
        }
    }
    return std::make_unique<FArcadePhysicsBackend>();
}

