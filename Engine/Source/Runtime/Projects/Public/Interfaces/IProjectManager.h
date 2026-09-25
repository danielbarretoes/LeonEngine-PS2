#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "ProjectDescriptor.h"

/** The loaded project descriptor (UE: IProjectManager). */
class PROJECTS_API IProjectManager
{
public:
	virtual ~IProjectManager() = default;

	/** The project loaded by LoadProjectFile, or nullptr. */
	virtual const FProjectDescriptor* GetCurrentProject() const = 0;

	/** Reads the .lproj; also refreshes the plugin list, whose enabled state depends on it. */
	virtual bool LoadProjectFile(const FString& ProjectFile) = 0;

	static IProjectManager& Get();
};
