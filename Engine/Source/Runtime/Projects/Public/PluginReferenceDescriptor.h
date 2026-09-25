#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "Serialization/JsonWriter.h"

class FJsonObject;

/** A project's (or plugin's) reference to a plugin, in its "Plugins" list (UE: FPluginReferenceDescriptor). */
struct PROJECTS_API FPluginReferenceDescriptor
{
	FString Name;
	bool bEnabled;
	bool bOptional;
	FString Description;

	/** Platforms the reference applies to; empty for all. */
	TArray<FString> PlatformAllowList;
	TArray<FString> PlatformDenyList;

	FPluginReferenceDescriptor(const FString& InName = FString(), bool bInEnabled = false);

	/** Enabled for this platform (UE: IsEnabledForPlatform). */
	bool IsEnabledForPlatform(const FString& Platform) const;

	bool Read(const FJsonObject& Object, FText& OutFailReason);
	static bool ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FPluginReferenceDescriptor>& OutPlugins,
		FText& OutFailReason);

	void Write(TJsonWriter<>& Writer) const;
	static void WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FPluginReferenceDescriptor>& Plugins);
};
