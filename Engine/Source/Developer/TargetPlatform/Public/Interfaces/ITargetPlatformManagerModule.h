#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class ITargetPlatform;

/**
 * The TargetPlatform module's interface (UE: ITargetPlatformManagerModule): the platforms the tools can cook for. Leon
 * builds the Win64 and PS2 platforms into the module (UE loads a <Platform>TargetPlatform module for each).
 */
class ITargetPlatformManagerModule : public IModuleInterface
{
public:
	/** Every platform, sorted by name (UE: GetTargetPlatforms). */
	virtual const TArray<ITargetPlatform*>& GetTargetPlatforms() = 0;

	/** The platform of that name, ignoring case, or nullptr (UE: FindTargetPlatform). */
	virtual ITargetPlatform* FindTargetPlatform(const FString& Name) = 0;

	/**
	 * The platforms the command line's -TargetPlatform=A+B names, or the running one without it; an unknown name is
	 * logged and left out (UE: GetActiveTargetPlatforms).
	 */
	virtual const TArray<ITargetPlatform*>& GetActiveTargetPlatforms() = 0;

	/** The platform the tools run on: Win64 on Windows (UE: GetRunningTargetPlatform). */
	virtual ITargetPlatform* GetRunningTargetPlatform() = 0;
};

/** The TargetPlatform module, or nullptr in a program that does not link it (UE: GetTargetPlatformManager). */
TARGETPLATFORM_API ITargetPlatformManagerModule* GetTargetPlatformManager();

/** The TargetPlatform module; the program must link it (UE: GetTargetPlatformManagerRef). */
TARGETPLATFORM_API ITargetPlatformManagerModule& GetTargetPlatformManagerRef();
