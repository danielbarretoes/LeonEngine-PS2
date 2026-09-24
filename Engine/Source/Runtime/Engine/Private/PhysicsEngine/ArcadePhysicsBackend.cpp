#include <iostream>
#include "IPhysicsBackend.h"

namespace {

class ArcadePhysicsBackend final : public IPhysicsBackend {
public:
    [[nodiscard]] const char* GetName() const override { return "Arcade"; }
};

PhysicsBackendFactory& JoltFactory() {
    static PhysicsBackendFactory factory = nullptr;
    return factory;
}

} // namespace

void RegisterPhysicsBackendFactory(EPhysicsBackendKind kind, PhysicsBackendFactory factory) {
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
    return std::make_unique<ArcadePhysicsBackend>();
}

