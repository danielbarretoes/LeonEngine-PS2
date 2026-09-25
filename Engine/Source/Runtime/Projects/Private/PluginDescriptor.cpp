#include "PluginDescriptor.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FPluginDescriptor::FPluginDescriptor()
	: FileVersion(3)
	, Version(0)
	, EnabledByDefault(EPluginEnabledByDefault::Unspecified)
	, bCanContainContent(false)
	, bIsBetaVersion(false)
	, bIsExperimentalVersion(false)
	, bIsHidden(false)
{
}

bool FPluginDescriptor::Load(const FString& FileName, FText& OutFailReason)
{
	// Load the file into a string.
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FileName))
	{
		OutFailReason = FText::FromString(FString::Printf("Failed to open descriptor file '%s'", *FileName));
		return false;
	}
	return Read(Text, OutFailReason);
}

bool FPluginDescriptor::Read(const FString& Text, FText& OutFailReason)
{
	// Deserialize a JSON object from the string.
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		OutFailReason = FText::FromString(FString::Printf("Failed to read file. %s", *Reader->GetErrorMessage()));
		return false;
	}
	return Read(*Object, OutFailReason);
}

bool FPluginDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	// Read the file version.
	if (!Object.TryGetNumberField("FileVersion", FileVersion) &&
		!Object.TryGetNumberField("PluginFileVersion", FileVersion))
	{
		OutFailReason = FText::FromString("Invalid plugin file version");
		return false;
	}
	if (FileVersion < 1 || FileVersion > 3)
	{
		OutFailReason = FText::FromString(FString::Printf("Plugin file version %d is not supported", FileVersion));
		return false;
	}

	// Read the other fields.
	Object.TryGetNumberField("Version", Version);
	Object.TryGetStringField("VersionName", VersionName);
	Object.TryGetStringField("FriendlyName", FriendlyName);
	Object.TryGetStringField("Description", Description);
	if (!Object.TryGetStringField("Category", Category))
	{
		// Category used to be called CategoryPath in .uplugin files.
		Object.TryGetStringField("CategoryPath", Category);
	}
	Object.TryGetStringField("CreatedBy", CreatedBy);
	Object.TryGetStringField("CreatedByURL", CreatedByURL);
	Object.TryGetStringField("DocsURL", DocsURL);
	Object.TryGetStringField("EngineVersion", EngineVersion);
	Object.TryGetStringArrayField("SupportedTargetPlatforms", SupportedTargetPlatforms);

	Modules.Empty();
	if (!FModuleDescriptor::ReadArray(Object, "Modules", Modules, OutFailReason))
	{
		return false;
	}

	bool bEnabledByDefault = false;
	if (Object.TryGetBoolField("EnabledByDefault", bEnabledByDefault))
	{
		EnabledByDefault = bEnabledByDefault ? EPluginEnabledByDefault::Enabled : EPluginEnabledByDefault::Disabled;
	}
	Object.TryGetBoolField("CanContainContent", bCanContainContent);
	Object.TryGetBoolField("IsBetaVersion", bIsBetaVersion);
	Object.TryGetBoolField("IsExperimentalVersion", bIsExperimentalVersion);
	Object.TryGetBoolField("IsHidden", bIsHidden);

	Plugins.Empty();
	return FPluginReferenceDescriptor::ReadArray(Object, "Plugins", Plugins, OutFailReason);
}

bool FPluginDescriptor::Save(const FString& FileName, FText& OutFailReason) const
{
	FString Text;
	Write(Text);
	if (!FFileHelper::SaveStringToFile(Text, *FileName))
	{
		OutFailReason = FText::FromString(FString::Printf("Failed to write descriptor file '%s'", *FileName));
		return false;
	}
	return true;
}

void FPluginDescriptor::Write(FString& Text) const
{
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	Write(*Writer);
	Writer->Close();
}

void FPluginDescriptor::Write(TJsonWriter<>& Writer) const
{
	Writer.WriteObjectStart();
	Writer.WriteValue("FileVersion", FileVersion);
	Writer.WriteValue("Version", Version);
	Writer.WriteValue("VersionName", VersionName);
	Writer.WriteValue("FriendlyName", FriendlyName);
	Writer.WriteValue("Description", Description);
	Writer.WriteValue("Category", Category);
	Writer.WriteValue("CreatedBy", CreatedBy);
	Writer.WriteValue("CreatedByURL", CreatedByURL);
	Writer.WriteValue("DocsURL", DocsURL);
	if (!EngineVersion.IsEmpty())
	{
		Writer.WriteValue("EngineVersion", EngineVersion);
	}
	if (EnabledByDefault != EPluginEnabledByDefault::Unspecified)
	{
		Writer.WriteValue("EnabledByDefault", EnabledByDefault == EPluginEnabledByDefault::Enabled);
	}
	Writer.WriteValue("CanContainContent", bCanContainContent);
	Writer.WriteValue("IsBetaVersion", bIsBetaVersion);
	Writer.WriteValue("IsExperimentalVersion", bIsExperimentalVersion);
	if (bIsHidden)
	{
		Writer.WriteValue("IsHidden", bIsHidden);
	}
	if (SupportedTargetPlatforms.Num() > 0)
	{
		Writer.WriteValue("SupportedTargetPlatforms", SupportedTargetPlatforms);
	}
	FModuleDescriptor::WriteArray(Writer, "Modules", Modules);
	if (Plugins.Num() > 0)
	{
		FPluginReferenceDescriptor::WriteArray(Writer, "Plugins", Plugins);
	}
	Writer.WriteObjectEnd();
}
