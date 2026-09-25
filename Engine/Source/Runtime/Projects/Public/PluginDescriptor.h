#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "ModuleDescriptor.h"
#include "PluginReferenceDescriptor.h"
#include "Serialization/JsonWriter.h"

class FJsonObject;

/** Whether a plugin is on when nothing references it (UE: EPluginEnabledByDefault). */
enum class EPluginEnabledByDefault : uint8
{
	Unspecified,
	Enabled,
	Disabled
};

/** A .lplugin file (UE: FPluginDescriptor, .uplugin). */
struct PROJECTS_API FPluginDescriptor
{
	int32 FileVersion;
	int32 Version;
	FString VersionName;
	FString FriendlyName;
	FString Description;
	FString Category;
	FString CreatedBy;
	FString CreatedByURL;
	FString DocsURL;
	FString EngineVersion;
	TArray<FString> SupportedTargetPlatforms;
	TArray<FModuleDescriptor> Modules;
	EPluginEnabledByDefault EnabledByDefault;
	bool bCanContainContent;
	bool bIsBetaVersion;
	bool bIsExperimentalVersion;
	bool bIsHidden;
	TArray<FPluginReferenceDescriptor> Plugins;

	FPluginDescriptor();

	bool Load(const FString& FileName, FText& OutFailReason);
	bool Read(const FString& Text, FText& OutFailReason);
	bool Read(const FJsonObject& Object, FText& OutFailReason);
	bool Save(const FString& FileName, FText& OutFailReason) const;
	void Write(FString& Text) const;
	void Write(TJsonWriter<>& Writer) const;

	/** "lplugin" (UE: "uplugin"). */
	static FString GetExtension()
	{
		return FString("lplugin");
	}
};
