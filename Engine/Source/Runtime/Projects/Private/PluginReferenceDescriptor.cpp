#include "PluginReferenceDescriptor.h"

#include "Dom/JsonObject.h"

FPluginReferenceDescriptor::FPluginReferenceDescriptor(const FString& InName, bool bInEnabled)
	: Name(InName)
	, bEnabled(bInEnabled)
	, bOptional(false)
{
}

bool FPluginReferenceDescriptor::IsEnabledForPlatform(const FString& Platform) const
{
	// If it's not enabled at all, return false.
	if (!bEnabled)
	{
		return false;
	}
	// If there is a list of allowed platforms, and this isn't one of them, return false.
	if (PlatformAllowList.Num() > 0 && !PlatformAllowList.Contains(Platform))
	{
		return false;
	}
	// If this platform is denied, also return false.
	return !PlatformDenyList.Contains(Platform);
}

bool FPluginReferenceDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	// Get the name.
	if (!Object.TryGetStringField("Name", Name))
	{
		OutFailReason = FText::FromString("Plugin references must have a 'Name' field");
		return false;
	}

	// Get the enabled field.
	if (!Object.TryGetBoolField("Enabled", bEnabled))
	{
		OutFailReason = FText::FromString(FString::Printf("Plugin reference '%s' must have an 'Enabled' field", *Name));
		return false;
	}

	// Read the optional fields.
	Object.TryGetBoolField("Optional", bOptional);
	Object.TryGetStringField("Description", Description);
	if (!Object.TryGetStringArrayField("PlatformAllowList", PlatformAllowList))
	{
		Object.TryGetStringArrayField("WhitelistPlatforms", PlatformAllowList);
	}
	if (!Object.TryGetStringArrayField("PlatformDenyList", PlatformDenyList))
	{
		Object.TryGetStringArrayField("BlacklistPlatforms", PlatformDenyList);
	}
	return true;
}

bool FPluginReferenceDescriptor::ReadArray(
	const FJsonObject& Object, const TCHAR* Name, TArray<FPluginReferenceDescriptor>& OutPlugins, FText& OutFailReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!Object.TryGetArrayField(Name, Array))
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Item : *Array)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Item.IsValid() || !Item->TryGetObject(ObjectPtr) || !ObjectPtr->IsValid())
		{
			OutFailReason = FText::FromString(FString::Printf("'%s' must be an array of objects", Name));
			return false;
		}
		FPluginReferenceDescriptor Descriptor;
		if (!Descriptor.Read(**ObjectPtr, OutFailReason))
		{
			return false;
		}
		OutPlugins.Add(Descriptor);
	}
	return true;
}

void FPluginReferenceDescriptor::Write(TJsonWriter<>& Writer) const
{
	Writer.WriteObjectStart();
	Writer.WriteValue("Name", Name);
	Writer.WriteValue("Enabled", bEnabled);
	if (bEnabled && bOptional)
	{
		Writer.WriteValue("Optional", bOptional);
	}
	if (!Description.IsEmpty())
	{
		Writer.WriteValue("Description", Description);
	}
	if (PlatformAllowList.Num() > 0)
	{
		Writer.WriteValue("PlatformAllowList", PlatformAllowList);
	}
	if (PlatformDenyList.Num() > 0)
	{
		Writer.WriteValue("PlatformDenyList", PlatformDenyList);
	}
	Writer.WriteObjectEnd();
}

void FPluginReferenceDescriptor::WriteArray(
	TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FPluginReferenceDescriptor>& Plugins)
{
	Writer.WriteArrayStart(Name);
	for (const FPluginReferenceDescriptor& Plugin : Plugins)
	{
		Plugin.Write(Writer);
	}
	Writer.WriteArrayEnd();
}
