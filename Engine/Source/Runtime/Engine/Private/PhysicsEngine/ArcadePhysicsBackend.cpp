#include "EngineLogs.h"
#include "IPhysicsBackend.h"

namespace
{

	class FArcadePhysicsBackend final : public IPhysicsBackend
	{
	public:
		[[nodiscard]] const TCHAR* GetName() const override
		{
			return "Arcade";
		}
	};

	FPhysicsBackendFactory& JoltFactory()
	{
		static FPhysicsBackendFactory Factory = nullptr;
		return Factory;
	}

} // namespace

void RegisterPhysicsBackendFactory(EPhysicsBackendKind Kind, FPhysicsBackendFactory Factory)
{
	if (Kind == EPhysicsBackendKind::Jolt)
	{
		JoltFactory() = Factory;
	}
}

TUniquePtr<IPhysicsBackend> CreatePhysicsBackend(EPhysicsBackendKind Kind)
{
	if (Kind == EPhysicsBackendKind::Jolt)
	{
		if (JoltFactory() != nullptr)
		{
			return JoltFactory()();
		}
		static bool bLoggedJoltFallback = false;
		if (!bLoggedJoltFallback)
		{
			bLoggedJoltFallback = true;
			UE_LOG(LogPhysics, Warning, "CreatePhysicsBackend: JoltPhysics plugin not enabled; falling back to Arcade");
		}
	}
	return MakeUnique<FArcadePhysicsBackend>();
}
