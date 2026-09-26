#include "Engine/CollisionProfile.h"

#include "EngineLogs.h"

namespace
{

	/** The engine channels' names (UE: the display names of ECollisionChannel). */
	const TCHAR* const EngineChannelNames[] = {
		TEXT("WorldStatic"),
		TEXT("WorldDynamic"),
		TEXT("Pawn"),
		TEXT("Visibility"),
		TEXT("Camera"),
		TEXT("PhysicsBody"),
		TEXT("Vehicle"),
		TEXT("Destructible"),
	};

} // namespace

UCollisionProfile::UCollisionProfile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UCollisionProfile* UCollisionProfile::Get()
{
	return GetMutableDefault<UCollisionProfile>();
}

void UCollisionProfile::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		LoadProfileConfig();
	}
}

void UCollisionProfile::LoadProfileConfig(bool /*bForceInit*/)
{
	FCollisionResponseContainer Defaults(ECR_Block);
	for (const FCustomChannelSetup& Setup : DefaultChannelResponses)
	{
		if (Setup.Channel.GetValue() >= FCollisionResponseContainer::NumChannels)
		{
			UE_LOG(LogEngine, Warning, TEXT("CollisionProfile: channel %d of '%s' is not a collision channel"),
				static_cast<int32>(Setup.Channel.GetValue()), *Setup.Name.ToString());
			continue;
		}
		(void)Defaults.SetResponse(Setup.Channel.GetValue(), Setup.DefaultResponse.GetValue());
	}
	FCollisionResponseContainer::DefaultResponseContainer = Defaults;
}

FName UCollisionProfile::ReturnChannelNameFromContainerIndex(int32 ContainerIndex) const
{
	for (const FCustomChannelSetup& Setup : DefaultChannelResponses)
	{
		if (static_cast<int32>(Setup.Channel.GetValue()) == ContainerIndex)
		{
			return Setup.Name;
		}
	}
	if (ContainerIndex >= 0 && ContainerIndex < static_cast<int32>(UE_ARRAY_COUNT(EngineChannelNames)))
	{
		return FName(EngineChannelNames[ContainerIndex]);
	}
	return NAME_None;
}

int32 UCollisionProfile::ReturnContainerIndexFromChannelName(FName DisplayName) const
{
	for (int32 Index = 0; Index < FCollisionResponseContainer::NumChannels; ++Index)
	{
		if (DisplayName != NAME_None && ReturnChannelNameFromContainerIndex(Index) == DisplayName)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
