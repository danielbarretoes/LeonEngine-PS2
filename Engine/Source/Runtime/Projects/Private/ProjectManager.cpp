#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectManager, Log, All);

namespace
{
	class FProjectManager final : public IProjectManager
	{
	public:
		virtual const FProjectDescriptor* GetCurrentProject() const override
		{
			return bLoaded ? &CurrentProject : nullptr;
		}

		virtual bool LoadProjectFile(const FString& ProjectFile) override
		{
			FProjectDescriptor Descriptor;
			FText FailReason;
			if (!Descriptor.Load(ProjectFile, FailReason))
			{
				UE_LOG(LogProjectManager, Warning, "Could not load %s: %s", *ProjectFile, *FailReason.ToString());
				return false;
			}
			CurrentProject = Descriptor;
			bLoaded = true;
			IPluginManager::Get().RefreshPluginsList();
			return true;
		}

	private:
		FProjectDescriptor CurrentProject;
		bool bLoaded = false;
	};
} // namespace

IProjectManager& IProjectManager::Get()
{
	static FProjectManager Singleton;
	return Singleton;
}
