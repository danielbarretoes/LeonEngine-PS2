#include <iostream>
#include <leon/physics/IPhysicsBackend.h>

#if defined(LEON_WITH_JOLT) && LEON_WITH_JOLT
#include <leon/physics/JoltPhysicsBackend.h>
#endif

namespace leon {
namespace {

class ArcadePhysicsBackend final : public IPhysicsBackend {
public:
    [[nodiscard]] const char* GetName() const override { return "Arcade"; }
};

} // namespace

std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(EPhysicsBackendKind kind) {
    if (kind == EPhysicsBackendKind::Jolt) {
#if defined(LEON_WITH_JOLT) && LEON_WITH_JOLT
        return CreateJoltPhysicsBackend();
#else
        static bool s_loggedJoltFallback = false;
        if (!s_loggedJoltFallback) {
            s_loggedJoltFallback = true;
            std::cerr << "CreatePhysicsBackend: LEON_WITH_JOLT is off; falling back to Arcade\n";
        }
#endif
    }
    return std::make_unique<ArcadePhysicsBackend>();
}

} // namespace leon
