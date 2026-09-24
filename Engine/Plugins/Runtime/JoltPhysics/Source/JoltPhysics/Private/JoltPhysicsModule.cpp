#include "IPhysicsBackend.h"
#include "JoltPhysicsBackend.h"
#include "Modules/ModuleManager.h"

/** Registers the Jolt backend with the engine's physics backend registry. */
class FJoltPhysicsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		leon::RegisterPhysicsBackendFactory(leon::EPhysicsBackendKind::Jolt, &leon::CreateJoltPhysicsBackend);
	}

	virtual void ShutdownModule() override
	{
		leon::RegisterPhysicsBackendFactory(leon::EPhysicsBackendKind::Jolt, nullptr);
	}
};

IMPLEMENT_MODULE(FJoltPhysicsModule, JoltPhysics)
