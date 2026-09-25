#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"

/** InputCore registers the keys' details when it starts (UE: FInputCoreModule). */
class FInputCoreModule : public FDefaultModuleImpl
{
public:
	void StartupModule() override
	{
		EKeys::Initialize();
	}
};

IMPLEMENT_MODULE(FInputCoreModule, InputCore)
