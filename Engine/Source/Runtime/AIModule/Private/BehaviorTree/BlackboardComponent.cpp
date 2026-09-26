#include "BehaviorTree/BehaviorTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlackboard, Log, All);

UBlackboardComponent::FEntry* UBlackboardComponent::FindForWrite(FName KeyName, EBlackboardKeyType Type)
{
	FEntry& Entry = Entries.FindOrAdd(KeyName);
	if (Entry.Type == EBlackboardKeyType::None)
	{
		Entry.Type = Type;
	}
	if (Entry.Type != Type)
	{
		UE_LOG(LogBlackboard, Warning, TEXT("Blackboard: key '%s' holds another type; the value is not set"),
			*KeyName.ToString());
		return nullptr;
	}
	Entry.bSet = true;
	return &Entry;
}

const UBlackboardComponent::FEntry* UBlackboardComponent::FindForRead(FName KeyName, EBlackboardKeyType Type) const
{
	const FEntry* Entry = Entries.Find(KeyName);
	return Entry != nullptr && Entry->Type == Type && Entry->bSet ? Entry : nullptr;
}

void UBlackboardComponent::SetValueAsBool(FName KeyName, bool bValue)
{
	if (FEntry* Entry = FindForWrite(KeyName, EBlackboardKeyType::Bool))
	{
		Entry->bValue = bValue;
	}
}

void UBlackboardComponent::SetValueAsInt(FName KeyName, int32 Value)
{
	if (FEntry* Entry = FindForWrite(KeyName, EBlackboardKeyType::Int))
	{
		Entry->IntValue = Value;
	}
}

void UBlackboardComponent::SetValueAsFloat(FName KeyName, float Value)
{
	if (FEntry* Entry = FindForWrite(KeyName, EBlackboardKeyType::Float))
	{
		Entry->FloatValue = Value;
	}
}

void UBlackboardComponent::SetValueAsVector(FName KeyName, const FVector& Value)
{
	if (FEntry* Entry = FindForWrite(KeyName, EBlackboardKeyType::Vector))
	{
		Entry->VectorValue = Value;
	}
}

void UBlackboardComponent::SetValueAsName(FName KeyName, FName Value)
{
	if (FEntry* Entry = FindForWrite(KeyName, EBlackboardKeyType::Name))
	{
		Entry->NameValue = Value;
	}
}

void UBlackboardComponent::SetValueAsObject(FName KeyName, UObject* Value)
{
	if (FEntry* Entry = FindForWrite(KeyName, EBlackboardKeyType::Object))
	{
		Entry->ObjectValue = Value;
	}
}

bool UBlackboardComponent::GetValueAsBool(FName KeyName) const
{
	const FEntry* Entry = FindForRead(KeyName, EBlackboardKeyType::Bool);
	return Entry != nullptr && Entry->bValue;
}

int32 UBlackboardComponent::GetValueAsInt(FName KeyName) const
{
	const FEntry* Entry = FindForRead(KeyName, EBlackboardKeyType::Int);
	return Entry != nullptr ? Entry->IntValue : 0;
}

float UBlackboardComponent::GetValueAsFloat(FName KeyName) const
{
	const FEntry* Entry = FindForRead(KeyName, EBlackboardKeyType::Float);
	return Entry != nullptr ? Entry->FloatValue : 0.0f;
}

FVector UBlackboardComponent::GetValueAsVector(FName KeyName) const
{
	const FEntry* Entry = FindForRead(KeyName, EBlackboardKeyType::Vector);
	return Entry != nullptr ? Entry->VectorValue : FVector::ZeroVector;
}

FName UBlackboardComponent::GetValueAsName(FName KeyName) const
{
	const FEntry* Entry = FindForRead(KeyName, EBlackboardKeyType::Name);
	return Entry != nullptr ? Entry->NameValue : NAME_None;
}

UObject* UBlackboardComponent::GetValueAsObject(FName KeyName) const
{
	const FEntry* Entry = FindForRead(KeyName, EBlackboardKeyType::Object);
	return Entry != nullptr ? Entry->ObjectValue.Get() : nullptr;
}

EBlackboardKeyType UBlackboardComponent::GetKeyType(FName KeyName) const
{
	const FEntry* Entry = Entries.Find(KeyName);
	return Entry != nullptr ? Entry->Type : EBlackboardKeyType::None;
}

bool UBlackboardComponent::IsValueSet(FName KeyName) const
{
	const FEntry* Entry = Entries.Find(KeyName);
	if (Entry == nullptr || !Entry->bSet)
	{
		return false;
	}
	return Entry->Type != EBlackboardKeyType::Object || Entry->ObjectValue.IsValid();
}

void UBlackboardComponent::ClearValue(FName KeyName)
{
	if (FEntry* Entry = Entries.Find(KeyName))
	{
		const EBlackboardKeyType Type = Entry->Type;
		*Entry = FEntry();
		Entry->Type = Type;
	}
}
