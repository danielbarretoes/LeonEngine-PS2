#include "IPhysicsBackend.h"

#include <iostream>

namespace
{

	class FArcadePhysicsBackend final : public IPhysicsBackend
	{
	public:
		[[nodiscard]] const char* GetName() const override
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

std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(EPhysicsBackendKind Kind)
{
	if (Kind == EPhysicsBackendKind::Jolt)
	{
		if (JoltFactory() != nullptr)
		{
			return JoltFactory()();
		}
		static bool bSLoggedJoltFallback = false;
		if (!bSLoggedJoltFallback)
		{
			bSLoggedJoltFallback = true;
			std::cerr << "CreatePhysicsBackend: JoltPhysics plugin not enabled; falling back to Arcade\n";
		}
	}
	return std::make_unique<FArcadePhysicsBackend>();
}
