#include "ModuleDescriptor.h"

#include "Dom/JsonObject.h"
#include "Misc/CString.h"
#include "Templates/UnrealTemplate.h"

namespace
{
	const TCHAR* const HostTypeNames[] = {"Runtime", "RuntimeNoCommandlet", "RuntimeAndProgram", "CookedOnly",
		"UncookedOnly", "Developer", "DeveloperTool", "Editor", "EditorNoCommandlet", "EditorAndProgram", "Program",
		"ServerOnly", "ClientOnly", "ClientOnlyNoCommandlet"};

	const TCHAR* const LoadingPhaseNames[] = {"EarliestPossible", "PostConfigInit", "PostSplashScreen",
		"PreEarlyLoadingScreen", "PreLoadingScreen", "PreDefault", "Default", "PostDefault", "PostEngineInit", "None"};

	static_assert(UE_ARRAY_COUNT(HostTypeNames) == EHostType::Max, "Host type names out of date");
	static_assert(UE_ARRAY_COUNT(LoadingPhaseNames) == ELoadingPhase::Max, "Loading phase names out of date");

	void ReadStringArray(const FJsonObject& Object, const TCHAR* Name, const TCHAR* LegacyName, TArray<FString>& Out)
	{
		if (!Object.TryGetStringArrayField(Name, Out) && LegacyName != nullptr)
		{
			Object.TryGetStringArrayField(LegacyName, Out);
		}
	}
} // namespace

EHostType::Type EHostType::FromString(const TCHAR* String)
{
	for (int32 Index = 0; Index < EHostType::Max; ++Index)
	{
		if (FCString::Stricmp(String, HostTypeNames[Index]) == 0)
		{
			return EHostType::Type(Index);
		}
	}
	return EHostType::Max;
}

const TCHAR* EHostType::ToString(const EHostType::Type Value)
{
	return (Value >= 0 && Value < EHostType::Max) ? HostTypeNames[Value] : "";
}

ELoadingPhase::Type ELoadingPhase::FromString(const TCHAR* String)
{
	for (int32 Index = 0; Index < ELoadingPhase::Max; ++Index)
	{
		if (FCString::Stricmp(String, LoadingPhaseNames[Index]) == 0)
		{
			return ELoadingPhase::Type(Index);
		}
	}
	return ELoadingPhase::Max;
}

const TCHAR* ELoadingPhase::ToString(const ELoadingPhase::Type Value)
{
	return (Value >= 0 && Value < ELoadingPhase::Max) ? LoadingPhaseNames[Value] : "";
}

FModuleDescriptor::FModuleDescriptor(const FName InName, EHostType::Type InType, ELoadingPhase::Type InLoadingPhase)
	: Name(InName)
	, Type(InType)
	, LoadingPhase(InLoadingPhase)
{
}

bool FModuleDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	// Read the module name.
	FString NameString;
	if (!Object.TryGetStringField("Name", NameString))
	{
		OutFailReason = FText::FromString("Module must have a 'Name' field");
		return false;
	}
	Name = FName(*NameString);

	// Read the module type.
	FString TypeString;
	if (!Object.TryGetStringField("Type", TypeString))
	{
		OutFailReason = FText::FromString(FString::Printf("Module '%s' must have a 'Type' field", *NameString));
		return false;
	}
	Type = EHostType::FromString(*TypeString);
	if (Type == EHostType::Max)
	{
		OutFailReason =
			FText::FromString(FString::Printf("Module '%s' has an unknown 'Type' of '%s'", *NameString, *TypeString));
		return false;
	}

	// Read the loading phase.
	FString LoadingPhaseString;
	if (Object.TryGetStringField("LoadingPhase", LoadingPhaseString))
	{
		LoadingPhase = ELoadingPhase::FromString(*LoadingPhaseString);
		if (LoadingPhase == ELoadingPhase::Max)
		{
			OutFailReason = FText::FromString(
				FString::Printf("Module '%s' has an unknown 'LoadingPhase' of '%s'", *NameString, *LoadingPhaseString));
			return false;
		}
	}

	ReadStringArray(Object, "PlatformAllowList", "WhitelistPlatforms", PlatformAllowList);
	ReadStringArray(Object, "PlatformDenyList", "BlacklistPlatforms", PlatformDenyList);
	ReadStringArray(Object, "AdditionalDependencies", nullptr, AdditionalDependencies);
	return true;
}

bool FModuleDescriptor::ReadArray(
	const FJsonObject& Object, const TCHAR* Name, TArray<FModuleDescriptor>& OutModules, FText& OutFailReason)
{
	const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
	if (!Object.TryGetArrayField(Name, ModulesArray))
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& ModuleValue : *ModulesArray)
	{
		const TSharedPtr<FJsonObject>* ModuleObject = nullptr;
		if (!ModuleValue.IsValid() || !ModuleValue->TryGetObject(ModuleObject) || !ModuleObject->IsValid())
		{
			OutFailReason = FText::FromString(FString::Printf("'%s' must be an array of objects", Name));
			return false;
		}
		FModuleDescriptor Descriptor;
		if (!Descriptor.Read(**ModuleObject, OutFailReason))
		{
			return false;
		}
		OutModules.Add(Descriptor);
	}
	return true;
}

void FModuleDescriptor::Write(TJsonWriter<>& Writer) const
{
	Writer.WriteObjectStart();
	Writer.WriteValue("Name", Name.ToString());
	Writer.WriteValue("Type", FString(EHostType::ToString(Type)));
	Writer.WriteValue("LoadingPhase", FString(ELoadingPhase::ToString(LoadingPhase)));
	if (PlatformAllowList.Num() > 0)
	{
		Writer.WriteValue("PlatformAllowList", PlatformAllowList);
	}
	if (PlatformDenyList.Num() > 0)
	{
		Writer.WriteValue("PlatformDenyList", PlatformDenyList);
	}
	if (AdditionalDependencies.Num() > 0)
	{
		Writer.WriteValue("AdditionalDependencies", AdditionalDependencies);
	}
	Writer.WriteObjectEnd();
}

void FModuleDescriptor::WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FModuleDescriptor>& Modules)
{
	if (Modules.Num() == 0)
	{
		return;
	}
	Writer.WriteArrayStart(Name);
	for (const FModuleDescriptor& Module : Modules)
	{
		Module.Write(Writer);
	}
	Writer.WriteArrayEnd();
}

bool FModuleDescriptor::IsCompiledForPlatform(const FString& Platform) const
{
	if (PlatformAllowList.Num() > 0 && !PlatformAllowList.Contains(Platform))
	{
		return false;
	}
	return !PlatformDenyList.Contains(Platform);
}
