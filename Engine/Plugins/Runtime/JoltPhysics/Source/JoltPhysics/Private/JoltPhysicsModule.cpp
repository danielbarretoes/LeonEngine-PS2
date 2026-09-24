#include "IPhysicsBackend.h"
#include "JoltPhysicsBackend.h"
#include "Modules/ModuleManager.h"

/** Registers the Jolt backend with the engine's physics backend registry. */
class FJoltPhysicsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		RegisterPhysicsBackendFactory(EPhysicsBackendKind::Jolt, &CreateJoltPhysicsBackend);
	}

	virtual void ShutdownModule() override
	{
		RegisterPhysicsBackendFactory(EPhysicsBackendKind::Jolt, nullptr);
	}
};

IMPLEMENT_MODULE(FJoltPhysicsModule, JoltPhysics)
