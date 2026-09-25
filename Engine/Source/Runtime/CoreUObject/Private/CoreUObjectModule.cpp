#include "Modules/ModuleManager.h"
#include "UObject/UObjectBase.h"

/**
 * Starts the object system when the module starts (UE: FCoreUObjectModule and StaticUObjectInit): the object array,
 * the intrinsic classes, CoreUObject's own reflected types, then every module loaded after it through
 * FModuleManager::OnProcessLoadedObjectsCallback.
 */
class FCoreUObjectModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UObjectBaseInit();
		ProcessNewlyLoadedUObjects(TEXT("CoreUObject"), true);
		FModuleManager::Get().OnProcessLoadedObjectsCallback() = &ProcessNewlyLoadedUObjects;
	}

	virtual void ShutdownModule() override
	{
		// Objects left when the modules shut down stay until the process exits (UE does not collect at exit either).
		FModuleManager::Get().OnProcessLoadedObjectsCallback() = nullptr;
	}
};

IMPLEMENT_MODULE(FCoreUObjectModule, CoreUObject)
