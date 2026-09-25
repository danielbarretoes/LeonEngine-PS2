#include "CommandletHelpers.h"

#include "Commandlets/Commandlet.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

UClass* CommandletHelpers::FindCommandletClass(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return nullptr;
	}
	TArray<UClass*> Classes;
	GetCommandletClasses(Classes);
	for (UClass* Class : Classes)
	{
		const FString ClassName = Class->GetName();
		if (ClassName == Name || ClassName == Name + TEXT("Commandlet"))
		{
			return Class;
		}
	}
	return nullptr;
}

void CommandletHelpers::GetCommandletClasses(TArray<UClass*>& OutClasses)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UCommandlet::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			OutClasses.Add(Class);
		}
	}
	OutClasses.Sort([](const UClass& A, const UClass& B) { return A.GetName() < B.GetName(); });
}
