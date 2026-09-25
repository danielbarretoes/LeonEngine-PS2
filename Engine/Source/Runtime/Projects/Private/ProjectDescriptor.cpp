#include "ProjectDescriptor.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FProjectDescriptor::FProjectDescriptor()
	: FileVersion(EProjectDescriptorVersion::Latest)
{
}

bool FProjectDescriptor::Load(const FString& FileName, FText& OutFailReason)
{
	// Read the file to a string.
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FileName))
	{
		OutFailReason = FText::FromString(FString::Printf("Failed to open descriptor file '%s'", *FileName));
		return false;
	}

	// Deserialize a JSON object from the string.
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		OutFailReason = FText::FromString(FString::Printf("Failed to read file. %s", *Reader->GetErrorMessage()));
		return false;
	}

	// Parse it as a project descriptor.
	return Read(*Object, OutFailReason);
}

bool FProjectDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	// Read the file version.
	int32 FileVersionInt32;
	if (!Object.TryGetNumberField("FileVersion", FileVersionInt32) &&
		!Object.TryGetNumberField("ProjectFileVersion", FileVersionInt32))
	{
		OutFailReason = FText::FromString("Unrecognized project file version");
		return false;
	}
	if (FileVersionInt32 <= EProjectDescriptorVersion::Invalid || FileVersionInt32 > EProjectDescriptorVersion::Latest)
	{
		OutFailReason =
			FText::FromString(FString::Printf("Project file version %d is not supported", FileVersionInt32));
		return false;
	}
	FileVersion = EProjectDescriptorVersion::Type(FileVersionInt32);

	// Read simple fields.
	Object.TryGetStringField("EngineAssociation", EngineAssociation);
	Object.TryGetStringField("Category", Category);
	Object.TryGetStringField("Description", Description);

	// Read the modules and the plugins.
	Modules.Empty();
	Plugins.Empty();
	if (!FModuleDescriptor::ReadArray(Object, "Modules", Modules, OutFailReason) ||
		!FPluginReferenceDescriptor::ReadArray(Object, "Plugins", Plugins, OutFailReason))
	{
		return false;
	}

	// Read the target platforms.
	TargetPlatforms.Empty();
	Object.TryGetStringArrayField("TargetPlatforms", TargetPlatforms);
	return true;
}

bool FProjectDescriptor::Save(const FString& FileName, FText& OutFailReason) const
{
	// Write the contents of the descriptor to a string.
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	Write(*Writer);
	Writer->Close();

	if (!FFileHelper::SaveStringToFile(Text, *FileName))
	{
		OutFailReason = FText::FromString(FString::Printf("Failed to write descriptor file '%s'", *FileName));
		return false;
	}
	return true;
}

void FProjectDescriptor::Write(TJsonWriter<>& Writer) const
{
	Writer.WriteObjectStart();

	// Write all the simple fields.
	Writer.WriteValue("FileVersion", int32(EProjectDescriptorVersion::Latest));
	Writer.WriteValue("EngineAssociation", EngineAssociation);
	Writer.WriteValue("Category", Category);
	Writer.WriteValue("Description", Description);

	// Write the module and plugin lists.
	FModuleDescriptor::WriteArray(Writer, "Modules", Modules);
	FPluginReferenceDescriptor::WriteArray(Writer, "Plugins", Plugins);

	// Write the target platforms.
	if (TargetPlatforms.Num() > 0)
	{
		Writer.WriteValue("TargetPlatforms", TargetPlatforms);
	}

	Writer.WriteObjectEnd();
}
