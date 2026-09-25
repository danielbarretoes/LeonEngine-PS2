#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "ScenePrivate.h"

namespace
{

	/** The index at which an element with OrderKey keeps the array sorted (after equal keys). */
	template <typename InfoType>
	[[nodiscard]] int32 InsertionIndex(const TArray<InfoType>& Infos, uint64 OrderKey)
	{
		int32 Index = Infos.Num();
		while (Index > 0 && Infos[Index - 1].OrderKey > OrderKey)
		{
			--Index;
		}
		return Index;
	}

	template <typename InfoType, typename ComponentType>
	[[nodiscard]] int32 FindInfo(const TArray<InfoType>& Infos, const ComponentType* Component)
	{
		for (int32 Index = 0; Index < Infos.Num(); ++Index)
		{
			if (Infos[Index].Component == Component)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

} // namespace

FScene::FScene(UWorld* InWorld)
	: World(InWorld)
{
}

FScene::~FScene()
{
	// A component still in the scene when it goes (the world's owner did not unregister it) forgets its proxy.
	for (FPrimitiveSceneInfo& Info : Primitives)
	{
		if (Info.Component != nullptr && Info.Component->SceneProxy == Info.Proxy.Get())
		{
			Info.Component->SceneProxy = nullptr;
		}
	}
	for (FLightSceneInfo& Info : Lights)
	{
		if (Info.Component != nullptr && Info.Component->SceneProxy == Info.Proxy.Get())
		{
			Info.Component->SceneProxy = nullptr;
		}
	}
}

uint64 FScene::GetOrderKey(const UActorComponent* Component)
{
	const AActor* Owner = Component != nullptr ? Component->GetOwner() : nullptr;
	if (Owner == nullptr)
	{
		return TNumericLimits<uint64>::Max();
	}
	const uint64 ComponentIndex =
		static_cast<uint64>(FMath::Max(Owner->GetComponents().Find(const_cast<UActorComponent*>(Component)), 0));
	return (Owner->GetUniqueID() << 20) | (ComponentIndex & 0xFFFFFu);
}

void FScene::AddPrimitive(UPrimitiveComponent* Primitive)
{
	if (Primitive == nullptr || FindInfo(Primitives, Primitive) != INDEX_NONE)
	{
		return;
	}
	FPrimitiveSceneProxy* Proxy = Primitive->CreateSceneProxy();
	if (Proxy == nullptr)
	{
		return;
	}
	FPrimitiveSceneInfo Info;
	Info.Component = Primitive;
	Info.Proxy.Reset(Proxy);
	Info.OrderKey = GetOrderKey(Primitive);
	const int32 Index = InsertionIndex(Primitives, Info.OrderKey);
	Primitives.Insert(MoveTemp(Info), Index);
	Primitive->SceneProxy = Proxy;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	const int32 Index = FindInfo(Primitives, Primitive);
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (Primitive->SceneProxy == Primitives[Index].Proxy.Get())
	{
		Primitive->SceneProxy = nullptr;
	}
	Primitives.RemoveAt(Index);
}

void FScene::UpdatePrimitiveTransform(UPrimitiveComponent* Primitive)
{
	if (Primitive != nullptr && Primitive->SceneProxy != nullptr)
	{
		Primitive->SceneProxy->SetTransform(Primitive->GetComponentTransform().ToMatrixWithScale());
	}
}

void FScene::AddLight(ULightComponent* Light)
{
	if (Light == nullptr || FindInfo(Lights, Light) != INDEX_NONE)
	{
		return;
	}
	FLightSceneProxy* Proxy = Light->CreateSceneProxy();
	if (Proxy == nullptr)
	{
		return;
	}
	FLightSceneInfo Info;
	Info.Component = Light;
	Info.Proxy.Reset(Proxy);
	Info.OrderKey = GetOrderKey(Light);
	const int32 Index = InsertionIndex(Lights, Info.OrderKey);
	Lights.Insert(MoveTemp(Info), Index);
	Light->SceneProxy = Proxy;
}

void FScene::RemoveLight(ULightComponent* Light)
{
	const int32 Index = FindInfo(Lights, Light);
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (Light->SceneProxy == Lights[Index].Proxy.Get())
	{
		Light->SceneProxy = nullptr;
	}
	Lights.RemoveAt(Index);
}

void FScene::UpdateLightTransform(ULightComponent* Light)
{
	if (Light != nullptr && Light->SceneProxy != nullptr)
	{
		Light->SceneProxy->SetTransform(Light->GetComponentTransform());
	}
}
