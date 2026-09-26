#include "Factories/MapImportSettings.h"

UMapImportSettings::UMapImportSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FMapImportNodeRule* UMapImportSettings::FindRule(const FString& NodeName) const
{
	const FMapImportNodeRule* Best = nullptr;
	for (const FMapImportNodeRule& Rule : NodeRules)
	{
		if (!Rule.Prefix.IsEmpty() && NodeName.StartsWith(Rule.Prefix, ESearchCase::IgnoreCase) &&
			(Best == nullptr || Rule.Prefix.Len() > Best->Prefix.Len()))
		{
			Best = &Rule;
		}
	}
	return Best;
}

bool UMapImportSettings::AppliesRequiredTags(const FString& MapPackageName)
{
	return !MapPackageName.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase);
}

FString UMapImportSettings::GetSuffix(const FString& NodeName, const FString& Prefix)
{
	FString Suffix = NodeName.StartsWith(Prefix, ESearchCase::IgnoreCase) ? NodeName.Mid(Prefix.Len()) : NodeName;
	// A Blender copy number: `.001`.
	int32 Dot = INDEX_NONE;
	if (Suffix.FindLastChar('.', Dot) && Dot + 1 < Suffix.Len())
	{
		bool bDigits = true;
		for (int32 Index = Dot + 1; Index < Suffix.Len(); ++Index)
		{
			bDigits &= FChar::IsDigit(Suffix[Index]);
		}
		if (bDigits)
		{
			Suffix.LeftInline(Dot);
		}
	}
	Suffix.RemoveFromStart(TEXT("_"));
	return Suffix;
}
