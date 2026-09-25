#include "Components/LightComponentBase.h"

ULightComponentBase::ULightComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadows = true;
}

void ULightComponentBase::SetIntensity(float NewIntensity)
{
	Intensity = NewIntensity;
	MarkRenderStateDirty();
}

void ULightComponentBase::SetLightColor(const FLinearColor& NewLightColor)
{
	LightColor = NewLightColor;
	MarkRenderStateDirty();
}

void ULightComponentBase::SetCastShadows(bool bNewValue)
{
	CastShadows = bNewValue;
	MarkRenderStateDirty();
}
