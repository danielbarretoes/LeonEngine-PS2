#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "ModuleDescriptor.h"
#include "PluginReferenceDescriptor.h"
#include "Serialization/JsonWriter.h"

class FJsonObject;

/** Versions of the project file format (UE: EProjectDescriptorVersion). */
namespace EProjectDescriptorVersion
{
	enum Type
	{
		Invalid = 0,
		Initial = 1,
		NameHash = 2,
		ProjectPluginUnification = 3,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};
} // namespace EProjectDescriptorVersion

/** A .lproj file: the project's modules, plugins and platforms (UE: FProjectDescriptor, .uproject). */
struct PROJECTS_API FProjectDescriptor
{
	EProjectDescriptorVersion::Type FileVersion;
	FString EngineAssociation;
	FString Category;
	FString Description;
	TArray<FModuleDescriptor> Modules;
	TArray<FPluginReferenceDescriptor> Plugins;

	/** Platforms the project builds for (LeonBuildTool names); empty for all. */
	TArray<FString> TargetPlatforms;

	FProjectDescriptor();

	bool Load(const FString& FileName, FText& OutFailReason);
	bool Read(const FJsonObject& Object, FText& OutFailReason);
	bool Save(const FString& FileName, FText& OutFailReason) const;
	void Write(TJsonWriter<>& Writer) const;

	/** "lproj" (UE: "uproject"). */
	static FString GetExtension()
	{
		return FString("lproj");
	}
};
