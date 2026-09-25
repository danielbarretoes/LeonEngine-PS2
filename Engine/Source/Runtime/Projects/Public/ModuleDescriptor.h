#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "Serialization/JsonWriter.h"
#include "UObject/NameTypes.h"

class FJsonObject;

/** Where a module may be used (UE: EHostType). */
namespace EHostType
{
	enum Type
	{
		Runtime,
		RuntimeNoCommandlet,
		RuntimeAndProgram,
		CookedOnly,
		UncookedOnly,
		Developer,
		DeveloperTool,
		Editor,
		EditorNoCommandlet,
		EditorAndProgram,
		Program,
		ServerOnly,
		ClientOnly,
		ClientOnlyNoCommandlet,
		Max
	};

	/** Parses a name ignoring case; Max when unknown. */
	PROJECTS_API EHostType::Type FromString(const TCHAR* String);
	PROJECTS_API const TCHAR* ToString(const EHostType::Type Value);
} // namespace EHostType

/** When a module starts up (UE: ELoadingPhase). Leon links statically and starts modules in dependency order. */
namespace ELoadingPhase
{
	enum Type
	{
		EarliestPossible,
		PostConfigInit,
		PostSplashScreen,
		PreEarlyLoadingScreen,
		PreLoadingScreen,
		PreDefault,
		Default,
		PostDefault,
		PostEngineInit,
		None,
		Max
	};

	PROJECTS_API ELoadingPhase::Type FromString(const TCHAR* String);
	PROJECTS_API const TCHAR* ToString(const ELoadingPhase::Type Value);
} // namespace ELoadingPhase

/**
 * One entry of a descriptor's "Modules" list (UE: FModuleDescriptor). Platform lists use UE 5's names
 * (PlatformAllowList / PlatformDenyList), which the .lplugin files use; UE 4.27's WhitelistPlatforms /
 * BlacklistPlatforms are read too.
 */
struct PROJECTS_API FModuleDescriptor
{
	FName Name;
	EHostType::Type Type;
	ELoadingPhase::Type LoadingPhase;

	/** Platforms the module is built for; empty for all (LeonBuildTool names: Win64, Linux, PS2). */
	TArray<FString> PlatformAllowList;
	TArray<FString> PlatformDenyList;

	TArray<FString> AdditionalDependencies;

	FModuleDescriptor(const FName InName = NAME_None, EHostType::Type InType = EHostType::Runtime,
		ELoadingPhase::Type InLoadingPhase = ELoadingPhase::Default);

	bool Read(const FJsonObject& Object, FText& OutFailReason);
	static bool ReadArray(
		const FJsonObject& Object, const TCHAR* Name, TArray<FModuleDescriptor>& OutModules, FText& OutFailReason);

	void Write(TJsonWriter<>& Writer) const;
	static void WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FModuleDescriptor>& Modules);

	/** Built for this platform (UE: IsCompiledInCurrentConfiguration, platform lists only). */
	bool IsCompiledForPlatform(const FString& Platform) const;
};
