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
		// Objects stay alive until the process exits: destroying them is the P10 garbage collector's job.
		FModuleManager::Get().OnProcessLoadedObjectsCallback() = nullptr;
	}
};

IMPLEMENT_MODULE(FCoreUObjectModule, CoreUObject)
